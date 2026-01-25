#!/usr/bin/env python3
"""
ESP32 Rover - Web-based Bug Report & Feature Request Tool

A Flask web application to file bug reports and feature requests remotely.
Provides the same functionality as file_report.py but accessible via browser.

Usage:
    python scripts/file_report_web.py

    Then access: http://localhost:5000 (or via VPN)

Options:
    --host HOST     Host to bind to (default: 0.0.0.0)
    --port PORT     Port to bind to (default: 5000)
    --debug         Enable debug mode
"""

import os
import sys
import re
import json
import argparse
import urllib.request
from datetime import datetime
from pathlib import Path
from functools import wraps

# Try to import Flask
try:
    from flask import Flask, render_template_string, request, jsonify, redirect, url_for
except ImportError:
    print("Error: Flask is required. Install it with:")
    print("  pip install flask")
    sys.exit(1)

# Determine project root
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent
BUGREPORT_DIR = PROJECT_ROOT / "docs" / "bugreport"
FEATURE_REQUEST_DIR = PROJECT_ROOT / "docs" / "feature_requests"

app = Flask(__name__)

# HTML Templates
BASE_TEMPLATE = '''
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{{ page_title }} - ESP32 Rover</title>
    <style>
        :root {
            --bg-color: #1a1a2e;
            --card-bg: #16213e;
            --accent: #0f3460;
            --primary: #e94560;
            --text: #eaeaea;
            --text-muted: #a0a0a0;
            --success: #4caf50;
            --error: #f44336;
            --border: #0f3460;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: var(--bg-color);
            color: var(--text);
            min-height: 100vh;
            padding: 20px;
        }

        .container {
            max-width: 800px;
            margin: 0 auto;
        }

        h1 {
            text-align: center;
            margin-bottom: 10px;
            color: var(--primary);
        }

        h2 {
            font-size: 1.5rem;
            margin-bottom: 20px;
            text-align: center;
            color: var(--text);
        }

        .subtitle {
            text-align: center;
            color: var(--text-muted);
            margin-bottom: 30px;
        }

        .card {
            background: var(--card-bg);
            border-radius: 12px;
            padding: 25px;
            margin-bottom: 20px;
            border: 1px solid var(--border);
        }

        .card-title {
            font-size: 1.1rem;
            font-weight: 600;
            margin-bottom: 15px;
            color: var(--text);
        }

        .form-group {
            margin-bottom: 20px;
        }

        label {
            display: block;
            margin-bottom: 8px;
            font-weight: 500;
            color: var(--text);
        }

        .label-hint {
            font-weight: normal;
            font-size: 0.85rem;
            color: var(--text-muted);
        }

        input[type="text"],
        textarea {
            width: 100%;
            padding: 12px;
            border: 1px solid var(--border);
            border-radius: 8px;
            background: var(--bg-color);
            color: var(--text);
            font-size: 1rem;
            transition: border-color 0.2s;
        }

        input[type="text"]:focus,
        textarea:focus {
            outline: none;
            border-color: var(--primary);
        }

        textarea {
            resize: vertical;
            min-height: 100px;
        }

        .btn {
            padding: 12px 24px;
            border: none;
            border-radius: 8px;
            font-size: 1rem;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s;
            text-decoration: none;
            display: inline-block;
        }

        .btn-primary {
            background: var(--primary);
            color: white;
        }

        .btn-primary:hover {
            background: #d63e52;
        }

        .btn-secondary {
            background: var(--accent);
            color: var(--text);
        }

        .btn-secondary:hover {
            background: #1a4b7a;
        }

        .btn-group {
            display: flex;
            gap: 10px;
            justify-content: flex-end;
            margin-top: 20px;
        }

        .menu-btn {
            display: block;
            width: 100%;
            padding: 20px;
            margin-bottom: 15px;
            background: var(--accent);
            border: 1px solid var(--border);
            border-radius: 12px;
            color: var(--text);
            text-align: center;
            text-decoration: none;
            font-size: 1.1rem;
            font-weight: 500;
            transition: all 0.2s;
        }

        .menu-btn:hover {
            background: var(--primary);
            border-color: var(--primary);
        }

        .menu-btn-desc {
            font-size: 0.9rem;
            color: var(--text-muted);
            margin-top: 8px;
            font-weight: normal;
        }

        .inline-group {
            display: flex;
            gap: 15px;
            align-items: flex-end;
        }

        .inline-group .form-group {
            flex: 1;
            margin-bottom: 0;
        }

        .fetch-group {
            display: flex;
            gap: 10px;
            align-items: flex-end;
        }

        .fetch-group .form-group {
            flex: 1;
            margin-bottom: 0;
        }

        .status-message {
            padding: 10px 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            font-size: 0.95rem;
        }

        .status-success {
            background: rgba(76, 175, 80, 0.2);
            border: 1px solid var(--success);
            color: var(--success);
        }

        .status-error {
            background: rgba(244, 67, 54, 0.2);
            border: 1px solid var(--error);
            color: var(--error);
        }

        .status-info {
            background: rgba(233, 69, 96, 0.2);
            border: 1px solid var(--primary);
            color: var(--primary);
        }

        #fetchStatus {
            margin-top: 10px;
            font-size: 0.9rem;
        }

        .back-link {
            display: inline-flex;
            align-items: center;
            color: var(--text-muted);
            text-decoration: none;
            margin-bottom: 20px;
            font-size: 0.95rem;
        }

        .back-link:hover {
            color: var(--primary);
        }

        .back-link::before {
            content: "←";
            margin-right: 8px;
        }

        @media (max-width: 600px) {
            .inline-group {
                flex-direction: column;
            }

            .inline-group .form-group {
                width: 100%;
            }

            .btn-group {
                flex-direction: column;
            }

            .btn {
                width: 100%;
                text-align: center;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        {% block content %}{% endblock %}
    </div>
    {% block scripts %}{% endblock %}
</body>
</html>
'''

