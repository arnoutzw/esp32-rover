#!/usr/bin/env python3
"""
Bug Report Filing Tool for ESP32 Rover Firmware

A simple GUI application to file bug reports with proper structure
for AI-assisted investigation and resolution.

Usage:
    python scripts/file_bugreport.py

    Or with the firmware version pre-filled:
    python scripts/file_bugreport.py --version 2.0.3 --hash abc1234
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


class BugReportApp:
    def __init__(self, root, initial_version="", initial_hash=""):
        self.root = root
        self.root.title("ESP32 Rover - Bug Report")
        self.root.geometry("700x600")
        self.root.minsize(500, 400)

        # Determine project root (script is in scripts/, go up one level)
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
        self.desc_text = scrolledtext.ScrolledText(desc_frame, wrap=tk.WORD, height=8)
        self.desc_text.pack(fill=tk.BOTH, expand=True)

        # Steps to Reproduce
        steps_frame = ttk.LabelFrame(main_frame, text="Steps to Reproduce (Optional)", padding="10")
        steps_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        self.steps_text = scrolledtext.ScrolledText(steps_frame, wrap=tk.WORD, height=5)
        self.steps_text.pack(fill=tk.BOTH, expand=True)

        # Additional Notes
        notes_frame = ttk.LabelFrame(main_frame, text="Additional Notes / Technical Details (Optional)",
                                     padding="10")
        notes_frame.pack(fill=tk.BOTH, expand=True, pady=(0, 10))
        self.notes_text = scrolledtext.ScrolledText(notes_frame, wrap=tk.WORD, height=4)
        self.notes_text.pack(fill=tk.BOTH, expand=True)

        # Buttons
        btn_frame = ttk.Frame(main_frame)
        btn_frame.pack(fill=tk.X, pady=(10, 0))

        self.submit_btn = ttk.Button(btn_frame, text="Submit Bug Report", command=self.submit_report)
        self.submit_btn.pack(side=tk.RIGHT, padx=(5, 0))

        self.cancel_btn = ttk.Button(btn_frame, text="Cancel", command=self.root.quit)
        self.cancel_btn.pack(side=tk.RIGHT)

    def fetch_build_info(self):
        """Fetch build info from device via REST API."""
        import urllib.request
        import json

        device_ip = self.device_ip_var.get().strip()
        if not device_ip:
            messagebox.showerror("Error", "Please enter a device IP or hostname")
            return

        url = f"http://{device_ip}/status"
        self.fetch_btn.config(state=tk.DISABLED)
        self.root.update()

        try:
            with urllib.request.urlopen(url, timeout=5) as response:
                data = json.loads(response.read().decode())

            diag = data.get("diag", {})
            version = diag.get("buildVersion", "")
            fingerprint = diag.get("buildFingerprint", "")

            if version:
                # Extract version number (e.g., "2.0.3+15" -> "2.0.3")
                version_match = re.match(r'(\d+\.\d+\.\d+)', version)
                if version_match:
                    self.version_var.set(version_match.group(1))

            if fingerprint:
                self.hash_var.set(fingerprint[:7])

            messagebox.showinfo("Success", f"Fetched build info:\nVersion: {version}\nHash: {fingerprint}")

        except urllib.error.URLError as e:
            messagebox.showerror("Connection Error", f"Could not connect to device:\n{e}")
        except json.JSONDecodeError:
            messagebox.showerror("Error", "Invalid response from device")
        except Exception as e:
            messagebox.showerror("Error", f"Failed to fetch build info:\n{e}")
        finally:
            self.fetch_btn.config(state=tk.NORMAL)

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

            self.root.quit()

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


def main():
    parser = argparse.ArgumentParser(description="File a bug report for ESP32 Rover firmware")
    parser.add_argument("--version", "-v", default="", help="Firmware version (e.g., 2.0.3)")
    parser.add_argument("--hash", "-H", default="", help="Git commit hash (e.g., abc1234)")
    args = parser.parse_args()

    root = tk.Tk()
    app = BugReportApp(root, initial_version=args.version, initial_hash=args.hash)
    root.mainloop()


if __name__ == "__main__":
    main()
