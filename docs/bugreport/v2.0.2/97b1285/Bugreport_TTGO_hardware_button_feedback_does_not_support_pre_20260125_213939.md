# Bug Report: TTGO hardware button feedback does not support pressing both buttons at the same time

## Build Information

| Field | Value |
|-------|-------|
| **Version** | v2.0.2 |
| **Git Hash** | 97b1285 |
| **Reported** | 2026-01-25 21:39:39 |
| **Status** | Fixed |
| **Fixed In** | v2.0.3+3 (b32a316) |

## Description

WHen pressing the hardware buttons on the front of the device I only see on or the other light up in the display (not both at once) although I am holding both pressed

I expect that both light up when both are pressed

## Steps to Reproduce

startup the device
press both hardware buttons
check on the lcd screen the response

## Investigation

See [RCA_v2.0.2_97b1285.md](RCA_v2.0.2_97b1285.md) for root cause analysis.

**Root Cause:** ISR-based button reading has a race condition when buttons are pressed simultaneously. The first button's edge triggers the ISR before the second button is fully pressed.

**Fix:** Added direct GPIO polling for display purposes (bypassing ISR-cached values) while keeping ISR for state machine edge detection.

**Fix Commit:** b32a316 - "fix: use direct GPIO reading for button display"

---

*Filed using file_bugreport.py*