INDEX_TEMPLATE = '''
{% extends "base" %}
{% block content %}
<h1>ESP32 Rover</h1>
<h2>Report & Request Tool</h2>
<p class="subtitle">Choose what you'd like to file:</p>

<div class="card">
    <a href="{{ url_for('bug_report') }}" class="menu-btn">
        File Bug Report
        <div class="menu-btn-desc">Report a problem or unexpected behavior</div>
    </a>

    <a href="{{ url_for('feature_request') }}" class="menu-btn">
        File Feature Request
        <div class="menu-btn-desc">Suggest a new feature or improvement</div>
    </a>
</div>
{% endblock %}
'''

BUG_REPORT_TEMPLATE = '''
{% extends "base" %}
{% block content %}
<a href="{{ url_for('index') }}" class="back-link">Back to Menu</a>

<h2>File a Bug Report</h2>

{% if message %}
<div class="status-message {{ message_type }}">{{ message }}</div>
{% endif %}

<form method="POST" action="{{ url_for('bug_report') }}">
    <div class="card">
        <div class="card-title">Build Information</div>

        <div class="inline-group">
            <div class="form-group">
                <label for="version">Version <span class="label-hint">(e.g., 2.0.3)</span></label>
                <input type="text" id="version" name="version" value="{{ version }}" required
                       pattern="^\\d+\\.\\d+(\\.\\d+)?$" placeholder="2.0.3">
            </div>

            <div class="form-group">
                <label for="git_hash">Git Hash <span class="label-hint">(7 chars)</span></label>
                <input type="text" id="git_hash" name="git_hash" value="{{ git_hash }}" required
                       pattern="^[a-fA-F0-9]{7,40}$" placeholder="abc1234" maxlength="40">
            </div>
        </div>

        <div class="fetch-group" style="margin-top: 15px;">
            <div class="form-group">
                <label for="device_ip">Device IP/Hostname</label>
                <input type="text" id="device_ip" name="device_ip" value="ttgo-rover.local" placeholder="ttgo-rover.local">
            </div>
            <button type="button" class="btn btn-secondary" onclick="fetchBuildInfo()">Fetch Build Info</button>
        </div>
        <div id="fetchStatus"></div>
    </div>

    <div class="card">
        <div class="form-group">
            <label for="title">Bug Title</label>
            <input type="text" id="title" name="title" value="{{ title }}" required placeholder="Brief description of the bug">
        </div>

        <div class="form-group">
            <label for="description">Description <span class="label-hint">(What happened? What did you expect?)</span></label>
            <textarea id="description" name="description" rows="5" required placeholder="Describe the bug in detail...">{{ description }}</textarea>
        </div>

        <div class="form-group">
            <label for="steps">Steps to Reproduce <span class="label-hint">(Optional)</span></label>
            <textarea id="steps" name="steps" rows="4" placeholder="1. Do this&#10;2. Then that&#10;3. Bug occurs">{{ steps }}</textarea>
        </div>

        <div class="form-group">
            <label for="notes">Additional Notes / Technical Details <span class="label-hint">(Optional)</span></label>
            <textarea id="notes" name="notes" rows="3" placeholder="Any additional context...">{{ notes }}</textarea>
        </div>

        <div class="btn-group">
            <a href="{{ url_for('index') }}" class="btn btn-secondary">Cancel</a>
            <button type="submit" class="btn btn-primary">Submit Bug Report</button>
        </div>
    </div>
</form>
{% endblock %}

{% block scripts %}
<script>
async function fetchBuildInfo() {
    const deviceIp = document.getElementById('device_ip').value.trim();
    const statusDiv = document.getElementById('fetchStatus');

    if (!deviceIp) {
        statusDiv.innerHTML = '<span style="color: var(--error);">Please enter a device IP or hostname</span>';
        return;
    }

    statusDiv.innerHTML = '<span style="color: var(--text-muted);">Fetching build info...</span>';

    try {
        const response = await fetch('/api/fetch-build-info?device=' + encodeURIComponent(deviceIp));
        const data = await response.json();

        if (data.success) {
            if (data.version) document.getElementById('version').value = data.version;
            if (data.hash) document.getElementById('git_hash').value = data.hash;
            statusDiv.innerHTML = '<span style="color: var(--success);">Fetched: v' + data.version + ' (' + data.hash + ')</span>';
        } else {
            statusDiv.innerHTML = '<span style="color: var(--error);">' + data.error + '</span>';
        }
    } catch (e) {
        statusDiv.innerHTML = '<span style="color: var(--error);">Failed to fetch: ' + e.message + '</span>';
    }
}
</script>
{% endblock %}
'''

