# Feature Request: live plots of steering and speed in the webview page

| Field | Value |
|-------|-------|
| **Requested** | 2026-01-25 22:37:45 |
| **Status** | Implemented (2cc9899) |
| **Requirement** | REQ-SW-032, REQ-SW-033 |

## What I Want

Have a live plot of steering and speed in the webview page that shows telemetry over a variable time window (5, 10, 15 min).

## Why I Need It

Telemetry for investigation and monitoring rover behavior over time.

## How I Imagine It Working

Draw a dual-axis chart that updates live with new sensor data coming in - speed on left axis, steering on right axis.

## Technical Notes (Optional)

Implemented using Chart.js with dual Y-axes for speed (-100% to +100%) and steering (-45° to +45°).

---

*Filed using file_report.py*
