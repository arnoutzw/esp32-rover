#include "foc.h"
#include <math.h>
#include <stdbool.h>

#define PI 3.14159265358979323846f
#define TWO_PI (2.0f * PI)
#define SQRT3 1.7320508075688772935f
#define SQRT3_2 0.86602540378443864676f  // sqrt(3)/2
#define INV_SQRT3 0.57735026918962576451f // 1/sqrt(3)

// Sine lookup table (256 entries for one quadrant)
#define SINE_TABLE_SIZE 256
static float sine_table[SINE_TABLE_SIZE + 1];
static bool foc_initialized = false;

void foc_init(void)
{
    if (foc_initialized) return;

    // Generate sine lookup table for first quadrant
    for (int i = 0; i <= SINE_TABLE_SIZE; i++) {
        sine_table[i] = sinf((float)i * (PI / 2.0f) / SINE_TABLE_SIZE);
    }
    foc_initialized = true;
}

float foc_sin(float angle)
{
    // Normalize angle to 0-2*PI
    angle = foc_normalize_angle(angle);

    // Determine quadrant and map to first quadrant
    int quadrant = (int)(angle / (PI / 2.0f));
    float normalized = angle - quadrant * (PI / 2.0f);

    // Map to table index
    float index_f = normalized * (SINE_TABLE_SIZE * 2.0f / PI);
    int index = (int)index_f;
    float frac = index_f - index;

    // Clamp index
    if (index >= SINE_TABLE_SIZE) index = SINE_TABLE_SIZE - 1;

    // Linear interpolation
    float value = sine_table[index] + frac * (sine_table[index + 1] - sine_table[index]);

    // Apply quadrant sign
    switch (quadrant % 4) {
        case 0: return value;
        case 1: return sine_table[SINE_TABLE_SIZE] - value + sine_table[SINE_TABLE_SIZE - index];
        case 2: return -value;
        case 3: return -(sine_table[SINE_TABLE_SIZE] - value + sine_table[SINE_TABLE_SIZE - index]);
    }
    return 0;
}

float foc_cos(float angle)
{
    return foc_sin(angle + PI / 2.0f);
}

float foc_normalize_angle(float angle)
{
    float a = fmodf(angle, TWO_PI);
    return a >= 0 ? a : (a + TWO_PI);
}

float foc_electrical_angle(float mechanical_angle, uint8_t pole_pairs)
{
    return foc_normalize_angle(mechanical_angle * pole_pairs);
}

void foc_inverse_park(float vd, float vq, float angle_el, float *v_alpha, float *v_beta)
{
    float cos_a = foc_cos(angle_el);
    float sin_a = foc_sin(angle_el);

    *v_alpha = vd * cos_a - vq * sin_a;
    *v_beta = vd * sin_a + vq * cos_a;
}

void foc_inverse_clarke(float v_alpha, float v_beta, float *va, float *vb, float *vc)
{
    *va = v_alpha;
    *vb = -0.5f * v_alpha + SQRT3_2 * v_beta;
    *vc = -0.5f * v_alpha - SQRT3_2 * v_beta;
}

void foc_sinusoidal_pwm(float vq, float vd, float angle_el, float voltage_limit, svpwm_output_t *output)
{
    // Inverse Park transform
    float v_alpha, v_beta;
    foc_inverse_park(vd, vq, angle_el, &v_alpha, &v_beta);

    // Inverse Clarke transform
    float va, vb, vc;
    foc_inverse_clarke(v_alpha, v_beta, &va, &vb, &vc);

    // Normalize to duty cycle (0 to 1)
    // Center around 0.5 and scale by voltage limit
    float scale = 0.5f / voltage_limit;

    output->duty_a = 0.5f + va * scale;
    output->duty_b = 0.5f + vb * scale;
    output->duty_c = 0.5f + vc * scale;

    // Clamp values
    if (output->duty_a < 0) output->duty_a = 0;
    if (output->duty_a > 1) output->duty_a = 1;
    if (output->duty_b < 0) output->duty_b = 0;
    if (output->duty_b > 1) output->duty_b = 1;
    if (output->duty_c < 0) output->duty_c = 0;
    if (output->duty_c > 1) output->duty_c = 1;
}

void foc_svpwm(float vq, float vd, float angle_el, float voltage_limit, svpwm_output_t *output)
{
    // Inverse Park transform
    float v_alpha, v_beta;
    foc_inverse_park(vd, vq, angle_el, &v_alpha, &v_beta);

    // Determine sector (1-6)
    int sector;
    float angle = atan2f(v_beta, v_alpha);
    if (angle < 0) angle += TWO_PI;
    sector = (int)(angle / (PI / 3.0f)) + 1;
    if (sector > 6) sector = 6;

    // Calculate reference vector magnitude
    float v_ref = sqrtf(v_alpha * v_alpha + v_beta * v_beta);

    // Limit voltage
    if (v_ref > voltage_limit) {
        v_ref = voltage_limit;
        float scale = voltage_limit / sqrtf(v_alpha * v_alpha + v_beta * v_beta);
        v_alpha *= scale;
        v_beta *= scale;
    }

    // Calculate times T1, T2 for each sector
    float T1, T2, T0;
    float angle_sector = angle - (sector - 1) * (PI / 3.0f);

    T1 = SQRT3 * v_ref * foc_sin(PI / 3.0f - angle_sector) / voltage_limit;
    T2 = SQRT3 * v_ref * foc_sin(angle_sector) / voltage_limit;
    T0 = 1.0f - T1 - T2;

    if (T0 < 0) {
        // Over-modulation, scale T1 and T2
        float scale = 1.0f / (T1 + T2);
        T1 *= scale;
        T2 *= scale;
        T0 = 0;
    }

    // Calculate duty cycles based on sector
    float ta, tb, tc;
    float t0_half = T0 / 2.0f;

    switch (sector) {
        case 1:
            ta = T1 + T2 + t0_half;
            tb = T2 + t0_half;
            tc = t0_half;
            break;
        case 2:
            ta = T1 + t0_half;
            tb = T1 + T2 + t0_half;
            tc = t0_half;
            break;
        case 3:
            ta = t0_half;
            tb = T1 + T2 + t0_half;
            tc = T2 + t0_half;
            break;
        case 4:
            ta = t0_half;
            tb = T1 + t0_half;
            tc = T1 + T2 + t0_half;
            break;
        case 5:
            ta = T2 + t0_half;
            tb = t0_half;
            tc = T1 + T2 + t0_half;
            break;
        case 6:
        default:
            ta = T1 + T2 + t0_half;
            tb = t0_half;
            tc = T1 + t0_half;
            break;
    }

    output->duty_a = ta;
    output->duty_b = tb;
    output->duty_c = tc;

    // Clamp values
    if (output->duty_a > 1) output->duty_a = 1;
    if (output->duty_b > 1) output->duty_b = 1;
    if (output->duty_c > 1) output->duty_c = 1;
    if (output->duty_a < 0) output->duty_a = 0;
    if (output->duty_b < 0) output->duty_b = 0;
    if (output->duty_c < 0) output->duty_c = 0;
}
