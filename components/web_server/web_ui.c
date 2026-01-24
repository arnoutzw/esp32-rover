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

                <div class="control-box">
                    <button class="btn btn-stop" id="btn-estop">EMERGENCY STOP</button>
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
    </script>
</body>
</html>
)rawliteral";

const char* web_ui_get_html(void)
{
    return web_ui_html;
}
