# Camera Robustness Improvement Plan

## Overview
Implement three integrated features to make the ESP32-CAM camera system more robust:
1. Manual camera reset button in web UI
2. Automatic watchdog/recovery mechanism
3. Enhanced error messages with diagnostic info

## Critical Files

### Files to Modify
- `firmware/components/web_server/web_server.c` (~300 lines added)
- `firmware/components/web_server/web_ui.c` (~100 lines added)
- `firmware/components/web_server/include/web_server.h` (minor additions)

### Files to Reference (read-only)
- `firmware/components/camera/include/camera.h` (existing API)
- `firmware/components/camera/camera.c` (existing implementation)

## Implementation Details

### Phase 1: Backend State Management

#### Add Camera State Tracking (web_server.c)

**Location**: After existing static variables (around line 75)

```c
#ifdef ROVER_TARGET_ESP32CAM
// Camera reset state
static camera_config_params_t stored_camera_config = {0};
static bool camera_config_stored = false;
static SemaphoreHandle_t camera_reset_mutex = NULL;

// Camera health tracking
typedef struct {
    uint32_t consecutive_failures;
    uint32_t total_failures;
    uint32_t soft_resets;
    uint32_t hard_resets;
    uint32_t last_reset_time;
    bool auto_recovery_enabled;
} camera_health_t;

static camera_health_t camera_health = {0};
static volatile bool stream_should_stop = false;

#define MAX_CONSECUTIVE_FAILURES_SOFT 5
#define MAX_CONSECUTIVE_FAILURES_HARD 10
#define MAX_HARD_RESETS_PER_SESSION 3
#define RESET_COOLDOWN_MS 5000
#endif
```

#### Store Camera Config During Initialization

**Function**: Modify `web_server_init()` (around line 690)

```c
#ifdef ROVER_TARGET_ESP32CAM
// Store camera config for potential reset
if (config->camera_config) {
    memcpy(&stored_camera_config, config->camera_config, sizeof(camera_config_params_t));
    camera_config_stored = true;

    // Create reset mutex
    camera_reset_mutex = xSemaphoreCreateMutex();
    if (!camera_reset_mutex) {
        ESP_LOGE(TAG, "Failed to create camera reset mutex");
    }

    // Initialize health tracking
    camera_health.auto_recovery_enabled = true;
}
#endif
```

### Phase 2: Camera Reset Functionality

#### Add Camera Reset Function

**Location**: New function in web_server.c (before endpoint handlers)

```c
#ifdef ROVER_TARGET_ESP32CAM
static esp_err_t camera_perform_reset(bool is_auto_recovery)
{
    if (!camera_config_stored) {
        ESP_LOGE(TAG, "Cannot reset: camera config not stored");
        return ESP_ERR_INVALID_STATE;
    }

    // Take mutex to prevent concurrent resets
    if (xSemaphoreTake(camera_reset_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Camera reset already in progress");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;

    // Step 1: Stop stream task if running
    if (stream_task_handle != NULL) {
        ESP_LOGI(TAG, "Stopping stream task for camera reset");
        stream_should_stop = true;

        // Wait up to 2 seconds for task to stop
        int wait_count = 0;
        while (stream_task_handle != NULL && wait_count < 40) {
            vTaskDelay(pdMS_TO_TICKS(50));
            wait_count++;
        }

        if (stream_task_handle != NULL) {
            ESP_LOGW(TAG, "Stream task did not stop gracefully, forcing");
            // Task handle will be NULL'd by task itself
        }

        stream_should_stop = false;
    }

    // Step 2: Deinitialize camera
    ESP_LOGI(TAG, "Deinitializing camera for reset");
    ret = camera_module_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera deinit failed: 0x%x", ret);
        xSemaphoreGive(camera_reset_mutex);
        return ret;
    }

    // Small delay for hardware to stabilize
    vTaskDelay(pdMS_TO_TICKS(100));

    // Step 3: Reinitialize camera
    ESP_LOGI(TAG, "Reinitializing camera");
    ret = camera_module_init(&stored_camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera reinit failed: 0x%x", ret);
        xSemaphoreGive(camera_reset_mutex);
        return ret;
    }

    // Step 4: Reset health counters on successful reset
    if (is_auto_recovery) {
        camera_health.hard_resets++;
    }
    camera_health.consecutive_failures = 0;
    camera_health.last_reset_time = (uint32_t)(esp_timer_get_time() / 1000);

    xSemaphoreGive(camera_reset_mutex);

    ESP_LOGI(TAG, "Camera reset successful");
    return ESP_OK;
}
#endif
```

