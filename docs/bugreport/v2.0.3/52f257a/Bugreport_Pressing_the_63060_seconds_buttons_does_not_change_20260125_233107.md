# Bug Report: Pressing the 6,30,60 seconds buttons does not change anything for the x-axis

## Build Information

| Field | Value |
|-------|-------|
| **Version** | v2.0.3 |
| **Git Hash** | 52f257a |
| **Reported** | 2026-01-25 23:31:07 |
| **Status** | Fixed (09ebfdd) |

## Description

Pressing the 6,30,60 seconds buttons does not change anything for the x-axis, the timescale should change accordingly to the button pressed

## Steps to Reproduce

go to webgui , press the time range button

## Investigation

### Root Cause

The `setChartWindow()` function in `web_ui.c:1249` was only updating the `chartWindowSeconds` variable and button CSS states, but not triggering a chart redraw. The chart only redraws when new telemetry data arrives via WebSocket.

### Fix

Added storage for current steering/speed history data and call `updateTelemetryChart()` when the time window selection changes:

1. Added `currentSteeringHistory` and `currentSpeedHistory` variables to store chart data
2. Modified `updateTelemetryChart()` to store incoming history data
3. Modified `setChartWindow()` to call `updateTelemetryChart()` with stored data after changing the time window

**Commit:** 09ebfdd

---

*Filed using file_report.py*