FEATURE_REQUEST_TEMPLATE = '''
{% extends "base" %}
{% block content %}
<a href="{{ url_for('index') }}" class="back-link">Back to Menu</a>

<h2>File a Feature Request</h2>

{% if message %}
<div class="status-message {{ message_type }}">{{ message }}</div>
{% endif %}

<form method="POST" action="{{ url_for('feature_request') }}">
    <div class="card">
        <div class="form-group">
            <label for="title">Feature Title</label>
            <input type="text" id="title" name="title" value="{{ title }}" required placeholder="A short, descriptive name for the feature">
            <span class="label-hint">A short, descriptive name for the feature</span>
        </div>

        <div class="form-group">
            <label for="want">What I Want</label>
            <textarea id="want" name="want" rows="5" required placeholder="Describe the feature or improvement you'd like to see...">{{ want }}</textarea>
            <span class="label-hint">Describe the feature or improvement you'd like to see</span>
        </div>

        <div class="form-group">
            <label for="why">Why I Need It</label>
            <textarea id="why" name="why" rows="4" required placeholder="Explain the problem this solves or the value it adds...">{{ why }}</textarea>
            <span class="label-hint">Explain the problem this solves or the value it adds</span>
        </div>

        <div class="form-group">
            <label for="how">How I Imagine It Working</label>
            <textarea id="how" name="how" rows="4" required placeholder="Describe the expected behavior from a user perspective...">{{ how }}</textarea>
            <span class="label-hint">Describe the expected behavior from a user perspective</span>
        </div>

        <div class="form-group">
            <label for="tech">Technical Notes <span class="label-hint">(Optional)</span></label>
            <textarea id="tech" name="tech" rows="3" placeholder="Any technical constraints, preferences, or implementation ideas...">{{ tech }}</textarea>
            <span class="label-hint">Any technical constraints, preferences, or implementation ideas</span>
        </div>

        <div class="btn-group">
            <a href="{{ url_for('index') }}" class="btn btn-secondary">Cancel</a>
            <button type="submit" class="btn btn-primary">Submit Feature Request</button>
        </div>
    </div>
</form>
{% endblock %}
'''