#### Add Reset Endpoint Handler

**Location**: New handler in web_server.c (around line 680)

```c
#ifdef ROVER_TARGET_ESP32CAM
static esp_err_t camera_reset_handler(httpd_req_t *req)
{
    esp_err_t ret = camera_perform_reset(false);

    char response[128];
    if (ret == ESP_OK) {
        snprintf(response, sizeof(response),
                 "{\"status\":\"success\",\"message\":\"Camera reset successful\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_sendstr(req, response);
    } else {
        snprintf(response, sizeof(response),
                 "{\"status\":\"error\",\"code\":\"0x%x\",\"message\":\"Camera reset failed\"}", ret);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, response);
        return ESP_FAIL;
    }
}

static const httpd_uri_t uri_camera_reset = {
    .uri = "/camera/reset",
    .method = HTTP_POST,
    .handler = camera_reset_handler,
    .user_ctx = NULL
};
#endif
```

#### Register Reset Endpoint

**Location**: In `web_server_init()` (around line 750)

```c
#ifdef ROVER_TARGET_ESP32CAM
httpd_register_uri_handler(server, &uri_camera_reset);
ESP_LOGI(TAG, "Camera reset endpoint enabled (/camera/reset)");
#endif
```

### Phase 3: Watchdog/Auto-Recovery in Stream Task

#### Modify Stream Task

**Location**: Update `stream_task()` function (lines 172-241)

**Key Changes**:
1. Check `stream_should_stop` flag at loop start
2. Track consecutive failures
3. Attempt soft reset at 5 failures
4. Attempt hard reset at 10 failures
5. Reset counter on successful frame

