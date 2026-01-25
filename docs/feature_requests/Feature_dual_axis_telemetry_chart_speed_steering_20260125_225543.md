# Feature Request: Dual-Axis Telemetry Chart (Speed + Steering)

| Field | Value |
|-------|-------|
| **Requested** | 2026-01-25 22:55:43 |
| **Status** | Implemented |
| **Requirement** | REQ-SW-033 |
| **Implementation** | Commit 2cc9899 |

## What I Want

Duplicate the live telemetry chart to also include speed (if 2 y-scales in the same plot with different color can be used) use the same plot.

## Why I Need It

Combined visualization of speed and steering enables correlation analysis between throttle and turning inputs, improving debugging and control tuning.

## How I Imagine It Working

- Single chart with dual Y-axes
- Speed data on left Y-axis with distinct color (orange/red)
- Steering data on right Y-axis with distinct color (blue)
- Both axes range from -100% to +100%
- Time window buttons (6s, 30s, 60s) apply to both series
- Legend shows both series with color coding

## Technical Notes

- Extend existing Chart.js implementation
- Add speedHistory ring buffer (600 samples)
- Add speedHistory array to /status JSON response
- Configure Chart.js for dual Y-axes

---

*Filed via feature request*