SUCCESS_TEMPLATE = '''
{% extends "base" %}
{% block content %}
<h2>{{ report_type }} Submitted!</h2>

<div class="card">
    <div class="status-message status-success">
        Your {{ report_type.lower() }} has been saved successfully.
    </div>

    <div class="form-group">
        <label>Location:</label>
        <code style="display: block; padding: 10px; background: var(--bg-color); border-radius: 5px; word-break: break-all;">
            {{ filepath }}
        </code>
    </div>

    <p style="margin-top: 20px; color: var(--text-muted);">
        {% if report_type == 'Bug Report' %}
        The AI assistant will investigate this bug report and create:
        <ul style="margin-top: 10px; margin-left: 20px;">
            <li>RCA_*.md (Root Cause Analysis)</li>
            <li>FIX_PLAN_*.md (Fix Implementation Plan)</li>
        </ul>
        {% else %}
        The AI assistant will process this request and:
        <ol style="margin-top: 10px; margin-left: 20px;">
            <li>Analyze the request</li>
            <li>Extract formal requirements</li>
            <li>Present proposed requirements for approval</li>
        </ol>
        {% endif %}
    </p>

    <div class="btn-group" style="margin-top: 30px;">
        <a href="{{ url_for('index') }}" class="btn btn-primary">Back to Menu</a>
    </div>
</div>
{% endblock %}
'''


def render_with_base(template_str, **kwargs):
    """Render a template that extends the base template."""
    from jinja2 import Environment
    env = Environment()
    env.globals['url_for'] = url_for

    # Create a template dict
    templates = {
        'base': BASE_TEMPLATE,
        'content': template_str
    }

    # Simple template inheritance simulation
    full_template = BASE_TEMPLATE.replace('{% block content %}{% endblock %}', template_str.split('{% block content %}')[1].split('{% endblock %}')[0] if '{% block content %}' in template_str else template_str)
    full_template = full_template.replace('{% block scripts %}{% endblock %}', template_str.split('{% block scripts %}')[1].split('{% endblock %}')[0] if '{% block scripts %}' in template_str else '')

    return render_template_string(full_template, **kwargs)


@app.route('/')
def index():
    return render_with_base(INDEX_TEMPLATE, page_title="Home")


@app.route('/bug-report', methods=['GET', 'POST'])
def bug_report():
    if request.method == 'POST':
        # Get form data
        version = request.form.get('version', '').strip()
        git_hash = request.form.get('git_hash', '').strip()[:7].lower()
        title = request.form.get('title', '').strip()
        description = request.form.get('description', '').strip()
        steps = request.form.get('steps', '').strip()
        notes = request.form.get('notes', '').strip()

        # Validate
        errors = []
        if not version or not re.match(r'^\d+\.\d+(\.\d+)?$', version):
            errors.append("Version must be in format X.Y or X.Y.Z")
        if not git_hash or not re.match(r'^[a-fA-F0-9]{7,40}$', git_hash):
            errors.append("Git hash must be 7-40 hexadecimal characters")
        if not title:
            errors.append("Bug title is required")
        if not description:
            errors.append("Bug description is required")

        if errors:
            return render_with_base(BUG_REPORT_TEMPLATE,
                page_title="Bug Report",
                version=version, git_hash=git_hash, title=title,
                description=description, steps=steps, notes=notes,
                message="; ".join(errors), message_type="status-error")

        # Create directory structure
        version_dir = f"v{version}" if not version.startswith('v') else version
        report_dir = BUGREPORT_DIR / version_dir / git_hash
        report_dir.mkdir(parents=True, exist_ok=True)

        # Generate filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        safe_title = re.sub(r'[^\w\s-]', '', title)[:50].strip().replace(' ', '_')
        filename = f"Bugreport_{safe_title}_{timestamp}.md"
        filepath = report_dir / filename

        # Generate markdown content
        content = generate_bug_markdown(version, git_hash, title, description, steps, notes)

        try:
            with open(filepath, 'w') as f:
                f.write(content)

            relative_path = filepath.relative_to(PROJECT_ROOT)
            return render_with_base(SUCCESS_TEMPLATE,
                page_title="Success",
                report_type="Bug Report",
                filepath=str(relative_path))
        except Exception as e:
            return render_with_base(BUG_REPORT_TEMPLATE,
                page_title="Bug Report",
                version=version, git_hash=git_hash, title=title,
                description=description, steps=steps, notes=notes,
                message=f"Failed to save: {e}", message_type="status-error")

    # GET request
    return render_with_base(BUG_REPORT_TEMPLATE,
        page_title="Bug Report",
        version="", git_hash="", title="",
        description="", steps="", notes="",
        message=None, message_type=None)


