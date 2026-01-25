#!/usr/bin/env python3
"""
ESP32 Rover - Bug Report & Feature Request Tool

A GUI application to file bug reports and feature requests with proper structure
for AI-assisted investigation and implementation.

Usage:
    python scripts/file_bugreport.py

    Or with the firmware version pre-filled (for bug reports):
    python scripts/file_bugreport.py --version 2.0.3 --hash abc1234

    Or directly open a specific form:
    python scripts/file_bugreport.py --bug
    python scripts/file_bugreport.py --feature
"""

import os
import sys
import re
import argparse
from datetime import datetime
from pathlib import Path

# Try to import tkinter
try:
    import tkinter as tk
    from tkinter import ttk, messagebox, scrolledtext
except ImportError:
    print("Error: tkinter is required. Install it with:")
    print("  macOS: brew install python-tk")
    print("  Ubuntu: sudo apt-get install python3-tk")
    print("  Windows: tkinter is included with Python")
    sys.exit(1)


class MainMenuApp:
    """Main menu with options to file bug report or feature request."""

    def __init__(self, root, initial_version="", initial_hash=""):
        self.root = root
        self.root.title("ESP32 Rover - Report Tool")
        self.root.geometry("400x300")
        self.root.minsize(350, 250)
        self.initial_version = initial_version
        self.initial_hash = initial_hash

        # Determine project root
        self.script_dir = Path(__file__).parent
        self.project_root = self.script_dir.parent

        self.create_widgets()

    def create_widgets(self):
        # Main frame with padding
        main_frame = ttk.Frame(self.root, padding="20")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Title
        title_label = ttk.Label(main_frame, text="ESP32 Rover",
                                font=('TkDefaultFont', 20, 'bold'))
        title_label.pack(pady=(0, 5))

        subtitle_label = ttk.Label(main_frame, text="Report & Request Tool",
                                   font=('TkDefaultFont', 12))
        subtitle_label.pack(pady=(0, 30))

        # Description
        desc_label = ttk.Label(main_frame,
                               text="Choose what you'd like to file:",
                               font=('TkDefaultFont', 10))
        desc_label.pack(pady=(0, 20))

        # Buttons frame
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X, pady=10)

        # Bug Report button
        bug_btn = ttk.Button(btn_frame, text="File Bug Report",
                             command=self.open_bug_report, width=25)
        bug_btn.pack(pady=10)

        bug_desc = ttk.Label(btn_frame,
                             text="Report a problem or unexpected behavior",
                             foreground="gray", font=('TkDefaultFont', 9))
        bug_desc.pack(pady=(0, 20))

        # Feature Request button
        feature_btn = ttk.Button(btn_frame, text="File Feature Request",
                                 command=self.open_feature_request, width=25)
        feature_btn.pack(pady=10)

        feature_desc = ttk.Label(btn_frame,
                                 text="Suggest a new feature or improvement",
                                 foreground="gray", font=('TkDefaultFont', 9))
        feature_desc.pack(pady=(0, 20))

        # Exit button
        exit_btn = ttk.Button(main_frame, text="Exit", command=self.root.quit, width=15)
        exit_btn.pack(pady=(20, 0))

    def open_bug_report(self):
        """Open the bug report form in a new window."""
        self.root.withdraw()  # Hide main menu
        bug_window = tk.Toplevel(self.root)
        bug_window.protocol("WM_DELETE_WINDOW", lambda: self.on_child_close(bug_window))
        BugReportApp(bug_window, self.initial_version, self.initial_hash,
                     on_close=lambda: self.on_child_close(bug_window))

    def open_feature_request(self):
        """Open the feature request form in a new window."""
        self.root.withdraw()  # Hide main menu
        feature_window = tk.Toplevel(self.root)
        feature_window.protocol("WM_DELETE_WINDOW", lambda: self.on_child_close(feature_window))
        FeatureRequestApp(feature_window,
                          on_close=lambda: self.on_child_close(feature_window))

    def on_child_close(self, child_window):
        """Handle child window closing."""
        child_window.destroy()
        self.root.deiconify()  # Show main menu again


