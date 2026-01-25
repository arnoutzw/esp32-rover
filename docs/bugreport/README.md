# Bug Reports

Bug reports are organized by version and git commit hash for traceability.

## Directory Structure

```
bugreport/
├── v2.0.1/
│   └── e4b11fc/
│       ├── Bugreport TTGO.md
│       ├── FIX_PLAN_v2.0.1_e4b11fc.md
│       └── RCA_v2.0.1_e4b11fc.md
├── v2.0.2/
│   └── 94d854d/
│       └── (bug reports for this build)
└── README.md
```

## Naming Convention

- **Version folders**: `v{MAJOR}.{MINOR}.{PATCH}` (e.g., `v2.0.1`)
- **Hash folders**: First 7 characters of the git commit hash (e.g., `e4b11fc`)

## File Types

| File Type | Purpose |
|-----------|---------|
| `Bugreport *.md` | Initial bug report with symptoms and observations |
| `RCA_*.md` | Root Cause Analysis document |
| `FIX_PLAN_*.md` | Implementation plan for the fix |

## Creating a New Bug Report

1. Identify the firmware version and build hash from the device:
   ```bash
   curl -s http://<device-ip>/status | jq '.diag.buildVersion, .diag.buildFingerprint'
   ```

2. Create the folder structure:
   ```bash
   mkdir -p docs/bugreport/v{VERSION}/{HASH}
   ```

3. Create bug report files in that folder.

## Linking to Fixes

When a bug is fixed, reference the fixing commit in the bug report and update the status.
