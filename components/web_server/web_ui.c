#include "web_server.h"

// HTML/CSS/JavaScript for the rover control interface
static const char web_ui_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>ESP32 Rover Control</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a2e;
            color: #eee;
            min-height: 100vh;
            overflow-y: auto;
            overflow-x: hidden;
        }
        .container {
            display: flex;
            flex-direction: column;
            min-height: 100vh;
            padding: 10px;
        }
        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px;
            background: #16213e;
            border-radius: 10px;
            margin-bottom: 10px;
        }
        .header h1 {
            font-size: 1.2em;
            color: #0f4c75;
        }
        .status-indicator {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #ff4444;
        }
        .status-dot.connected {
            background: #44ff44;
        }
        .main-content {
            display: flex;
            flex: 1;
            gap: 10px;
            min-height: 0;
        }
        .camera-panel {
            flex: 2;
            display: flex;
            flex-direction: column;
            background: #16213e;
            border-radius: 10px;
            overflow: hidden;
        }
        .camera-view {
            flex: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            background: #000;
            position: relative;
        }
        .camera-view img {
            max-width: 100%;
            max-height: 100%;
            object-fit: contain;
        }
        .camera-placeholder {
            color: #666;
            font-size: 1.2em;
        }
        .control-panel {
            flex: 1;
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        .joystick-container {
            flex: 1;
            background: #16213e;
            border-radius: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            min-height: 200px;
            touch-action: none;
        }
        .joystick {
            width: 180px;
            height: 180px;
            background: radial-gradient(circle, #2d2d44 0%, #1a1a2e 100%);
            border-radius: 50%;
            position: relative;
            border: 3px solid #0f4c75;
            touch-action: none;
        }
        .joystick-knob {
            width: 60px;
            height: 60px;
            background: radial-gradient(circle, #3282b8 0%, #0f4c75 100%);
            border-radius: 50%;
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            cursor: pointer;
            box-shadow: 0 4px 15px rgba(0,0,0,0.3);
        }
        .controls-row {
            display: flex;
            gap: 10px;
        }
        .control-box {
            flex: 1;
            background: #16213e;
            border-radius: 10px;
            padding: 15px;
        }
        .control-box h3 {
            font-size: 0.9em;
            color: #888;
            margin-bottom: 10px;
        }
        .slider-container {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .slider {
            flex: 1;
            -webkit-appearance: none;
            height: 8px;
            background: #2d2d44;
            border-radius: 4px;
            outline: none;
        }
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            background: #3282b8;
            border-radius: 50%;
            cursor: pointer;
        }
        .slider-value {
            min-width: 40px;
            text-align: right;
            font-family: monospace;
        }
        .btn {
            padding: 12px 20px;
            border: none;
            border-radius: 8px;
            font-size: 1em;
            cursor: pointer;
            transition: all 0.2s;
        }
        .btn-stop {
            background: #e74c3c;
            color: white;
            width: 100%;
        }
        .btn-stop:hover {
            background: #c0392b;
        }
        .btn-stop:active {
            transform: scale(0.98);
        }
        .btn-led {
            background: #2d2d44;
            color: #888;
            padding: 10px 20px;
            border: 2px solid #444;
        }
        .btn-led.on {
            background: #f9ca24;
            color: #1a1a2e;
            border-color: #f9ca24;
            box-shadow: 0 0 15px rgba(249, 202, 36, 0.5);
        }
        .btn-led:hover {
            background: #444;
        }
        .btn-led.on:hover {
            background: #e0b720;
        }
        .telemetry {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
            background: #16213e;
            border-radius: 10px;
            padding: 15px;
        }
        .telemetry-item {
            display: flex;
            flex-direction: column;
        }
        .telemetry-label {
            font-size: 0.8em;
            color: #888;
        }
        .telemetry-value {
            font-size: 1.2em;
            font-family: monospace;
            color: #3282b8;
        }
        .button-indicators {
            display: flex;
            gap: 10px;
            margin-top: 5px;
        }
        .hw-button {
            padding: 8px 16px;
            border-radius: 6px;
            font-size: 0.9em;
            font-weight: bold;
            background: #2d2d44;
            color: #666;
            border: 2px solid #444;
            transition: all 0.1s;
        }
        .hw-button.pressed {
            background: #3282b8;
            color: #fff;
            border-color: #3282b8;
            box-shadow: 0 0 10px rgba(50, 130, 184, 0.5);
        }
        .diagnostics {
            background: #16213e;
            border-radius: 10px;
            padding: 15px;
            margin-top: 10px;
        }
        .diag-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
            cursor: pointer;
        }
        .diag-header h3 {
            font-size: 0.9em;
            color: #888;
            margin: 0;
        }
        .diag-toggle {
            color: #3282b8;
            font-size: 0.8em;
        }
        .diag-content {
            display: none;
        }
        .diag-content.expanded {
            display: block;
        }
        .diag-section {
            margin-bottom: 12px;
        }
        .diag-section-title {
            font-size: 0.75em;
            color: #3282b8;
            text-transform: uppercase;
            padding: 4px 8px;
            background: #2d2d44;
            border-radius: 4px;
            margin-bottom: 6px;
        }
        .diag-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 6px;
        }
        .diag-item {
            display: flex;
            justify-content: space-between;
            font-size: 0.85em;
        }
        .diag-label {
            color: #888;
        }
        .diag-value {
            font-family: monospace;
            color: #44ff44;
        }
        .diag-value.warn {
            color: #ffaa00;
        }
        .diag-value.error {
            color: #ff4444;
        }
        .diag-sub {
            font-family: monospace;
            font-size: 0.8em;
            color: #00aaff;
            display: block;
        }
        .diag-bar {
            height: 8px;
            background: #2d2d44;
            border-radius: 4px;
            overflow: hidden;
            margin-top: 4px;
        }
        .diag-bar-fill {
            height: 100%;
            transition: width 0.3s;
        }
        .diag-full-row {
            grid-column: span 2;
        }
        @media (max-width: 768px) {
            .main-content {
                flex-direction: column;
            }
            .camera-panel {
                flex: none;
                height: 40vh;
            }
            .joystick {
                width: 150px;
                height: 150px;
            }
            .joystick-knob {
                width: 50px;
                height: 50px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ESP32-CAM Rover</h1>
            <div class="status-indicator">
                <span id="conn-status">Disconnected</span>
                <div class="status-dot" id="status-dot"></div>
            </div>
        </div>

        <div class="main-content">
            <div class="camera-panel">
                <div class="camera-view">
                    <img id="camera-stream" src="" alt="Camera Stream" style="display:none;">
                    <div class="camera-placeholder" id="camera-placeholder">Camera Loading...</div>
                </div>
            </div>

            <div class="control-panel">
                <div class="joystick-container">
                    <div class="joystick" id="joystick">
                        <div class="joystick-knob" id="joystick-knob"></div>
                    </div>
                </div>

                <div class="controls-row">
                    <div class="control-box">
                        <h3>MAX SPEED</h3>
                        <div class="slider-container">
                            <input type="range" class="slider" id="speed-limit" min="0" max="100" value="50">
                            <span class="slider-value" id="speed-limit-value">50%</span>
                        </div>
                    </div>
                    <div class="control-box">
                        <h3>TRIM</h3>
                        <div class="slider-container">
                            <input type="range" class="slider" id="trim" min="-20" max="20" value="0">
                            <span class="slider-value" id="trim-value">0</span>
                        </div>
                    </div>
                </div>

                <div class="controls-row">
                    <div class="control-box">
                        <button class="btn btn-led" id="btn-led">FLASH LED</button>
                    </div>
                    <div class="control-box">
                        <button class="btn btn-stop" id="btn-estop">EMERGENCY STOP</button>
                    </div>
                </div>

                <div class="telemetry">
                    <div class="telemetry-item">
                        <span class="telemetry-label">Speed</span>
                        <span class="telemetry-value" id="tel-speed">0%</span>
                    </div>
                    <div class="telemetry-item">
                        <span class="telemetry-label">Steering</span>
                        <span class="telemetry-value" id="tel-steering">0°</span>
                    </div>
                    <div class="telemetry-item">
                        <span class="telemetry-label">Velocity</span>
                        <span class="telemetry-value" id="tel-velocity">0 rad/s</span>
                    </div>
                    <div class="telemetry-item">
                        <span class="telemetry-label">Battery</span>
                        <span class="telemetry-value" id="tel-battery">--V</span>
                    </div>
                    <div class="telemetry-item" style="grid-column: span 2;">
                        <span class="telemetry-label">Hardware Buttons</span>
                        <div class="button-indicators">
                            <span class="hw-button" id="btn-left">L</span>
                            <span class="hw-button" id="btn-right">R</span>
                        </div>
                    </div>
                </div>

                <div class="diagnostics">
                    <div class="diag-header" onclick="toggleDiagnostics()">
                        <h3>DIAGNOSTICS</h3>
                        <span class="diag-toggle" id="diag-toggle">+ Show</span>
                    </div>
                    <div class="diag-content" id="diag-content">
                        <div class="diag-section">
                            <div class="diag-section-title">WiFi</div>
                            <div class="diag-grid">
                                <div class="diag-item">
                                    <span class="diag-label">SSID</span>
                                    <span class="diag-value" id="diag-ssid">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Channel</span>
                                    <span class="diag-value" id="diag-channel">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">TX Power</span>
                                    <span class="diag-value" id="diag-txpower">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Clients</span>
                                    <span class="diag-value" id="diag-clients">--</span>
                                </div>
                                <div class="diag-item diag-full-row">
                                    <span class="diag-label">IP</span>
                                    <span class="diag-value" id="diag-ip">--</span>
                                </div>
                                <div class="diag-item diag-full-row">
                                    <span class="diag-label">MAC</span>
                                    <span class="diag-value" id="diag-mac">--</span>
                                </div>
                            </div>
                        </div>
                        <div class="diag-section">
                            <div class="diag-section-title">Memory</div>
                            <div class="diag-grid">
                                <div class="diag-item diag-full-row">
                                    <span class="diag-label">RAM Used</span>
                                    <span class="diag-value" id="diag-ram">--</span>
                                </div>
                                <div class="diag-full-row">
                                    <div class="diag-bar">
                                        <div class="diag-bar-fill" id="diag-ram-bar" style="width:0%;background:#44ff44;"></div>
                                    </div>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Internal</span>
                                    <span class="diag-value" id="diag-internal">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Watermark</span>
                                    <span class="diag-value" id="diag-watermark">--</span>
                                </div>
                            </div>
                        </div>
                        <div class="diag-section">
                            <div class="diag-section-title">System</div>
                            <div class="diag-grid">
                                <div class="diag-item">
                                    <span class="diag-label">CPU</span>
                                    <span class="diag-value" id="diag-cpu">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Tasks</span>
                                    <span class="diag-value" id="diag-tasks">--</span>
                                    <span class="diag-sub" id="diag-tasks-cores">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Battery</span>
                                    <span class="diag-value" id="diag-battery">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Uptime</span>
                                    <span class="diag-value" id="diag-uptime">--</span>
                                </div>
                            </div>
                        </div>
                        <div class="diag-section">
                            <div class="diag-section-title">Services</div>
                            <div class="diag-grid">
                                <div class="diag-item">
                                    <span class="diag-label">REST API</span>
                                    <span class="diag-value" id="diag-restapi">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">MQTT</span>
                                    <span class="diag-value" id="diag-mqtt">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Internet</span>
                                    <span class="diag-value" id="diag-internet">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Local Time</span>
                                    <span class="diag-value" id="diag-localtime">--:--:--</span>
                                </div>
                            </div>
                        </div>
                        <div class="diag-section">
                            <div class="diag-section-title" style="display:flex;justify-content:space-between;align-items:center;">
                                <span>System Logs</span>
                                <div style="display:flex;gap:6px;align-items:center;">
                                    <select id="log-level-filter" style="font-size:0.7em;padding:2px 4px;background:#1a1a2e;color:#eee;border:1px solid #333;border-radius:3px;">
                                        <option value="all">All</option>
                                        <option value="error">Errors</option>
                                        <option value="warn">Warnings+</option>
                                        <option value="info" selected>Info+</option>
                                    </select>
                                    <button id="log-clear-btn" style="font-size:0.65em;padding:2px 6px;background:#333;color:#eee;border:none;border-radius:3px;cursor:pointer;">Clear</button>
                                    <label style="font-size:0.65em;display:flex;align-items:center;gap:3px;cursor:pointer;">
                                        <input type="checkbox" id="log-autoscroll" checked style="margin:0;">
                                        Auto
                                    </label>
                                </div>
                            </div>
                            <div id="log-container" style="max-height:180px;overflow-y:auto;background:#0d0d0d;border-radius:4px;padding:6px;font-family:monospace;font-size:0.7em;line-height:1.3;margin-top:6px;">
                                <div id="log-entries"></div>
                            </div>
                            <div class="diag-grid" style="margin-top:4px;">
                                <div class="diag-item">
                                    <span class="diag-label">Entries</span>
                                    <span class="diag-value" id="log-count">--</span>
                                </div>
                                <div class="diag-item">
                                    <span class="diag-label">Dropped</span>
                                    <span class="diag-value" id="log-dropped">--</span>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        // Configuration
        const SEND_INTERVAL = 50;  // ms between control updates
        const STATUS_INTERVAL = 100;  // ms between status requests (10Hz for responsive buttons)

        // State
        let speed = 0;
        let steering = 0;
        let speedLimit = 50;
        let trim = 0;
        let connected = false;
        let estop = false;

        // DOM Elements
        const joystick = document.getElementById('joystick');
        const knob = document.getElementById('joystick-knob');
        const statusDot = document.getElementById('status-dot');
        const connStatus = document.getElementById('conn-status');
        const cameraStream = document.getElementById('camera-stream');
        const cameraPlaceholder = document.getElementById('camera-placeholder');

        // Joystick control
        let joystickActive = false;
        let joystickRect;
        let joystickCenter = { x: 0, y: 0 };
        const maxDistance = 60;

        function updateJoystickRect() {
            joystickRect = joystick.getBoundingClientRect();
            joystickCenter = {
                x: joystickRect.left + joystickRect.width / 2,
                y: joystickRect.top + joystickRect.height / 2
            };
        }

        function handleJoystickMove(clientX, clientY) {
            if (!joystickActive) return;

            let dx = clientX - joystickCenter.x;
            let dy = clientY - joystickCenter.y;

            // Limit to circle
            const distance = Math.sqrt(dx * dx + dy * dy);
            if (distance > maxDistance) {
                dx = (dx / distance) * maxDistance;
                dy = (dy / distance) * maxDistance;
            }

            // Update knob position
            knob.style.transform = `translate(calc(-50% + ${dx}px), calc(-50% + ${dy}px))`;

            // Calculate speed and steering (-100 to 100)
            speed = -dy / maxDistance * 100;  // Inverted: up = forward
            steering = dx / maxDistance * 100;

            // Update telemetry display
            document.getElementById('tel-speed').textContent = Math.round(speed) + '%';
            document.getElementById('tel-steering').textContent = Math.round(steering) + '°';
        }

        function handleJoystickEnd() {
            joystickActive = false;
            knob.style.transform = 'translate(-50%, -50%)';
            speed = 0;
            steering = 0;
            document.getElementById('tel-speed').textContent = '0%';
            document.getElementById('tel-steering').textContent = '0°';
        }

        // Mouse events
        joystick.addEventListener('mousedown', (e) => {
            joystickActive = true;
            updateJoystickRect();
            handleJoystickMove(e.clientX, e.clientY);
        });

        document.addEventListener('mousemove', (e) => {
            handleJoystickMove(e.clientX, e.clientY);
        });

        document.addEventListener('mouseup', handleJoystickEnd);

        // Touch events
        joystick.addEventListener('touchstart', (e) => {
            e.preventDefault();
            joystickActive = true;
            updateJoystickRect();
            const touch = e.touches[0];
            handleJoystickMove(touch.clientX, touch.clientY);
        });

        document.addEventListener('touchmove', (e) => {
            if (joystickActive) {
                e.preventDefault();
                const touch = e.touches[0];
                handleJoystickMove(touch.clientX, touch.clientY);
            }
        }, { passive: false });

        document.addEventListener('touchend', handleJoystickEnd);

        // Slider controls
        document.getElementById('speed-limit').addEventListener('input', (e) => {
            speedLimit = parseInt(e.target.value);
            document.getElementById('speed-limit-value').textContent = speedLimit + '%';
        });

        document.getElementById('trim').addEventListener('input', (e) => {
            trim = parseInt(e.target.value);
            document.getElementById('trim-value').textContent = trim;
        });

        // Emergency stop
        document.getElementById('btn-estop').addEventListener('click', () => {
            estop = true;
            speed = 0;
            steering = 0;
            sendCommand();
            setTimeout(() => { estop = false; }, 100);
        });

        // Send control command
        async function sendCommand() {
            const actualSpeed = speed * speedLimit / 100;
            const actualSteering = steering + trim;

            try {
                const response = await fetch('/control', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        speed: actualSpeed,
                        steering: actualSteering,
                        estop: estop
                    })
                });

                if (response.ok) {
                    setConnected(true);
                } else {
                    setConnected(false);
                }
            } catch (e) {
                setConnected(false);
            }
        }

        // Toggle diagnostics panel
        function toggleDiagnostics() {
            const content = document.getElementById('diag-content');
            const toggle = document.getElementById('diag-toggle');
            const expanded = content.classList.toggle('expanded');
            toggle.textContent = expanded ? '- Hide' : '+ Show';
        }

        // Format uptime as HH:MM:SS
        function formatUptime(secs) {
            const hrs = Math.floor(secs / 3600);
            const mins = Math.floor((secs % 3600) / 60);
            const s = secs % 60;
            return String(hrs).padStart(2, '0') + ':' +
                   String(mins).padStart(2, '0') + ':' +
                   String(s).padStart(2, '0');
        }

        // Format bytes as KB
        function formatKB(bytes) {
            return Math.round(bytes / 1024) + 'K';
        }

        // Fetch status
        async function fetchStatus() {
            try {
                const response = await fetch('/status');
                if (response.ok) {
                    const data = await response.json();
                    document.getElementById('tel-velocity').textContent =
                        data.velocity.toFixed(1) + ' rad/s';
                    document.getElementById('tel-battery').textContent =
                        data.battery.toFixed(1) + 'V';
                    // Update button indicators
                    document.getElementById('btn-left').classList.toggle('pressed', data.btnL);
                    document.getElementById('btn-right').classList.toggle('pressed', data.btnR);

                    // Update diagnostics if available
                    if (data.diag) {
                        const d = data.diag;
                        // WiFi section
                        document.getElementById('diag-ssid').textContent = d.ssid || '--';
                        document.getElementById('diag-channel').textContent = d.channel;
                        document.getElementById('diag-txpower').textContent = d.txPower + ' dBm';
                        document.getElementById('diag-clients').textContent = d.clients;
                        document.getElementById('diag-ip').textContent = d.ip || '--';
                        document.getElementById('diag-mac').textContent = d.mac || '--';

                        // Memory section
                        const usedHeap = d.totalHeap - d.freeHeap;
                        const heapPct = d.totalHeap > 0 ? Math.round(usedHeap * 100 / d.totalHeap) : 0;
                        document.getElementById('diag-ram').textContent =
                            formatKB(usedHeap) + ' / ' + formatKB(d.totalHeap);
                        const ramBar = document.getElementById('diag-ram-bar');
                        ramBar.style.width = heapPct + '%';
                        ramBar.style.background = heapPct < 70 ? '#44ff44' :
                                                  heapPct < 90 ? '#ffaa00' : '#ff4444';
                        document.getElementById('diag-internal').textContent = formatKB(d.freeInternal);
                        document.getElementById('diag-watermark').textContent = formatKB(d.minHeap);

                        // System section
                        document.getElementById('diag-cpu').textContent = d.cpuFreq + ' MHz';
                        document.getElementById('diag-tasks').textContent = d.tasks;
                        document.getElementById('diag-tasks-cores').textContent = 'C0:' + d.tasksCore0 + ' C1:' + d.tasksCore1;
                        document.getElementById('diag-battery').textContent = data.battery.toFixed(2) + 'V';
                        document.getElementById('diag-uptime').textContent = formatUptime(d.uptime);

                        // Services section
                        const restEl = document.getElementById('diag-restapi');
                        restEl.textContent = d.restApi ? 'ON' : 'OFF';
                        restEl.className = 'diag-value' + (d.restApi ? '' : ' error');

                        const mqttEl = document.getElementById('diag-mqtt');
                        if (!d.mqttEnabled) {
                            mqttEl.textContent = 'OFF';
                            mqttEl.className = 'diag-value error';
                        } else if (d.mqttConnected) {
                            mqttEl.textContent = 'Connected';
                            mqttEl.className = 'diag-value';
                        } else {
                            mqttEl.textContent = 'Disconnected';
                            mqttEl.className = 'diag-value warn';
                        }

                        // Internet connectivity (REQ-10)
                        const internetEl = document.getElementById('diag-internet');
                        internetEl.textContent = d.internet ? 'Connected' : 'Offline';
                        internetEl.className = 'diag-value' + (d.internet ? '' : ' error');

                        // Local time (REQ-13)
                        const timeEl = document.getElementById('diag-localtime');
                        timeEl.textContent = d.localTime || '--:--:--';
                        timeEl.className = 'diag-value' + (d.ntpSynced ? '' : ' warn');
                    }

                    setConnected(true);
                }
            } catch (e) {
                // Status fetch failed, don't update connection status here
            }
        }

        function setConnected(state) {
            connected = state;
            statusDot.classList.toggle('connected', state);
            connStatus.textContent = state ? 'Connected' : 'Disconnected';
        }

        // Initialize camera stream
        function initCamera() {
            cameraStream.onload = () => {
                cameraStream.style.display = 'block';
                cameraPlaceholder.style.display = 'none';
            };
            cameraStream.onerror = () => {
                cameraStream.style.display = 'none';
                cameraPlaceholder.style.display = 'block';
                cameraPlaceholder.textContent = 'Camera Error';
                // Retry after 2 seconds
                setTimeout(initCamera, 2000);
            };
            cameraStream.src = '/stream?' + new Date().getTime();
        }

        // Start intervals
        setInterval(sendCommand, SEND_INTERVAL);
        setInterval(fetchStatus, STATUS_INTERVAL);

        // Initialize
        window.addEventListener('resize', updateJoystickRect);
        updateJoystickRect();
        initCamera();

        // =============================================================================
        // Log Streaming (REQ-31)
        // =============================================================================
        let logAutoScroll = true;
        let logLastTimestamp = 0;
        let logPollInterval = null;

        const logLevelColors = {
            'E': '#ff4444',
            'W': '#ffaa00',
            'I': '#44ff44',
            'D': '#888888',
            'V': '#666666'
        };

        function formatLogTimestamp(ms) {
            const secs = Math.floor(ms / 1000);
            const mins = Math.floor(secs / 60);
            const hours = Math.floor(mins / 60);
            return String(hours).padStart(2,'0') + ':' +
                   String(mins % 60).padStart(2,'0') + ':' +
                   String(secs % 60).padStart(2,'0');
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        function appendLogEntry(log) {
            const container = document.getElementById('log-entries');
            const entry = document.createElement('div');
            entry.style.marginBottom = '2px';
            entry.style.wordBreak = 'break-word';

            const levelColor = logLevelColors[log.l] || '#ccc';
            entry.innerHTML =
                '<span style="color:#555">' + formatLogTimestamp(log.t) + '</span> ' +
                '<span style="color:' + levelColor + ';font-weight:bold">' + log.l + '</span> ' +
                '<span style="color:#3282b8">' + escapeHtml(log.tag) + '</span> ' +
                '<span style="color:#ccc">' + escapeHtml(log.msg) + '</span>';

            container.appendChild(entry);

            // Limit displayed entries to prevent memory issues
            while (container.children.length > 300) {
                container.removeChild(container.firstChild);
            }

            // Auto-scroll if enabled
            if (logAutoScroll) {
                const logContainer = document.getElementById('log-container');
                logContainer.scrollTop = logContainer.scrollHeight;
            }
        }

        async function fetchLogs() {
            try {
                const levelFilter = document.getElementById('log-level-filter').value;
                const url = '/logs?level=' + levelFilter + '&since=' + logLastTimestamp + '&limit=50';
                const response = await fetch(url);
                if (response.ok) {
                    const data = await response.json();

                    // Update stats
                    if (data.stats) {
                        document.getElementById('log-count').textContent = data.stats.count;
                        document.getElementById('log-dropped').textContent = data.stats.dropped;
                    }

                    // Append new logs
                    if (data.logs && data.logs.length > 0) {
                        data.logs.forEach(log => {
                            appendLogEntry(log);
                            if (log.t > logLastTimestamp) {
                                logLastTimestamp = log.t;
                            }
                        });
                    }
                }
            } catch (e) {
                // Silently fail
            }
        }

        async function clearLogs() {
            try {
                await fetch('/logs', { method: 'DELETE' });
                document.getElementById('log-entries').innerHTML = '';
                logLastTimestamp = 0;
            } catch (e) {
                // Silently fail
            }
        }

        // Initialize log polling
        function initLogPolling() {
            // Initial fetch to get existing logs
            fetchLogs();

            // Poll every 1 second for new logs
            if (logPollInterval) clearInterval(logPollInterval);
            logPollInterval = setInterval(fetchLogs, 1000);
        }

        // Event listeners for log controls
        document.getElementById('log-level-filter').addEventListener('change', () => {
            document.getElementById('log-entries').innerHTML = '';
            logLastTimestamp = 0;
            fetchLogs();
        });

        document.getElementById('log-autoscroll').addEventListener('change', (e) => {
            logAutoScroll = e.target.checked;
        });

        document.getElementById('log-clear-btn').addEventListener('click', clearLogs);

        // Start log polling
        initLogPolling();

        // =============================================================================
        // Flash LED Control
        // =============================================================================
        let ledState = false;

        async function toggleLED() {
            try {
                const response = await fetch('/led', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ on: !ledState })
                });
                if (response.ok) {
                    const data = await response.json();
                    ledState = data.on;
                    updateLEDButton();
                }
            } catch (e) {
                // Silently fail
            }
        }

        async function fetchLEDState() {
            try {
                const response = await fetch('/led');
                if (response.ok) {
                    const data = await response.json();
                    ledState = data.on;
                    updateLEDButton();
                }
            } catch (e) {
                // Silently fail
            }
        }

        function updateLEDButton() {
            const btn = document.getElementById('btn-led');
            btn.classList.toggle('on', ledState);
            btn.textContent = ledState ? 'FLASH LED (ON)' : 'FLASH LED';
        }

        document.getElementById('btn-led').addEventListener('click', toggleLED);

        // Initial LED state fetch
        fetchLEDState();
    </script>
</body>
</html>
)rawliteral";

const char* web_ui_get_html(void)
{
    return web_ui_html;
}