class BugReportApp:
    """Bug report filing form."""

    def __init__(self, root, initial_version="", initial_hash="", on_close=None):
        self.root = root
        self.root.title("ESP32 Rover - Bug Report")
        self.root.geometry("700x650")
        self.root.minsize(500, 400)
        self.on_close = on_close

        # Determine project root
        self.script_dir = Path(__file__).parent
        self.project_root = self.script_dir.parent
        self.bugreport_dir = self.project_root / "docs" / "bugreport"

        self.create_widgets(initial_version, initial_hash)

    def create_widgets(self, initial_version, initial_hash):
        # Main frame with padding
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Title
        title_label = ttk.Label(main_frame, text="File a Bug Report",
                                font=('TkDefaultFont', 16, 'bold'))
        title_label.pack(pady=(0, 10))

        # Version and Hash frame
        info_frame = ttk.LabelFrame(main_frame, text="Build Information", padding="10")
        info_frame.pack(fill=tk.X, pady=(0, 10))

        # Version input
        version_frame = ttk.Frame(info_frame)
        version_frame.pack(fill=tk.X, pady=2)
        ttk.Label(version_frame, text="Version:", width=12).pack(side=tk.LEFT)
        self.version_var = tk.StringVar(value=initial_version)
        self.version_entry = ttk.Entry(version_frame, textvariable=self.version_var, width=20)
        self.version_entry.pack(side=tk.LEFT, padx=(0, 10))
        ttk.Label(version_frame, text="(e.g., 2.0.3)", foreground="gray").pack(side=tk.LEFT)

        # Hash input
        hash_frame = ttk.Frame(info_frame)
        hash_frame.pack(fill=tk.X, pady=2)
        ttk.Label(hash_frame, text="Git Hash:", width=12).pack(side=tk.LEFT)
        self.hash_var = tk.StringVar(value=initial_hash)
        self.hash_entry = ttk.Entry(hash_frame, textvariable=self.hash_var, width=20)
        self.hash_entry.pack(side=tk.LEFT, padx=(0, 10))
        ttk.Label(hash_frame, text="(7 chars, e.g., abc1234)", foreground="gray").pack(side=tk.LEFT)

        # Fetch from device button
        fetch_frame = ttk.Frame(info_frame)
        fetch_frame.pack(fill=tk.X, pady=(5, 0))
        ttk.Label(fetch_frame, text="Device IP:", width=12).pack(side=tk.LEFT)
        self.device_ip_var = tk.StringVar(value="ttgo-rover.local")
        self.device_ip_entry = ttk.Entry(fetch_frame, textvariable=self.device_ip_var, width=20)
        self.device_ip_entry.pack(side=tk.LEFT, padx=(0, 5))
        self.fetch_btn = ttk.Button(fetch_frame, text="Fetch Build Info", command=self.fetch_build_info)
        self.fetch_btn.pack(side=tk.LEFT)

        # Status label for fetch feedback (no popup)
        self.status_var = tk.StringVar(value="")
        self.status_label = ttk.Label(fetch_frame, textvariable=self.status_var,
                                       font=('TkDefaultFont', 9))
        self.status_label.pack(side=tk.LEFT, padx=(10, 0))

        # Bug Title
        title_frame = ttk.LabelFrame(main_frame, text="Bug Title", padding="10")
        title_frame.pack(fill=tk.X, pady=(0, 10))
        self.title_var = tk.StringVar()
        self.title_entry = ttk.Entry(title_frame, textvariable=self.title_var)
        self.title_entry.pack(fill=tk.X)

        # Bug Description
        desc_frame = ttk.LabelFrame(main_frame, text="Description (What happened? What did you expect?)",
                                    padding="10")
        desc_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        self.desc_text = scrolledtext.ScrolledText(desc_frame, wrap=tk.WORD, height=6)
        self.desc_text.pack(fill=tk.BOTH, expand=True)

        # Steps to Reproduce
        steps_frame = ttk.LabelFrame(main_frame, text="Steps to Reproduce (Optional)", padding="10")
        steps_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        self.steps_text = scrolledtext.ScrolledText(steps_frame, wrap=tk.WORD, height=4)
        self.steps_text.pack(fill=tk.BOTH, expand=True)

        # Additional Notes
        notes_frame = ttk.LabelFrame(main_frame, text="Additional Notes / Technical Details (Optional)",
                                     padding="10")
        notes_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        self.notes_text = scrolledtext.ScrolledText(notes_frame, wrap=tk.WORD, height=3)
        self.notes_text.pack(fill=tk.BOTH, expand=True)

        # Buttons
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X, pady=(10, 0))

        self.submit_btn = ttk.Button(btn_frame, text="Submit Bug Report", command=self.submit_report)
        self.submit_btn.pack(side=tk.RIGHT, padx=(5, 0))

        self.cancel_btn = ttk.Button(btn_frame, text="Back", command=self.go_back)
        self.cancel_btn.pack(side=tk.RIGHT)

    def go_back(self):
        """Go back to main menu."""
        if self.on_close:
            self.on_close()
        else:
            self.root.quit()

    def fetch_build_info(self):
        """Fetch build info from device via REST API and fill fields directly."""
        import urllib.request
        import json

        device_ip = self.device_ip_var.get().strip()
        if not device_ip:
            self.show_status("Please enter a device IP or hostname", error=True)
            return

        url = f"http://{device_ip}/status"
        self.fetch_btn.config(state=tk.DISABLED)
        self.show_status("Fetching build info...")
        self.root.update()

        try:
            with urllib.request.urlopen(url, timeout=5) as response:
                data = json.loads(response.read().decode())

            diag = data.get("diag", {})
            version = diag.get("buildVersion", "")
            fingerprint = diag.get("buildFingerprint", "")

            if version:
                version_match = re.match(r'(\d+\.\d+\.\d+)', version)
                if version_match:
                    self.version_var.set(version_match.group(1))

            if fingerprint:
                self.hash_var.set(fingerprint[:7])

            self.show_status(f"Fetched: v{self.version_var.get()} ({self.hash_var.get()})")

        except urllib.error.URLError as e:
            self.show_status(f"Connection error: {e.reason}", error=True)
        except json.JSONDecodeError:
            self.show_status("Invalid response from device", error=True)
        except Exception as e:
            self.show_status(f"Error: {e}", error=True)
        finally:
            self.fetch_btn.config(state=tk.NORMAL)

    def show_status(self, message, error=False):
        """Show status message in the status label."""
        self.status_var.set(message)
        self.status_label.config(foreground="red" if error else "green")

    def validate_inputs(self):
        """Validate all required inputs."""
        version = self.version_var.get().strip()
        git_hash = self.hash_var.get().strip()
        title = self.title_var.get().strip()
        description = self.desc_text.get("1.0", tk.END).strip()

        errors = []

        if not version:
            errors.append("Version is required")
        elif not re.match(r'^\d+\.\d+(\.\d+)?$', version):
            errors.append("Version must be in format X.Y or X.Y.Z (e.g., 2.0.3)")

        if not git_hash:
            errors.append("Git hash is required")
        elif not re.match(r'^[a-fA-F0-9]{7,40}$', git_hash):
            errors.append("Git hash must be 7-40 hexadecimal characters")

        if not title:
            errors.append("Bug title is required")

        if not description:
            errors.append("Bug description is required")

        return errors

    def submit_report(self):
        """Submit the bug report."""
        errors = self.validate_inputs()
        if errors:
            messagebox.showerror("Validation Error", "\n".join(errors))
            return

        version = self.version_var.get().strip()
        git_hash = self.hash_var.get().strip()[:7].lower()
        title = self.title_var.get().strip()
        description = self.desc_text.get("1.0", tk.END).strip()
        steps = self.steps_text.get("1.0", tk.END).strip()
        notes = self.notes_text.get("1.0", tk.END).strip()

        # Create directory structure
        version_dir = f"v{version}" if not version.startswith('v') else version
        report_dir = self.bugreport_dir / version_dir / git_hash
        report_dir.mkdir(parents=True, exist_ok=True)

        # Generate filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        safe_title = re.sub(r'[^\w\s-]', '', title)[:50].strip().replace(' ', '_')
        filename = f"Bugreport_{safe_title}_{timestamp}.md"
        filepath = report_dir / filename

        # Generate markdown content
        content = self.generate_markdown(version, git_hash, title, description, steps, notes)

        try:
            with open(filepath, 'w') as f:
                f.write(content)

            messagebox.showinfo("Success",
                f"Bug report saved!\n\n"
                f"Location:\n{filepath.relative_to(self.project_root)}\n\n"
                f"The AI assistant will investigate this bug report and create:\n"
                f"- RCA_{version_dir}_{git_hash}.md (Root Cause Analysis)\n"
                f"- FIX_PLAN_{version_dir}_{git_hash}.md (Fix Implementation Plan)")

            self.go_back()

        except Exception as e:
            messagebox.showerror("Error", f"Failed to save bug report:\n{e}")

    def generate_markdown(self, version, git_hash, title, description, steps, notes):
        """Generate markdown content for the bug report."""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        content = f"""# Bug Report: {title}

## Build Information

| Field | Value |
|-------|-------|
| **Version** | v{version} |
| **Git Hash** | {git_hash} |
| **Reported** | {timestamp} |
| **Status** | Open |

## Description

{description}

"""

        if steps:
            content += f"""## Steps to Reproduce

{steps}

"""

        if notes:
            content += f"""## Additional Notes

{notes}

"""

        content += """## Investigation

*This section will be filled by the AI assistant during investigation.*

---

*Filed using file_bugreport.py*
"""

        return content


