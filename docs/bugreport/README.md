# Bug Reports

Bug reports are organized by version and git commit hash for traceability.

## Filing a Bug Report

Use the GUI tool to file bug reports:

```bash
# Launch the bug report tool
python scripts/file_bugreport.py

# Or pre-fill version and hash
python scripts/file_bugreport.py --version 2.0.3 --hash abc1234
```

The tool can also fetch build info directly from a connected device via the REST API.

## AI Investigation Process

When an AI assistant sees a new bug report in this folder, it will automatically:

1. **Investigate** the bug by analyzing code, logs, and symptoms
2. **Create RCA** (Root Cause Analysis) document explaining the issue
3. **Create FIX_PLAN** document with implementation steps
4. **Present findings** for user review before implementing fixes

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

**Recommended: Use the GUI tool**

```bash
python scripts/file_bugreport.py
```

**Manual method:**

1. Identify the firmware version and build hash from the device:
   ```bash
   curl -s http://<device-ip>/status | jq '.diag.buildVersion, .diag.buildFingerprint'
   ```

2. Create the folder structure:
   ```bash
   mkdir -p docs/bugreport/v{VERSION}/{HASH}
   ```

3. Create a bug report markdown file in that folder.

## Linking to Fixes

When a bug is fixed, reference the fixing commit in the bug report and update the status.
