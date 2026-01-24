#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Space Vector PWM output
typedef struct {
    float duty_a;
    float duty_b;
    float duty_c;
} svpwm_output_t;

/**
 * @brief Initialize sine lookup table
 */
void foc_init(void);

/**
 * @brief Fast sine approximation using lookup table
 * @param angle Angle in radians
 * @return Sine value
 */
float foc_sin(float angle);

/**
 * @brief Fast cosine approximation using lookup table
 * @param angle Angle in radians
 * @return Cosine value
 */
float foc_cos(float angle);

/**
 * @brief Normalize angle to 0 to 2*PI range
 * @param angle Input angle in radians
 * @return Normalized angle
 */
float foc_normalize_angle(float angle);

/**
 * @brief Calculate electrical angle from mechanical angle
 * @param mechanical_angle Mechanical angle in radians
 * @param pole_pairs Number of pole pairs
 * @return Electrical angle in radians
 */
float foc_electrical_angle(float mechanical_angle, uint8_t pole_pairs);

/**
 * @brief Inverse Park transform (d-q to alpha-beta)
 * @param vd D-axis voltage
 * @param vq Q-axis voltage
 * @param angle_el Electrical angle
 * @param v_alpha Pointer to store alpha voltage
 * @param v_beta Pointer to store beta voltage
 */
void foc_inverse_park(float vd, float vq, float angle_el, float *v_alpha, float *v_beta);

/**
 * @brief Inverse Clarke transform (alpha-beta to a-b-c)
 * @param v_alpha Alpha voltage
 * @param v_beta Beta voltage
 * @param va Pointer to store phase A voltage
 * @param vb Pointer to store phase B voltage
 * @param vc Pointer to store phase C voltage
 */
void foc_inverse_clarke(float v_alpha, float v_beta, float *va, float *vb, float *vc);

/**
 * @brief Space Vector PWM modulation
 * @param vq Q-axis voltage (torque producing)
 * @param vd D-axis voltage (field weakening, usually 0)
 * @param angle_el Electrical angle in radians
 * @param voltage_limit Maximum voltage
 * @param output Pointer to store PWM duty cycles
 */
void foc_svpwm(float vq, float vd, float angle_el, float voltage_limit, svpwm_output_t *output);

/**
 * @brief Sinusoidal PWM modulation (simpler alternative to SVPWM)
 * @param vq Q-axis voltage
 * @param vd D-axis voltage
 * @param angle_el Electrical angle
 * @param voltage_limit Maximum voltage
 * @param output Pointer to store PWM duty cycles
 */
void foc_sinusoidal_pwm(float vq, float vd, float angle_el, float voltage_limit, svpwm_output_t *output);

#ifdef __cplusplus
}
#endif
