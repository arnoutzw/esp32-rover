# ESP32 Rover Firmware - AI Assistant Rules

This document defines the behavioral rules and standards that AI assistants MUST follow when working on this project.

---

## Table of Contents

1. [Context Consultation Rule](#context-consultation-rule)
2. [Plan Management](#plan-management)
3. [Git Workflow](#git-workflow)
   - [Branching Strategy](#branching-strategy)
   - [Commit Before Build Rule](#commit-before-build-rule)
   - [Clean Build Rule](#clean-build-rule)
   - [Push Immediately Rule](#push-immediately-rule)
4. [Build and Flash](#build-and-flash)
   - [Build Script Usage](#build-script-usage)
   - [Clean Build Rule](#clean-build-rule)
   - [Flash Verification](#flash-verification)
   - [Build Hash Verification Rule](#build-hash-verification-rule)
5. [Documentation Requirements](#documentation-requirements)
   - [Bug Documentation](#bug-documentation)
   - [Bug Report Analysis](#bug-report-analysis)
   - [Documentation Updates](#documentation-updates)
   - [Feature Request Processing](#feature-request-processing)
   - [Automated Report Processing Rule](#automated-report-processing-rule)
   - [Requirements-First Development Rule](#requirements-first-development-rule)
   - [Feature Implementation Verification Rule](#feature-implementation-verification-rule)
6. [Release Management](#release-management)
   - [Binary Archive Management](#binary-archive-management)
   - [CI/CD Pipeline](#cicd-pipeline)
   - [Release Process](#release-process)
7. [Code Quality](#code-quality)
   - [Static Analysis](#static-analysis)
   - [Code Coverage](#code-coverage)
8. [Security](#security)
   - [OTA Security](#ota-security)
   - [OTA Rollback Support](#ota-rollback-support)
9. [Embedded Coding Standards](#embedded-coding-standards)
   - [Critical Priority Rules](#critical-priority-rules)
   - [High Priority Rules](#high-priority-rules)
   - [Medium Priority Rules](#medium-priority-rules)

---

## Context Consultation Rule

**ALWAYS consult `CLAUDE/context.md` to understand user prompts and enhance reasoning.**

The `context.md` file serves as the project's mnemonic - a comprehensive memory of the entire project that helps AI assistants:

1. **Understand user prompts**: When a user mentions components, files, APIs, or concepts, use `context.md` to quickly identify what they're referring to and where to find relevant code.

2. **Enhance reasoning**: Before investigating or implementing changes, consult `context.md` to understand:
   - Project structure and where files are located
   - Component architecture and dependencies
   - Hardware constraints (ESP32-CAM vs TTGO differences)
   - Memory budgets and optimization requirements
   - API contracts and data structures

3. **Find relevant information faster**: Use the table of contents and sections to quickly locate:
   - Pin maps when dealing with GPIO issues
   - Status JSON structure when modifying REST API
   - Test organization when adding/modifying tests
   - Common commands when needing to build, flash, or debug

4. **Maintain consistency**: Reference existing patterns documented in `context.md` to ensure new code follows established conventions.

**When to consult `context.md`:**
- At the start of any task to understand context
- When user mentions unfamiliar terms or components
- Before modifying files to understand their role in the architecture
- When debugging to understand system behavior
- When the user's prompt is ambiguous - use context to infer intent

**This rule is foundational** - following it improves the quality and accuracy of all other work on this project.

---

## Plan Management

**All implementation plans MUST be stored in `CLAUDE/plans/` directory.**

When creating plans for features, refactoring, or multi-step implementations:

1. **Create a plan file** in `CLAUDE/plans/` with a descriptive name (e.g., `wifi-retry-implementation.md`, `memory-optimization-plan.md`)

2. **Plan file structure:**
   ```markdown
   # Plan: [Descriptive Title]

   ## Overview
   Brief description of what this plan accomplishes.

   ## Priority & Rationale
   - **Priority**: Critical / High / Medium / Low
   - **Why this priority**: [Argumentation for the priority level]
   - **Dependencies**: [Any prerequisites or blocking items]

   ## Implementation Steps
   Ordered list of concrete steps with clear acceptance criteria.

   ## Files to Modify
   Table of files and the changes needed.

   ## Verification
   How to verify the implementation is complete and correct.
   ```

3. **Priority levels with argumentation:**
   - **Critical**: Safety issues, data loss risks, or complete feature breakage. *Must include justification.*
   - **High**: Significant functionality impact or blocking other work. *Explain what is blocked.*
   - **Medium**: Improvements that enhance quality but aren't urgent. *Describe the benefit.*
   - **Low**: Nice-to-have enhancements. *Note why it can wait.*

4. **Always prioritize existing plans**: Before starting new work, check `CLAUDE/plans/` for incomplete plans and prioritize them based on their documented priority and rationale.

5. **Update plan status**: Mark plans as complete or archive them when finished.

**Why this matters:**
- Plans persist across sessions and context resets
- Prioritization with argumentation ensures informed decision-making
- Clear documentation prevents duplicate work
- Progress can be tracked and resumed

---

## Git Workflow

### Branching Strategy

**All development work happens on the `develop` branch. NEVER commit directly to `main`.**

**Strict Rules:**
- Work on `develop` branch for ALL changes (code, docs, config, everything)
- Commit frequently to `develop` with descriptive messages
- **NEVER commit directly to `main`** - main only receives merges from develop
- **NEVER checkout main to make changes** - only for merging/tagging releases
- Only merge to `main` and create a version tag when the user explicitly requests a release

**Workflow:**
```bash
# Normal development - ALWAYS commit AND push together
git checkout develop
# ... make changes ...
git add -A && git commit -m "Description of changes" && git push origin develop

# When user requests a release to main:
git checkout main
git merge develop --no-ff -m "Release vX.X.X"
git tag -a vX.X.X -m "Release description"
git push origin main --tags  # Push commits AND tags immediately
git checkout develop
git push origin develop  # Ensure develop is also pushed
```

**Why never commit to main directly:**
- `main` should only contain tagged releases
- Direct commits to main cause divergence between branches
- Merging main back to develop creates confusing history
- All changes must be tested on develop first

### Commit Before Build Rule

**NEVER attempt a build with uncommitted changes. Always commit first.**

Before running ANY build command (`./scripts/build.sh`, `idf.py build`, or similar):
1. Stage all changes: `git add -A`
2. Commit with a descriptive message: `git commit -m "Description"`
3. Push to origin: `git push origin develop`
4. Only then run the build command

**This ensures:**
- The git commit hash embedded in the firmware matches the actual source code
- Build fingerprint verification works correctly after flashing
- No "dirty" builds that cannot be reproduced from git history
- Every flashed firmware can be traced to an exact commit

**Workflow example:**
```bash
# After making code changes, ALWAYS do this sequence:
git add -A
git commit -m "Add WiFi retry logic for robust STA connection"
git push origin develop
./scripts/build.sh ttgo
```

### Clean Build Rule

**All builds and flashes are automatically enforced to use clean git state.**

The build script (`./scripts/build.sh`) now automatically:
1. Checks for uncommitted changes before building
2. Verifies all commits are pushed to origin
3. Blocks the build if the git state is dirty

**If you attempt to build with uncommitted or unpushed changes, you will see:**
```
Error: You have uncommitted changes

Uncommitted changes:
 M firmware/components/web_server/web_server.c

Please commit your changes before building:
  git add -A
  git commit -m "Your commit message"
  git push origin develop
```

**This enforcement ensures:**
- Every build is traceable to a specific commit that exists in the remote repository
- Build fingerprint in firmware always matches a known, pushed state
- No accidental "works on my machine" issues from uncommitted changes
- Firmware versions can be reliably reproduced from git history
- No more "dirty" builds with untraceable modifications

### Push Immediately Rule

**Every `git commit` MUST be immediately followed by `git push`.**

This applies to:
- All code changes (firmware, scripts, tests)
- All documentation updates (markdown files, comments)
- All configuration changes
- Commits on any branch (develop, feature branches)

**Why push immediately:**
- Changes are synced to remote immediately
- CI/CD pipelines are triggered without delay
- Team members have access to latest code
- Release workflows execute automatically
- No risk of losing work if local machine fails
- Documentation updates are immediately visible to all users

---

## Build and Flash

### Build Script Usage

**ALWAYS use `./scripts/build.sh` for builds, never `idf.py build` directly.**

The build script:
- Archives the binary with git metadata (commit hash, timestamp)
- Updates `latest.bin` symlink used by OTA script
- Enforces clean git state
- Generates `config_generated.h` automatically

### Clean Build Rule

**ALWAYS perform a clean build before flashing to prevent stale build artifacts.**

Incremental builds can retain stale `build_info` with old git hashes even when source code has changed. To prevent flashing firmware with incorrect version information:

```bash
# ALWAYS use this pattern for builds intended for flashing:
./scripts/build.sh <target> clean && ./scripts/build.sh <target>
```

**Why clean builds are mandatory:**
- The `build_info` component caches git hash at compile time
- Incremental builds may not regenerate `build_info.h` if only non-header files changed
- This causes the binary to report an old commit hash while containing new code
- Debugging becomes impossible when reported version doesn't match actual code

**Observed failure mode:**
1. Commit changes (e.g., `77b4f74`)
2. Run incremental build: `./scripts/build.sh ttgo`
3. Binary filename shows `77b4f74` but embedded hash is still `b32a316`
4. OTA succeeds but device reports old version
5. User thinks fix didn't work, wastes time debugging

**Clean build adds ~20 seconds but prevents hours of confusion.**

### Flash Verification

**After every OTA flash, verify the firmware fingerprint matches:**

```bash
# Get the expected commit hash from the build
git rev-parse --short HEAD  # e.g., c184d71

# Verify the device is running the correct firmware
curl -s http://<device-ip>/status | jq '.diag.buildFingerprint'
# Should output: "c184d71"
```

**If fingerprints don't match:**
1. You may have flashed an old archived binary (check `binaries/<target>/latest.bin` symlink)
2. The device may have rolled back to a previous OTA partition
3. Rebuild with `./scripts/build.sh <target>` to create a fresh archived binary

### Build Hash Verification Rule

**After every build, verify the binary contains the expected git hash before flashing.**

Before proceeding with OTA flash, always verify the built binary contains the correct commit hash:

```bash
# Get current git commit hash
EXPECTED_HASH=$(git rev-parse --short=7 HEAD)

# Verify hash is embedded in the binary
strings binaries/<target>/latest.bin | grep "$EXPECTED_HASH"

# Should output something like:
# v2.0.3-6-g77b4f74
# 77b4f74
```

**Verification steps (mandatory before OTA):**
1. Note the current git commit hash after committing
2. Run clean build: `./scripts/build.sh <target> clean && ./scripts/build.sh <target>`
3. Verify the binary filename contains the expected hash
4. Verify the hash is embedded in the binary using `strings` command
5. Only proceed with OTA if hashes match

**If hashes don't match:**
- **DO NOT proceed with OTA flash** - flag a warning to user
- Investigate why clean build didn't produce correct hash
- Never flash a binary with mismatched hash

**Complete build-flash workflow:**
```bash
# 1. Commit and push changes
git add -A && git commit -m "description" && git push

# 2. Note expected hash
EXPECTED=$(git rev-parse --short=7 HEAD)
echo "Expected hash: $EXPECTED"

# 3. Clean build (ALWAYS clean to prevent stale hash)
./scripts/build.sh ttgo clean && ./scripts/build.sh ttgo

# 4. Verify hash in binary
strings binaries/ttgo/latest.bin | grep "$EXPECTED"
# Must see the hash - if not, DO NOT FLASH

# 5. Flash via OTA
OTA_PASSWORD=<password> ./scripts/ota.sh ttgo

# 6. Verify device is running correct firmware
sleep 5 && curl -s http://ttgo-rover.local/status | jq '.diag.buildFingerprint'
# Must match expected hash
```

---

## Documentation Requirements

### Bug Documentation

**For every investigated bug with a confirmed fix, add a new entry to `docs/implementation/DEVELOPMENT_LESSONS.md`.**

Each entry should include:
1. **Symptom**: What the user observed (error message, behavior)
2. **Root Cause**: The underlying technical issue
3. **Investigation Process**: How the bug was diagnosed
4. **Solution**: The fix applied
5. **Lesson Learned**: Key takeaway for future development

This ensures knowledge is preserved and similar issues can be avoided or quickly diagnosed in the future.

### Bug Report Analysis

**For every analyzed bug report, create both an RCA (Root Cause Analysis) and a FIX_PLAN document.**

Bug reports are stored in `docs/bugreport/v{VERSION}/{HASH}/` where:
- `{VERSION}` is the firmware version (e.g., `v2.0.1`)
- `{HASH}` is the 7-character git commit hash (e.g., `e4b11fc`)

**Required documents for each analyzed bug:**

1. **RCA (Root Cause Analysis)** - `RCA_v{VERSION}_{HASH}.md`
   ```markdown
   # Root Cause Analysis: [Bug Title]

   ## Summary
   Brief description of the bug and its impact.

   ## Symptoms
   - What the user observed
   - Error messages, logs, or unexpected behavior

   ## Environment
   - Firmware version and build hash
   - Target hardware (TTGO/ESP32-CAM)
   - Configuration settings relevant to the bug

   ## Investigation
   Step-by-step analysis of how the root cause was identified.

   ## Root Cause
   Technical explanation of why the bug occurred.

   ## Contributing Factors
   Any secondary issues that enabled or worsened the bug.

   ## Impact Assessment
   - Severity: Critical / High / Medium / Low
   - Affected functionality
   - Risk of recurrence
   ```

2. **FIX_PLAN** - `FIX_PLAN_v{VERSION}_{HASH}.md`
   ```markdown
   # Fix Plan: [Bug Title]

   ## Overview
   Brief description of the fix approach.

   ## Related RCA
   Link to the RCA document.

   ## Proposed Fix
   Detailed description of the code changes.

   ## Files to Modify
   | File | Change |
   |------|--------|
   | path/to/file.c | Description of change |

   ## Implementation Steps
   1. Step one
   2. Step two

   ## Testing Plan
   How to verify the fix works.

   ## Rollback Plan
   How to revert if the fix causes issues.
   ```

**Workflow:**
1. User reports a bug or issue is discovered
2. Create the version/hash folder in `docs/bugreport/`
3. Create initial bug report document
4. Analyze and create `RCA_*.md`
5. Design fix and create `FIX_PLAN_*.md`
6. Implement fix following the plan
7. Update RCA with "Resolution" section after fix is verified

### Documentation Updates

**When making changes to firmware features, APIs, or configuration, update the relevant documentation.**

Required updates for different change types:

| Change Type | Update Required |
|-------------|-----------------|
| New feature or API change | `CHANGELOG.md` (under [Unreleased]), `CLAUDE/context.md` Technical Deep Dive section |
| Configuration change | `CHANGELOG.md`, `config/rover_config.yaml` comments, `CLAUDE/context.md` |
| Bug fix | `CHANGELOG.md`, `docs/implementation/DEVELOPMENT_LESSONS.md` |
| Status JSON change | `CHANGELOG.md`, Status JSON Structure section in `CLAUDE/context.md` |
| New endpoint | REST API Quick Reference section in `CLAUDE/context.md` |
| Build/tooling change | `CHANGELOG.md`, Common Commands section in `CLAUDE/context.md` |

**Documentation files to keep synchronized:**
- `CHANGELOG.md` - All user-visible changes
- `CLAUDE/context.md` - Technical reference for AI assistants and developers
- `docs/implementation/DEVELOPMENT_LESSONS.md` - Bug investigations and lessons learned
- `config/rover_config.yaml` - Inline comments for configuration options

### Feature Request Processing

**Check `docs/feature_requests/` for user feature requests and process them into formal requirements.**

Feature requests are written from a user's perspective and may include technical details. When you encounter a feature request:

1. **Analyze the request**: Understand what the user wants and why
2. **Extract requirements**: Identify specific, testable requirements from the request
3. **Create or update requirements document**: Add requirements to `docs/requirements/` in the appropriate specification (e.g., `firmware_requirements.md`, `electrical_requirements.md`)
4. **Present proposed changes**: Show the user the extracted requirements before committing
5. **After approval**: Commit the requirements and optionally archive/delete the feature request

**Requirements format:**

| ID | Priority | Description | Verification |
|----|----------|-------------|--------------|
| REQ-XX | High/Medium/Low | Clear, testable requirement | How to verify |

**Processing workflow:**
```markdown
1. Read feature request from docs/feature_requests/
2. Analyze and extract requirements
3. Draft requirements in appropriate docs/requirements/*.md file
4. Present to user: "I've processed your feature request. Here are the proposed requirements:"
5. Wait for user approval before committing
6. After approval, commit changes and inform user
```

### Automated Report Processing Rule

**When user says "check reports" or similar, scan for new bug reports and feature requests and process them autonomously through the full development cycle.**

This rule defines an end-to-end autonomous workflow that processes user-filed reports without intervention.

**Trigger phrases:**
- "check reports"
- "check feature requests"
- "check bug reports"
- "process reports"
- Any similar request to scan the report directories

**Automated workflow for Feature Requests:**

When a new feature request is found in `docs/feature_requests/` with `Status: New`:

1. **Analyze the request**: Read and understand what the user wants
2. **Update requirements**: Add formal requirements to `docs/requirements/software_requirements.md`
   - Assign REQ-SW-XXX ID
   - Add acceptance criteria
   - Add to traceability matrix
   - Add validation test entry
3. **Implement the feature**: Write the code following the requirements
   - Reference REQ ID in code comments
   - Follow embedded coding standards
4. **Update implementation documentation**:
   - Update `CHANGELOG.md` under [Unreleased]
   - Update `CLAUDE/context.md` if APIs changed
5. **Commit and push**: `git add -A && git commit -m "feat(REQ-SW-XXX): description" && git push`
6. **Build**: `./scripts/build.sh ttgo clean && ./scripts/build.sh ttgo`
7. **OTA flash**: `./scripts/ota.sh ttgo ttgo-rover.local`
8. **Verify deployment**: Query `/status` endpoint, confirm build fingerprint matches
9. **Quick functional test**: Verify the feature works on hardware
10. **Update feature request status**: Change `Status: New` to `Status: Implemented (hash)`
11. **Report completion**: Summarize what was done

**Automated workflow for Bug Reports:**

When a new bug report is found in `docs/bugreport/` with `Status: Open`:

1. **Analyze the bug**: Understand symptoms and reproduction steps
2. **Create RCA document**: `RCA_vX.X.X_hash.md` following template
3. **Create FIX_PLAN document**: `FIX_PLAN_vX.X.X_hash.md` with implementation plan
4. **Implement the fix**: Apply the code changes
5. **Update documentation**:
   - Add entry to `docs/implementation/DEVELOPMENT_LESSONS.md`
   - Update `CHANGELOG.md` under [Unreleased]
6. **Commit and push**: `git add -A && git commit -m "fix: description" && git push`
7. **Build**: `./scripts/build.sh ttgo clean && ./scripts/build.sh ttgo`
8. **OTA flash**: `./scripts/ota.sh ttgo ttgo-rover.local`
9. **Verify deployment**: Confirm build fingerprint matches
10. **Test the fix**: Verify bug is resolved on hardware
11. **Update bug report status**: Change `Status: Open` to `Status: Fixed (hash)`
12. **Update RCA**: Add Resolution section
13. **Report completion**: Summarize investigation and fix

**Important notes:**
- Process ALL new reports found, not just one
- If multiple reports exist, process them in order (bugs before features)
- If a step fails, stop and report the failure to user
- Never skip the build/flash/verify cycle
- Always update the report status after completion

**Example session:**
```
User: check reports

AI: Scanning docs/feature_requests/ and docs/bugreport/ for new reports...

Found 1 new feature request:
- Feature_Add_dark_mode_to_web_UI_20260125.md (Status: New)

Processing feature request: Add dark mode to web UI

Step 1: Adding requirement REQ-SW-036 to software_requirements.md...
Step 2: Implementing dark mode CSS variables in web_ui.c...
Step 3: Adding theme toggle button to settings panel...
Step 4: Updating CHANGELOG.md...
Step 5: Committing: "feat(REQ-SW-036): implement dark mode for web UI"
Step 6: Building for ttgo target...
Step 7: Flashing via OTA to ttgo-rover.local...
Step 8: Verifying deployment - build fingerprint: abc1234 ✓
Step 9: Quick test - dark mode toggle visible and functional ✓
Step 10: Updating feature request status to "Implemented (abc1234)"

✓ All reports processed successfully.
```

### Requirements-First Development Rule

**NEVER implement code before updating the requirements documentation. Requirements MUST be documented and approved BEFORE any implementation begins.**

This is a fundamental principle of proper software engineering. The correct workflow is:

1. **Requirements Phase** (FIRST):
   - Analyze the feature request or user need
   - Create/update formal requirements in `docs/requirements/software_requirements.md`
   - Assign a requirement ID (e.g., REQ-SW-032)
   - Define acceptance criteria
   - Add to traceability matrix
   - Add validation test entry
   - Commit the requirements documentation
   - Present to user for approval

2. **Implementation Phase** (SECOND - only after requirements are approved):
   - Implement the code according to the documented requirements
   - Reference the requirement ID in code comments (e.g., `// REQ-SW-032: ...`)
   - Test against the acceptance criteria
   - Commit the implementation

3. **Update Feature Request** (LAST):
   - Mark feature request as implemented
   - Link to the requirement ID

**Why requirements-first matters:**
- Ensures clear understanding of what needs to be built before building it
- Provides traceable acceptance criteria for testing
- Prevents scope creep and gold-plating
- Creates audit trail for changes
- Enables proper planning and estimation
- Separates "what" (requirements) from "how" (implementation)

**This rule is NON-NEGOTIABLE.** If you catch yourself writing code before the requirement is documented, STOP and document the requirement first.

**Example of correct workflow:**
```
User: "Add a live velocity chart to the web UI"

Step 1: Update docs/requirements/software_requirements.md
  - Add REQ-SW-032 with acceptance criteria
  - Add to traceability matrix
  - Add validation test VT-F-024
  - Commit: "docs: add REQ-SW-032 live telemetry chart requirement"

Step 2: Present requirement to user for approval

Step 3: After approval, implement the code
  - Add Chart.js to web_ui.c
  - Add steering history to web_server.c
  - Reference REQ-SW-032 in comments
  - Commit: "feat(REQ-SW-032): implement live telemetry chart"

Step 4: Update feature request status
  - Mark as implemented with REQ-SW-032 reference
```

### Feature Implementation Verification Rule

**After implementing any feature request, ALWAYS verify the implementation by building, flashing via OTA, and confirming the build hash matches.**

This ensures the feature is actually deployed and working on real hardware, not just committed to the repository.

**Mandatory verification steps after feature implementation:**

1. **Clean Build**:
   ```bash
   ./scripts/build.sh <target> clean && ./scripts/build.sh <target>
   ```

2. **Verify Build Hash**:
   - Note the commit hash from build output (e.g., `958b259`)
   - Verify it matches the expected commit: `git log --oneline -1`

3. **Flash via OTA**:
   ```bash
   ./scripts/ota.sh <target> <device-ip-or-hostname>
   ```
   Example: `./scripts/ota.sh ttgo ttgo-rover.local`

4. **Verify Deployment**:
   - Query the device's `/status` endpoint to confirm the build fingerprint
   - The `diag.buildFingerprint` field MUST match the commit hash from step 2
   ```bash
   curl http://<device>/status | jq '.diag.buildFingerprint'
   ```
   Or use the bug report tool to fetch build info:
   ```bash
   python scripts/file_report.py --bug
   # Click "Fetch Build Info" button
   ```

5. **Functional Verification**:
   - Test the implemented feature on actual hardware
   - Verify acceptance criteria from the requirement are met
   - Document any issues found

**Why this rule exists:**
- Ensures code actually runs on hardware, not just compiles
- Catches deployment issues (OTA failures, boot loops, etc.)
- Verifies the correct version is running (no stale firmware)
- Confirms feature works in real environment, not just in theory
- Prevents "it works on my machine" syndrome

**This verification is MANDATORY for all feature implementations.** Do not consider a feature complete until it has been flashed and verified on hardware.

**Example workflow:**
```bash
# After committing feature implementation
git log --oneline -1
# Output: 958b259 feat(REQ-SW-032): implement live telemetry chart

# Clean build
./scripts/build.sh ttgo clean && ./scripts/build.sh ttgo
# Verify output shows: 958b259

# Flash via OTA
./scripts/ota.sh ttgo ttgo-rover.local

# Verify deployment
curl -s http://ttgo-rover.local/status | jq '.diag.buildFingerprint'
# Output: "958b259" ← Must match!

# Test the feature
# Open http://ttgo-rover.local in browser
# Verify steering chart is visible and updating
```

---

## Release Management

### Binary Archive Management

**Automatically archive all built binaries and manage target switching cleanly.**

Built binaries are automatically archived in the `binaries/` directory with the following structure:
```
binaries/
├── esp32cam/          # ESP32-CAM binaries
│   ├── latest.bin → esp32-rover_esp32cam_YYYYMMDD_HHMMSS_commit.bin
│   ├── esp32-rover_esp32cam_YYYYMMDD_HHMMSS_commit.bin
│   ├── esp32-rover_esp32cam_YYYYMMDD_HHMMSS_commit.txt  # Metadata
│   └── ... (other builds)
└── ttgo/              # TTGO binaries
    └── (same structure as esp32cam)
```

**Key Features:**
1. **Automatic Archiving**: Every build automatically archives the binary with timestamp, commit hash, and branch info
2. **Latest Symlink**: `latest.bin` always points to the most recent build for that target
3. **Metadata**: Each binary has a `.txt` metadata file with build info
4. **Clean Target Switching**: When switching targets (e.g., esp32cam → ttgo), a clean build is automatically performed to prevent configuration conflicts

**Using Built Binaries:**

Flash the latest build (default):
```bash
./scripts/ota.sh esp32cam                    # Uses latest.bin symlink
```

List available binaries:
```bash
./scripts/ota.sh esp32cam --list-binaries
```

Flash a specific archived binary:
```bash
./scripts/ota.sh esp32cam --binary-file esp32-rover_esp32cam_20240115_143022_abc1234.bin
./scripts/ota.sh esp32cam --binary-file /absolute/path/to/binary.bin  # Absolute paths also work
```

Flash to specific IP with archived binary:
```bash
./scripts/ota.sh esp32cam 192.168.2.88 --binary-file esp32-rover_esp32cam_20240115_143022_abc1234.bin
```

**Target Switching Behavior:**

When you run `./scripts/build.sh` with a different target than the previous build:
1. The build system detects the target change
2. Automatically performs `idf.py clean` to remove old build artifacts
3. Removes the old `sdkconfig` to force regeneration with new target settings
4. Builds cleanly for the new target

This prevents configuration conflicts that could occur when switching between esp32cam and ttgo builds.

### CI/CD Pipeline

**Automated testing and builds via GitHub Actions.**

The project uses GitHub Actions for continuous integration:
- **Lint job**: Runs shellcheck on scripts and cppcheck on C code
- **Test job**: Runs unit tests on every push
- **Coverage job**: Generates code coverage reports
- **Build jobs**: Builds firmware for both targets (esp32cam and ttgo)

Workflow files:
- `.github/workflows/ci.yml` - Main CI pipeline (push/PR)
- `.github/workflows/release.yml` - Automated releases on tags

### Release Process

**Use the release script to create tagged releases.**

```bash
# Create a patch release (bug fixes)
./scripts/release.sh patch "Fixed OTA stability issue"

# Create a minor release (new features)
./scripts/release.sh minor "Added MQTT telemetry support"

# Create a major release (breaking changes)
./scripts/release.sh major "Complete architecture rewrite"

# Dry run to preview changes
./scripts/release.sh patch --dry-run
```

The release script:
1. Calculates the new version number
2. Generates changelog from commits
3. Builds both targets
4. Creates release artifacts in `releases/vX.X.X/`
5. Merges develop to main and creates a git tag
6. Pushes to origin

### CHANGELOG Update Rule

**CHANGELOG.md MUST be updated for every release.**

The CHANGELOG follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) format:

```markdown
## [X.X.X] - YYYY-MM-DD

### Added
- New features

### Changed
- Changes to existing functionality

### Fixed
- Bug fixes

### Removed
- Removed features

### Security
- Security fixes

### Documentation
- Documentation updates
```

**When to update:**
1. Before creating a release tag, add a new version section
2. Move items from `[Unreleased]` to the new version section
3. Include commit hashes for significant fixes (e.g., "Fixed battery flicker (5074ad7)")

**What to include:**
- All user-visible changes (features, fixes, improvements)
- Reference requirement IDs where applicable (e.g., "REQ-SW-035")
- Brief but clear descriptions of what changed and why

**Workflow:**
```bash
# 1. Update CHANGELOG.md with new version section
# 2. Commit the CHANGELOG update
git add CHANGELOG.md && git commit -m "docs: update CHANGELOG for vX.X.X"
# 3. Then create the tag and merge to main
```

---

## Code Quality

### Static Analysis

**Static analysis and linting are available via lint.sh.**

```bash
# Run all linting
./scripts/lint.sh

# Shell scripts only
./scripts/lint.sh --shell-only

# C code only
./scripts/lint.sh --c-only

# Strict mode (treat warnings as errors)
./scripts/lint.sh --strict
```

Requirements:
- `shellcheck` - Install via `brew install shellcheck` or `apt install shellcheck`
- `cppcheck` - Install via `brew install cppcheck` or `apt install cppcheck`

### Code Coverage

**Unit tests support code coverage measurement.**

```bash
cd test

# Run tests with coverage
make coverage

# Generate HTML coverage report
make coverage-html
# Open test/coverage_report/index.html

# Clean coverage data
make clean
```

Requirements for HTML reports:
- `lcov` - Install via `brew install lcov` or `apt install lcov`

---

## Security

### OTA Security

**OTA password must be set via environment variable.**

The OTA flash script requires an explicit password - no default password is used:

```bash
# Set password and flash
export OTA_PASSWORD=your_secure_password
./scripts/ota.sh esp32cam

# Or inline
OTA_PASSWORD=your_password ./scripts/ota.sh esp32cam
```

Binary checksums are displayed before flashing and stored in archive metadata for verification.

### OTA Rollback Support

**Automatic rollback on boot failure is enabled for ESP32-CAM only.**

ESP32-CAM has `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` set, which means:
- If a new firmware fails to boot properly, the device automatically rolls back to the previous working firmware
- The firmware must call `esp_ota_mark_app_valid_cancel_rollback()` after successful initialization to confirm it's working
- This prevents bricking the device with a bad OTA update

**Note:** TTGO T-Display does not have OTA rollback enabled due to IRAM memory constraints (no PSRAM). Extra caution should be taken when OTA flashing TTGO devices.

---

## Embedded Coding Standards

These rules are based on MISRA C, BARR-C, CERT C, and IEC 62443 standards, adapted for ESP-IDF and FreeRTOS embedded development.

### Critical Priority Rules

#### 1. Error Handling Policy

**Never use `ESP_ERROR_CHECK()` for non-critical initialization** - it crashes the entire device on failure.

```c
// BAD - crashes device if wifi init fails
ESP_ERROR_CHECK(esp_wifi_init(&cfg));

// GOOD - handles error gracefully
esp_err_t ret = esp_wifi_init(&cfg);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
    return ret;
}
```

**Rules:**
- Use `ESP_ERROR_CHECK()` only for truly fatal errors during startup (e.g., NVS init failure)
- Always check return values from `esp_*` functions
- Log all errors with context using `esp_err_to_name()`
- Propagate errors up the call stack - don't silently continue

#### 2. Memory Management Rules

**Always check allocation returns for NULL before use.**

```c
// BAD - no NULL check
TaskStatus_t *task_array = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
task_array[0].xHandle = NULL;  // Crash if allocation failed

// GOOD - check before use
TaskStatus_t *task_array = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
if (task_array == NULL) {
    ESP_LOGE(TAG, "Failed to allocate task array");
    return ESP_ERR_NO_MEM;
}
```

**Rules:**
- Check `malloc()`, `pvPortMalloc()`, `heap_caps_malloc()` returns for NULL
- Pair every allocation with deallocation in error paths (no leaks)
- Prefer static allocation for fixed-size buffers
- Document memory ownership in function comments
- Use `heap_caps_malloc(MALLOC_CAP_INTERNAL)` for DMA buffers

#### 3. Input Validation Rules

**Validate all external inputs at system boundaries.**

```c
// BAD - trusts input blindly
int speed = atoi(req->uri_query["speed"]);
motor_set_speed(speed);

// GOOD - validate bounds
int speed = atoi(speed_str);
if (speed < -100 || speed > 100) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Speed must be -100..100");
    return ESP_FAIL;
}
```

**Rules:**
- Validate all HTTP parameters (bounds, type, length)
- Validate config values at startup
- Check array indices before access
- Sanitize strings before use in format functions (prevent format string attacks)

#### 4. Watchdog Usage Policy

**Ensure all long-running operations yield to the watchdog.**

```c
// BAD - blocks watchdog for too long
while (processing) {
    process_chunk();  // May take > 5 seconds
}

// GOOD - yield periodically
while (processing) {
    process_chunk();
    vTaskDelay(pdMS_TO_TICKS(10));  // Yields to scheduler, feeds watchdog
}
```

**Rules:**
- Watchdog timeout is configured at `CFG_WATCHDOG_TIMEOUT_MS` (default 5000ms)
- All loops must call `vTaskDelay()` or `taskYIELD()` within timeout period
- Log warning if operation approaches watchdog timeout
- Never disable watchdog in production code

#### 5. Interrupt/ISR Rules

**All ISR-modified variables MUST be `volatile`.**

```c
// BAD - compiler may optimize away reads
static bool s_button_pressed = false;

// GOOD - ensures memory is re-read each time
static volatile bool s_button_pressed = false;
```

**Rules:**
- Use `volatile` for all variables modified in ISR and read elsewhere
- Use `IRAM_ATTR` for all ISR functions (keeps them in fast RAM)
- Keep ISR code minimal: set flag, notify task, return
- Never call blocking functions (`vTaskDelay`, `xSemaphoreTake`) from ISR
- Use `FromISR` variants: `xTaskNotifyFromISR()`, `xQueueSendFromISR()`

### High Priority Rules

#### 6. Naming Conventions

**Follow these naming patterns consistently:**

| Element | Convention | Example |
|---------|------------|---------|
| Static/file-scope variables | `s_` prefix | `s_battery_voltage` |
| Global variables | `g_` prefix (avoid globals) | `g_system_state` |
| Constants/defines | `UPPER_SNAKE_CASE` | `BATTERY_VOLTAGE_MIN` |
| Functions | `module_verb_noun()` | `wifi_start_ap()` |
| Types | `name_t` suffix | `rover_status_t` |
| Handlers/callbacks | `*_handler` or `*_cb` suffix | `button_isr_handler` |
| Boolean variables | `is_`, `has_`, `can_` prefix | `is_connected` |

#### 7. Function Size/Complexity Limits

**Keep functions small and focused.**

**Rules:**
- Maximum 100 lines of code per function
- Maximum 4 levels of nesting
- Single responsibility per function
- Extract state machines to separate functions
- If a function needs a comment block explaining sections, split it

```c
// BAD - 200+ line function with multiple responsibilities
void lcd_update_task(void *param) {
    // 50 lines of diagnostic mode handling
    // 50 lines of sleep mode handling
    // 100 lines of display update
}

// GOOD - extracted into focused functions
static void handle_diagnostic_mode(void) { /* 30 lines */ }
static void handle_sleep_mode(void) { /* 30 lines */ }
static void update_display(void) { /* 40 lines */ }

void lcd_update_task(void *param) {
    handle_diagnostic_mode();
    handle_sleep_mode();
    update_display();
}
```

#### 8. Return Value Checking

**All `esp_err_t` returns must be checked.**

```c
// BAD - ignoring return value
esp_wifi_get_mode(&wifi_mode);

// GOOD - explicit check
if (esp_wifi_get_mode(&wifi_mode) != ESP_OK) {
    ESP_LOGW(TAG, "Failed to get WiFi mode");
}

// GOOD - intentionally ignored (documented)
(void)esp_wifi_get_mode(&wifi_mode);  // Best-effort, non-critical
```

#### 9. Magic Number Prohibition

**All numeric literals must be named constants.**

```c
// BAD - magic numbers scattered in code
if (voltage < 3.0f) { /* low battery */ }
spi_clock = 40 * 1000 * 1000;
vTaskDelay(5000 / portTICK_PERIOD_MS);

// GOOD - named constants
#define BATTERY_VOLTAGE_EMPTY_V    3.0f
#define BATTERY_VOLTAGE_FULL_V     4.2f
#define LCD_SPI_CLOCK_HZ           (40 * 1000 * 1000)
#define PING_TIMEOUT_MS            5000
```

**Standard constants to define:**
- Battery thresholds: `BATTERY_VOLTAGE_EMPTY_V`, `BATTERY_VOLTAGE_FULL_V`
- Display offsets: `LCD_COL_OFFSET`, `LCD_ROW_OFFSET`
- Timing values: `*_TIMEOUT_MS`, `*_INTERVAL_MS`
- Hardware values: `*_CLOCK_HZ`, `*_DUTY_MAX`

#### 10. Comment Requirements

**Document the "why", not the "what".**

```c
// BAD - states the obvious
i++;  // Increment i

// GOOD - explains non-obvious reasoning
// Use 52-pixel offset because ST7789 has 240x320 panel but we use 135x240 window
#define LCD_COL_OFFSET 52
```

**Rules:**
- File header with purpose (not author/date - git tracks that)
- Function header for non-trivial functions (params, return, side effects)
- Inline comments only for non-obvious logic
- TODO comments must include issue/ticket reference

### Medium Priority Rules

#### 11. Global Variable Policy

**Minimize global state; protect shared data.**

```c
// BAD - unprotected global
float g_battery_voltage;  // Read by multiple tasks

// GOOD - protected by mutex, grouped in struct
typedef struct {
    float battery_voltage;
    SemaphoreHandle_t mutex;
} battery_state_t;
static battery_state_t s_battery = {0};
```

**Rules:**
- Prefer static file-scope over global scope
- Group related variables into structs
- Document thread-safety requirements
- Use mutexes for data shared across tasks

#### 12. Assertion Usage

**Use assertions for debug invariants only.**

```c
// GOOD - debug invariant check
configASSERT(task_handle != NULL);  // Programming error if NULL

// BAD - asserting on runtime condition
configASSERT(esp_wifi_connect() == ESP_OK);  // Don't assert on external failure
```

**Rules:**
- Use `configASSERT()` for catching programming errors
- Never assert on recoverable runtime conditions
- Assertions may be compiled out in release builds

#### 13. Logging Levels Policy

**Use appropriate log levels consistently.**

| Level | Use For | Example |
|-------|---------|---------|
| ERROR | Failures affecting functionality | `ESP_LOGE(TAG, "OTA failed: %s", err)` |
| WARN | Recoverable issues, degraded operation | `ESP_LOGW(TAG, "mDNS init failed, continuing")` |
| INFO | State changes, startup/shutdown | `ESP_LOGI(TAG, "WiFi connected to %s", ssid)` |
| DEBUG | Detailed tracing (disabled in release) | `ESP_LOGD(TAG, "Received command: speed=%d", speed)` |

**Rules:**
- Don't log in tight loops (use rate limiting)
- Include relevant context in log messages
- Use `#if CFG_DEBUG_*` for verbose debug logging

#### 14. Task Priority Policy

**Document priority rationale; avoid inversion.**

```c
// Document why this priority was chosen
#define MOTOR_TASK_PRIORITY    (tskIDLE_PRIORITY + 3)  // High: safety-critical timing
#define STATUS_TASK_PRIORITY   (tskIDLE_PRIORITY + 2)  // Medium: telemetry updates
#define LCD_TASK_PRIORITY      (tskIDLE_PRIORITY + 1)  // Low: display refresh
```

**Rules:**
- Higher priority = more time-critical
- Safety tasks (motor control) get highest priority
- Use priority inheritance mutexes to prevent inversion
- Document priority rationale in code

#### 15. Compiler Warning Policy

**Treat all warnings as errors.**

Build with `-Werror` enabled (default in this project). Never suppress warnings without documented justification.

```c
// BAD - suppressing without reason
#pragma GCC diagnostic ignored "-Wunused-variable"

// GOOD - if suppression is truly needed, document why
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
// ESP-IDF's httpd_req_to_sockfd returns int but we need to cast to socklen_t
// for setsockopt call - alignment is guaranteed by httpd implementation
socklen_t len = (socklen_t)httpd_req_to_sockfd(req);
#pragma GCC diagnostic pop
```

---

## Integration Tests

**Hardware integration tests are available for testing real devices.**

```bash
# Install test dependencies
pip install -r test/integration/requirements.txt

# Run integration tests against a device
export DEVICE_IP=esp32-rover.local
export OTA_PASSWORD=your_password
pytest test/integration/ -v
```

These tests verify:
- Device reachability and response times
- Status endpoint JSON responses
- OTA endpoint authentication
- Target-specific features (camera on ESP32-CAM, buttons on TTGO)

---

## Build Metrics

**Build duration and binary size are tracked automatically.**

After each build, metrics are:
- Displayed in the console
- Logged to `build_metrics.csv` for trend analysis

Format: `date,target,duration_s,size_bytes,commit`