```c
static void stream_task(void *pvParameters)
{
    stream_task_data_t *data = (stream_task_data_t *)pvParameters;
    httpd_req_t *req = data->req;
    char part_buf[128];
    esp_err_t res = ESP_OK;
    int paused_counter = 0;

    ESP_LOGI(TAG, "Stream task started on socket %d", data->socket_fd);

    while (true) {
        // Check for stop signal (for camera reset)
        if (stream_should_stop) {
            ESP_LOGI(TAG, "Stream task stopping on request");
            break;
        }

        // Check if streaming is enabled
        if (!camera_stream_is_enabled()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            paused_counter++;
            if (paused_counter == 1) {
                ESP_LOGI(TAG, "Camera stream paused by user");
            }
            continue;
        }

        if (paused_counter > 0) {
            ESP_LOGI(TAG, "Camera stream resumed");
            paused_counter = 0;
        }

        camera_fb_t *fb = camera_capture_frame();
        if (!fb) {
            // Increment failure counter
            camera_health.consecutive_failures++;
            camera_health.total_failures++;

            // Log based on failure count
            if (camera_health.consecutive_failures == 1) {
                ESP_LOGW(TAG, "Camera frame capture failed (attempt 1)");
            } else if (camera_health.consecutive_failures % 10 == 0) {
                ESP_LOGW(TAG, "Camera frame capture failed (%u consecutive failures)",
                         camera_health.consecutive_failures);
            }

            // Auto-recovery logic
            if (camera_health.auto_recovery_enabled) {
                // Soft reset at 5 failures: restart stream
                if (camera_health.consecutive_failures == MAX_CONSECUTIVE_FAILURES_SOFT) {
                    ESP_LOGW(TAG, "Attempting soft camera recovery (restart stream)");
                    camera_health.soft_resets++;
                    // Break and restart - stream will reconnect
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }

                // Hard reset at 10 failures: full reinit
                else if (camera_health.consecutive_failures >= MAX_CONSECUTIVE_FAILURES_HARD) {
                    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
                    uint32_t time_since_reset = now - camera_health.last_reset_time;

                    // Check cooldown and max reset limit
                    if (camera_health.hard_resets < MAX_HARD_RESETS_PER_SESSION &&
                        time_since_reset > RESET_COOLDOWN_MS) {

                        ESP_LOGW(TAG, "Attempting hard camera recovery (full reset)");

                        // Perform reset (will stop this task)
                        esp_err_t ret = camera_perform_reset(true);
                        if (ret == ESP_OK) {
                            ESP_LOGI(TAG, "Auto-recovery reset successful");
                            // Task will be stopped by reset, break here
                            break;
                        } else {
                            ESP_LOGE(TAG, "Auto-recovery reset failed: 0x%x", ret);
                        }
                    } else {
                        if (camera_health.hard_resets >= MAX_HARD_RESETS_PER_SESSION) {
                            if (camera_health.consecutive_failures == MAX_CONSECUTIVE_FAILURES_HARD) {
                                ESP_LOGE(TAG, "Max hard resets reached, disabling auto-recovery");
                                camera_health.auto_recovery_enabled = false;
                            }
                        }
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Frame captured successfully - reset failure counter
        if (camera_health.consecutive_failures > 0) {
            ESP_LOGI(TAG, "Camera recovered after %u failures", camera_health.consecutive_failures);
            camera_health.consecutive_failures = 0;
        }

        // Send frame (existing code)
        size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);

        res = httpd_resp_send_chunk(req, part_buf, hlen);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            ESP_LOGI(TAG, "Stream client disconnected (header)");
            break;
        }

        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            ESP_LOGI(TAG, "Stream client disconnected (data)");
            break;
        }

        res = httpd_resp_send_chunk(req, "\r\n", 2);
        if (res != ESP_OK) {
            camera_return_frame(fb);
            ESP_LOGI(TAG, "Stream client disconnected (boundary)");
            break;
        }

        camera_return_frame(fb);

        vTaskDelay(pdMS_TO_TICKS(66));  // ~15 FPS
    }

    // Cleanup
    httpd_req_async_handler_complete(req);
    free(data);
    stream_task_handle = NULL;
    ESP_LOGI(TAG, "Stream task ended");
    vTaskDelete(NULL);
}
```

### Phase 4: Enhanced Status Reporting

#### Update Status Handler

**Location**: Modify `status_handler()` to include camera health (around line 385)

```c
#ifdef ROVER_TARGET_ESP32CAM
// Add camera health to diagnostics
cJSON *diag = cJSON_GetObjectItem(root, "diag");
if (diag) {
    // Camera status based on failure count
    const char *cam_status;
    if (!camera_is_initialized()) {
        cam_status = "not_init";
    } else if (camera_health.consecutive_failures == 0) {
        cam_status = "ok";
    } else if (camera_health.consecutive_failures < MAX_CONSECUTIVE_FAILURES_SOFT) {
        cam_status = "loading";
    } else if (camera_health.consecutive_failures < MAX_CONSECUTIVE_FAILURES_HARD) {
        cam_status = "retrying";
    } else if (camera_health.auto_recovery_enabled) {
        cam_status = "error";
    } else {
        cam_status = "failed";
    }

    cJSON_AddStringToObject(diag, "cameraStatus", cam_status);
    cJSON_AddNumberToObject(diag, "cameraFailures", camera_health.consecutive_failures);
    cJSON_AddNumberToObject(diag, "cameraSoftResets", camera_health.soft_resets);
    cJSON_AddNumberToObject(diag, "cameraHardResets", camera_health.hard_resets);
}
#endif
```

### Phase 5: Frontend UI Implementation

#### Add Reset Button HTML

**Location**: In `web_ui.c`, add button to controls-row (around line 468)

```html
<div class="control-box" id="cam-reset-box">
    <button class="btn btn-cam-reset" id="btn-cam-reset">RESET CAM</button>
</div>
```

#### Add Reset Button CSS