class FeatureRequestApp:
    """Feature request filing form."""

    def __init__(self, root, on_close=None):
        self.root = root
        self.root.title("ESP32 Rover - Feature Request")
        self.root.geometry("700x700")
        self.root.minsize(500, 500)
        self.on_close = on_close

        # Determine project root
        self.script_dir = Path(__file__).parent
        self.project_root = self.script_dir.parent
        self.feature_request_dir = self.project_root / "docs" / "feature_requests"

        self.create_widgets()

    def create_widgets(self):
        # Main frame with padding
        main_frame = ttk.Frame(self.root, padding="10")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # Title
        title_label = ttk.Label(main_frame, text="File a Feature Request",
                                font=('TkDefaultFont', 16, 'bold'))
        title_label.pack(pady=(0, 10))

        # Feature Title
        title_frame = ttk.LabelFrame(main_frame, text="Feature Title", padding="10")
        title_frame.pack(fill=tk.X, pady=(0, 10))
        self.title_var = tk.StringVar()
        self.title_entry = ttk.Entry(title_frame, textvariable=self.title_var)
        self.title_entry.pack(fill=tk.X)
        ttk.Label(title_frame, text="A short, descriptive name for the feature",
                  foreground="gray", font=('TkDefaultFont', 9)).pack(anchor=tk.W)

        # What I Want
        want_frame = ttk.LabelFrame(main_frame, text="What I Want", padding="10")
        want_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        ttk.Label(want_frame, text="Describe the feature or improvement you'd like to see",
                  foreground="gray", font=('TkDefaultFont', 9)).pack(anchor=tk.W)
        self.want_text = scrolledtext.ScrolledText(want_frame, wrap=tk.WORD, height=5)
        self.want_text.pack(fill=tk.BOTH, expand=True, pady=(5, 0))

        # Why I Need It
        why_frame = ttk.LabelFrame(main_frame, text="Why I Need It", padding="10")
        why_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        ttk.Label(why_frame, text="Explain the problem this solves or the value it adds",
                  foreground="gray", font=('TkDefaultFont', 9)).pack(anchor=tk.W)
        self.why_text = scrolledtext.ScrolledText(why_frame, wrap=tk.WORD, height=4)
        self.why_text.pack(fill=tk.BOTH, expand=True, pady=(5, 0))

        # How I Imagine It Working
        how_frame = ttk.LabelFrame(main_frame, text="How I Imagine It Working", padding="10")
        how_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        ttk.Label(how_frame, text="Describe the expected behavior from a user perspective",
                  foreground="gray", font=('TkDefaultFont', 9)).pack(anchor=tk.W)
        self.how_text = scrolledtext.ScrolledText(how_frame, wrap=tk.WORD, height=4)
        self.how_text.pack(fill=tk.BOTH, expand=True, pady=(5, 0))

        # Technical Notes (Optional)
        tech_frame = ttk.LabelFrame(main_frame, text="Technical Notes (Optional)", padding="10")
        tech_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        ttk.Label(tech_frame, text="Any technical constraints, preferences, or implementation ideas",
                  foreground="gray", font=('TkDefaultFont', 9)).pack(anchor=tk.W)
        self.tech_text = scrolledtext.ScrolledText(tech_frame, wrap=tk.WORD, height=3)
        self.tech_text.pack(fill=tk.BOTH, expand=True, pady=(5, 0))

        # Buttons
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X, pady=(10, 0))

        self.submit_btn = ttk.Button(btn_frame, text="Submit Feature Request", command=self.submit_request)
        self.submit_btn.pack(side=tk.RIGHT, padx=(5, 0))

        self.cancel_btn = ttk.Button(btn_frame, text="Back", command=self.go_back)
        self.cancel_btn.pack(side=tk.RIGHT)

    def go_back(self):
        """Go back to main menu."""
        if self.on_close:
            self.on_close()
        else:
            self.root.quit()

    def validate_inputs(self):
        """Validate all required inputs."""
        title = self.title_var.get().strip()
        want = self.want_text.get("1.0", tk.END).strip()
        why = self.why_text.get("1.0", tk.END).strip()
        how = self.how_text.get("1.0", tk.END).strip()

        errors = []

        if not title:
            errors.append("Feature title is required")

        if not want:
            errors.append("'What I Want' description is required")

        if not why:
            errors.append("'Why I Need It' explanation is required")

        if not how:
            errors.append("'How I Imagine It Working' description is required")

        return errors

    def submit_request(self):
        """Submit the feature request."""
        errors = self.validate_inputs()
        if errors:
            messagebox.showerror("Validation Error", "\n".join(errors))
            return

        title = self.title_var.get().strip()
        want = self.want_text.get("1.0", tk.END).strip()
        why = self.why_text.get("1.0", tk.END).strip()
        how = self.how_text.get("1.0", tk.END).strip()
        tech = self.tech_text.get("1.0", tk.END).strip()

        # Create directory if needed
        self.feature_request_dir.mkdir(parents=True, exist_ok=True)

        # Generate filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        safe_title = re.sub(r'[^\w\s-]', '', title)[:50].strip().replace(' ', '_')
        filename = f"Feature_{safe_title}_{timestamp}.md"
        filepath = self.feature_request_dir / filename

        # Generate markdown content
        content = self.generate_markdown(title, want, why, how, tech)

        try:
            with open(filepath, 'w') as f:
                f.write(content)

            messagebox.showinfo("Success",
                f"Feature request saved!\n\n"
                f"Location:\n{filepath.relative_to(self.project_root)}\n\n"
                f"The AI assistant will process this request and:\n"
                f"1. Analyze the request\n"
                f"2. Extract formal requirements\n"
                f"3. Present proposed requirements for approval")

            self.go_back()

        except Exception as e:
            messagebox.showerror("Error", f"Failed to save feature request:\n{e}")

    def generate_markdown(self, title, want, why, how, tech):
        """Generate markdown content for the feature request."""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        content = f"""# Feature Request: {title}

| Field | Value |
|-------|-------|
| **Requested** | {timestamp} |
| **Status** | New |

## What I Want

{want}

## Why I Need It

{why}

## How I Imagine It Working

{how}

"""

        if tech:
            content += f"""## Technical Notes (Optional)

{tech}

"""

        content += """---

*Filed using file_bugreport.py*
"""

        return content


def main():
    parser = argparse.ArgumentParser(description="File bug reports or feature requests for ESP32 Rover firmware")
    parser.add_argument("--version", "-v", default="", help="Firmware version for bug reports (e.g., 2.0.3)")
    parser.add_argument("--hash", "-H", default="", help="Git commit hash for bug reports (e.g., abc1234)")
    parser.add_argument("--bug", "-b", action="store_true", help="Open bug report form directly")
    parser.add_argument("--feature", "-f", action="store_true", help="Open feature request form directly")
    args = parser.parse_args()

    root = tk.Tk()

    if args.bug:
        # Open bug report form directly
        root.title("ESP32 Rover - Bug Report")
        BugReportApp(root, initial_version=args.version, initial_hash=args.hash)
    elif args.feature:
        # Open feature request form directly
        root.title("ESP32 Rover - Feature Request")
        FeatureRequestApp(root)
    else:
        # Open main menu
        MainMenuApp(root, initial_version=args.version, initial_hash=args.hash)

    root.mainloop()


if __name__ == "__main__":
    main()
