# Feature Request: CRON job to write prompt to yourself every hour to check bug reports and feature requests directories

| Field | Value |
|-------|-------|
| **Requested** | 2026-01-26 00:13:41 |
| **Status** | Not Feasible |

## What I Want

I want you to monitor those directories to see if there is any work to be picked up

## Why I Need It

so you can continue when I am not at the pc

## How I Imagine It Working

script writes prompt to you as claude code

## Analysis

This feature is not feasible due to architectural limitations:

1. **Claude Code is session-based** - I only exist during active conversation sessions, not as a persistent background process
2. **No external API** - There's no mechanism for scripts to programmatically send messages to Claude Code and receive autonomous processing

### Current Workflow (Works Well)

The "Automated Report Processing Rule" in `CLAUDE/rules.md` already enables efficient batch processing:

```
User: "check reports"
Claude: [Scans directories, processes all new reports through full dev cycle]
```

### Recommended Alternatives

1. **Start sessions with "check reports"** - All accumulated reports will be processed
2. **macOS calendar reminder** - Set periodic reminder to check reports
3. **File reports remotely** - Use `file_report_web.py` via VPN from anywhere; reports queue up for next session

---

*Filed using file_report_web.py*