**Location**: In `web_ui.c` style section (around line 230)

```css
.btn-cam-reset {
    background: #ff6b6b;
    color: #fff;
    padding: 10px 20px;
    border: 2px solid #ff6b6b;
    font-weight: bold;
}
.btn-cam-reset:disabled {
    background: #2d2d44;
    color: #888;
    border-color: #444;
    cursor: not-allowed;
    opacity: 0.5;
}
```

#### Add Reset Button JavaScript

**Location**: In `web_ui.c` JavaScript section (around line 1050)

```javascript
// Camera Reset Functionality
let isResetting = false;

async function resetCamera() {
    if (isResetting) return;

    isResetting = true;
    const btn = document.getElementById('btn-cam-reset');
    const originalText = btn.textContent;
    btn.disabled = true;
    btn.textContent = 'RESETTING...';

    try {
        const response = await fetch('/camera/reset', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' }
        });

        if (response.ok) {
            const data = await response.json();
            console.log('Camera reset successful:', data);

            // Show success briefly
            btn.textContent = 'RESET OK';

            // Restart camera stream after 1 second
            setTimeout(() => {
                if (cameraEnabled) {
                    initCamera();
                }
                btn.disabled = false;
                btn.textContent = originalText;
                isResetting = false;
            }, 1000);
        } else {
            console.error('Camera reset failed');
            btn.textContent = 'RESET FAILED';
            setTimeout(() => {
                btn.disabled = false;
                btn.textContent = originalText;
                isResetting = false;
            }, 2000);
        }
    } catch (e) {
        console.error('Camera reset error:', e);
        btn.textContent = 'RESET ERROR';
        setTimeout(() => {
            btn.disabled = false;
            btn.textContent = originalText;
            isResetting = false;
        }, 2000);
    }
}

document.getElementById('btn-cam-reset').addEventListener('click', resetCamera);
```

#### Enhanced Camera Error Messages

**Location**: Modify `initCamera()` function (around line 928)

```javascript
function initCamera() {
    cameraStream.onload = () => {
        cameraStream.style.display = 'block';
        cameraPlaceholder.style.display = 'none';
    };
    cameraStream.onerror = () => {
        cameraStream.style.display = 'none';
        cameraPlaceholder.style.display = 'block';

        // Update message based on status from backend
        // Will be updated by fetchStatus() interval
        setTimeout(initCamera, 2000);
    };
    cameraStream.src = '/stream?' + new Date().getTime();
}
```

#### Update Status Fetch to Handle Camera Status

**Location**: Modify `fetchStatus()` to update camera placeholder (around line 850)

```javascript
async function fetchStatus() {
    try {
        const response = await fetch('/status');
        if (response.ok) {
            const d = await response.json();

            // ... existing status updates ...

            // Update camera placeholder based on status
            if (d.diag && d.diag.cameraStatus) {
                const cameraPlaceholder = document.getElementById('camera-placeholder');
                if (cameraPlaceholder && cameraPlaceholder.style.display !== 'none') {
                    switch (d.diag.cameraStatus) {
                        case 'loading':
                            cameraPlaceholder.textContent = 'Camera Loading...';
                            break;
                        case 'retrying':
                            cameraPlaceholder.textContent = 'Camera Error - Retrying...';
                            break;
                        case 'error':
                        case 'failed':
                            cameraPlaceholder.textContent = 'Camera Error - Click RESET CAM';
                            break;
                        default:
                            cameraPlaceholder.textContent = 'Camera Loading...';
                    }
                }
            }
        }
    } catch (e) {
        // Silent fail
    }
}
```

#### Show Camera Status in Diagnostics

**Location**: Add to diagnostics display (around line 890)

```javascript
// Camera diagnostics (ESP32-CAM only)
if (d.diag.cameraStatus) {
    const camStatusEl = document.getElementById('diag-cam-status');
    if (camStatusEl) {
        camStatusEl.textContent = d.diag.cameraStatus;
        camStatusEl.className = 'diag-value' +
            (d.diag.cameraStatus === 'ok' ? '' : ' error');
    }

    const camFailuresEl = document.getElementById('diag-cam-failures');
    if (camFailuresEl) {
        camFailuresEl.textContent = d.diag.cameraFailures || 0;
    }
}
```