@app.route('/feature-request', methods=['GET', 'POST'])
def feature_request():
    if request.method == 'POST':
        # Get form data
        title = request.form.get('title', '').strip()
        want = request.form.get('want', '').strip()
        why = request.form.get('why', '').strip()
        how = request.form.get('how', '').strip()
        tech = request.form.get('tech', '').strip()

        # Validate
        errors = []
        if not title:
            errors.append("Feature title is required")
        if not want:
            errors.append("'What I Want' is required")
        if not why:
            errors.append("'Why I Need It' is required")
        if not how:
            errors.append("'How I Imagine It Working' is required")

        if errors:
            return render_with_base(FEATURE_REQUEST_TEMPLATE,
                page_title="Feature Request",
                title=title, want=want, why=why, how=how, tech=tech,
                message="; ".join(errors), message_type="status-error")

        # Create directory if needed
        FEATURE_REQUEST_DIR.mkdir(parents=True, exist_ok=True)

        # Generate filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        safe_title = re.sub(r'[^\w\s-]', '', title)[:50].strip().replace(' ', '_')
        filename = f"Feature_{safe_title}_{timestamp}.md"
        filepath = FEATURE_REQUEST_DIR / filename

        # Generate markdown content
        content = generate_feature_markdown(title, want, why, how, tech)

        try:
            with open(filepath, 'w') as f:
                f.write(content)

            relative_path = filepath.relative_to(PROJECT_ROOT)
            return render_with_base(SUCCESS_TEMPLATE,
                page_title="Success",
                report_type="Feature Request",
                filepath=str(relative_path))
        except Exception as e:
            return render_with_base(FEATURE_REQUEST_TEMPLATE,
                page_title="Feature Request",
                title=title, want=want, why=why, how=how, tech=tech,
                message=f"Failed to save: {e}", message_type="status-error")

    # GET request
    return render_with_base(FEATURE_REQUEST_TEMPLATE,
        page_title="Feature Request",
        title="", want="", why="", how="", tech="",
        message=None, message_type=None)


@app.route('/api/fetch-build-info')
def fetch_build_info():
    """API endpoint to fetch build info from device."""
    device = request.args.get('device', '').strip()

    if not device:
        return jsonify({'success': False, 'error': 'No device specified'})

    url = f"http://{device}/status"

    try:
        req = urllib.request.Request(url, headers={'Accept': 'application/json'})
        with urllib.request.urlopen(req, timeout=5) as response:
            data = json.loads(response.read().decode())

        diag = data.get("diag", {})
        version = diag.get("buildVersion", "")
        fingerprint = diag.get("buildFingerprint", "")

        result_version = ""
        result_hash = ""

        if version:
            version_match = re.match(r'(\d+\.\d+\.\d+)', version)
            if version_match:
                result_version = version_match.group(1)

        if fingerprint:
            result_hash = fingerprint[:7]

        return jsonify({
            'success': True,
            'version': result_version,
            'hash': result_hash
        })

    except urllib.error.URLError as e:
        return jsonify({'success': False, 'error': f'Connection error: {e.reason}'})
    except json.JSONDecodeError:
        return jsonify({'success': False, 'error': 'Invalid response from device'})
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)})


def generate_bug_markdown(version, git_hash, title, description, steps, notes):
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

*Filed using file_report_web.py*
"""

    return content


def generate_feature_markdown(title, want, why, how, tech):
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

*Filed using file_report_web.py*
"""

    return content


def main():
    parser = argparse.ArgumentParser(
        description="Web-based bug report and feature request tool for ESP32 Rover"
    )
    parser.add_argument('--host', default='0.0.0.0', help='Host to bind to (default: 0.0.0.0)')
    parser.add_argument('--port', type=int, default=5000, help='Port to bind to (default: 5000)')
    parser.add_argument('--debug', action='store_true', help='Enable debug mode')
    args = parser.parse_args()

    print(f"\n{'='*60}")
    print("ESP32 Rover - Web Report Tool")
    print(f"{'='*60}")
    print(f"\nServer running at: http://{args.host}:{args.port}")
    print(f"Local access: http://localhost:{args.port}")
    print(f"\nPress Ctrl+C to stop the server")
    print(f"{'='*60}\n")

    app.run(host=args.host, port=args.port, debug=args.debug)


if __name__ == '__main__':
    main()