#### Add Camera Status to Diagnostics HTML

**Location**: In diagnostics panel (around line 540)

```html
<div class="diag-row" id="cam-diag-row">
    <span class="diag-label">Camera Status:</span>
    <span class="diag-value" id="diag-cam-status">-</span>
</div>
<div class="diag-row" id="cam-failures-row">
    <span class="diag-label">Camera Failures:</span>
    <span class="diag-value" id="diag-cam-failures">0</span>
</div>
```

## Verification & Testing

### Unit Testing
1. **Reset Endpoint**: Use curl to test manual reset
   ```bash
   curl -X POST http://esp32-rover.local/camera/reset
   ```

2. **Status Endpoint**: Verify camera health fields
   ```bash
   curl http://esp32-rover.local/status | jq .diag.cameraStatus
   ```

### Integration Testing
1. **Manual Reset Flow**:
   - Open web UI
   - Click "RESET CAM" button
   - Verify button shows "RESETTING..." then "RESET OK"
   - Verify stream restarts automatically
   - Check serial output for reset logs

2. **Auto-Recovery Soft Reset**:
   - Disconnect camera physically (if possible) or cover lens completely
   - Wait for 5 frame failures
   - Verify soft reset attempt in logs
   - Reconnect camera and verify recovery

3. **Auto-Recovery Hard Reset**:
   - Keep camera disconnected
   - Wait for 10 frame failures
   - Verify hard reset attempt in logs
   - Verify camera reinitializes
   - Check that max 3 hard resets are attempted

4. **Enhanced Error Messages**:
   - Monitor camera placeholder text as failures accumulate
   - Verify text changes: "Loading..." → "Retrying..." → "Click RESET CAM"
   - Check diagnostics panel shows camera status

### Serial Monitor Verification
Expected log output for successful reset:
```
I (12345) WEB_SERVER: Stopping stream task for camera reset
I (12456) WEB_SERVER: Deinitializing camera for reset
I (12567) WEB_SERVER: Reinitializing camera
I (12678) CAMERA: Camera initialized successfully
I (12789) WEB_SERVER: Camera reset successful
```

Expected log output for auto-recovery:
```
W (23456) WEB_SERVER: Camera frame capture failed (attempt 1)
W (23567) WEB_SERVER: Camera frame capture failed (5 consecutive failures)
W (23678) WEB_SERVER: Attempting soft camera recovery (restart stream)
I (23789) WEB_SERVER: Stream task ended
I (23890) WEB_SERVER: Stream task started on socket 54
```

### Edge Cases to Test
1. **Concurrent reset requests**: Click reset button rapidly
2. **Reset during active stream**: Multiple clients connected
3. **Camera not initialized**: Reset when camera never started
4. **Max resets exceeded**: Verify auto-recovery disables
5. **Reset cooldown**: Verify 5-second delay between hard resets

## Rollback Plan
If issues arise:
1. All code is guarded by `#ifdef ROVER_TARGET_ESP32CAM` - can be disabled
2. Reset endpoint can be disabled by commenting out registration
3. Auto-recovery can be disabled by setting `camera_health.auto_recovery_enabled = false`
4. UI button can be hidden with CSS: `#cam-reset-box { display: none; }`

## Build Commands
```bash
# Build for ESP32-CAM
./scripts/build.sh esp32cam

# Flash via serial
./scripts/build.sh esp32cam flash monitor

# Flash via OTA
OTA_PASSWORD="rover1234" ./scripts/ota.sh esp32cam
```

## Summary
This implementation provides a robust camera error recovery system with:
- User control via manual reset button
- Automatic recovery for transient failures
- Clear user feedback at each stage
- Thread-safe design with proper mutex protection
- Comprehensive logging for debugging
- Graceful degradation (disables after too many failures)
- No modifications to camera component internals

Total estimated changes: ~400 lines of code across 2 files (web_server.c, web_ui.c)
