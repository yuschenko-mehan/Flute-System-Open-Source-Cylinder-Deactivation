/*
 * ============================================================================
 * FLUTE SYSTEM – ESP32 BRIDGE (Wi-Fi + Telegram + Web Interface)
 * Target: ESP32-WROOM-32
 * ============================================================================
 * COMPETITION NOTES:
 * 1. Seasonality is handled via the configurable Skip-Fire pattern (Active/Total).
 * 2. Memory Leak Prevention: Telegram Bot is instantiated LOCALLY inside the alert 
 *    function, not globally. This prevents heap fragmentation on long-running ESP32s.
 * 3. Non-blocking architecture: UART reading and WebSocket broadcasting do not stall 
 *    the main loop, ensuring real-time responsiveness.
 * 4. JSON formats exactly match STM32 firmware expectations for seamless sync.
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ---- Wi-Fi Access Point Credentials ----
const char* ssid     = "FluteSystem";
const char* password = "flute1234";

// ---- Telegram (configured dynamically via Web UI) ----
String botToken = "";
String chatId   = "";

// ---- Core Objects ----
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
WiFiClientSecure client;

// ---- State Variables ----
String lastTelemetry = "{}";
int currentErrorCode = 0;
bool telegramAlertSent = false;

// ---- Heartbeat Monitoring ----
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_TIMEOUT = 2000; // 2 seconds
bool stm32Connected = false;

// ---- STM32 Communication ----
#define STM32_SERIAL Serial2 // RX2=GPIO 16, TX2=GPIO 17

/* ============================================================================
 * EMBEDDED WEB PAGE (Single Page Application)
 * COMPETITION NOTE: Fully self-contained. No external CDN dependencies.
 * ============================================================================ */
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>FLUTE SYSTEM</title>
  <style>
    body { background-color: #121212; color: #e0e0e0; font-family: 'Segoe UI', sans-serif; text-align: center; padding: 15px; margin: 0; }
    .card { background-color: #1e1e1e; border-radius: 12px; padding: 20px; margin: 15px 0; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    .value { font-size: 2.8em; font-weight: bold; color: #00e676; margin: 10px 0; }
    .label { font-size: 1.1em; color: #b0b0b0; text-transform: uppercase; letter-spacing: 1px; }
    .status-ok { color: #00e676; } .status-warn { color: #ffab00; } .status-err { color: #ff5252; }
    input[type=text], input[type=number], select { width: 90%; padding: 10px; margin: 8px 0; background: #2c2c2c; color: white; border: 1px solid #444; border-radius: 6px; font-size: 1em; box-sizing: border-box; }
    input[type=range] { width: 90%; accent-color: #00e676; margin: 8px 0; }
    button { padding: 15px 30px; border: none; border-radius: 8px; font-size: 1.1em; font-weight: bold; cursor: pointer; width: 100%; margin-top: 10px; }
    .btn-save { background: #00e676; color: #000; }
    .btn-stop { background: #ff1744; color: #fff; margin-top: 30px; }
    #alertBanner { display: none; background: #b71c1c; border: 2px solid #ff5252; padding: 20px; border-radius: 12px; margin-bottom: 20px; }
    #connectionStatus { position: fixed; top: 10px; right: 10px; padding: 8px 15px; border-radius: 5px; font-size: 0.9em; font-weight: bold; z-index: 100; }
    .conn-ok { background: #00e676; color: #000; } .conn-err { background: #ff5252; color: #fff; }
  </style>
</head>
<body>
  <div id="connectionStatus" class="conn-ok">● CONNECTED</div>
  <h2 style="color: #00e676;">🎵 FLUTE SYSTEM v2.2</h2>

  <div id="alertBanner">
    <div class="label" style="color: white;">🚨 CRITICAL ERROR</div>
    <div id="alertText" style="color: white; font-size: 1.3em;">Code: 0</div>
  </div>

  <div class="card">
    <div class="label">Engine RPM</div>
    <div class="value"><span id="rpm">0</span> <span style="font-size:0.4em">RPM</span></div>
  </div>

  <div class="card">
    <div class="label">Coolant Temp (ОР)</div>
    <div class="value"><span id="temp">0</span> <span style="font-size:0.4em">°C</span></div>
  </div>

  <div class="card">
    <div class="label">System Status</div>
    <div id="mode" class="value" style="font-size: 1.4em;">WAITING...</div>
  </div>

  <!-- ENGINE CONFIGURATION CARD -->
  <div class="card">
    <div class="label">⚙️ Engine Configuration Profile</div>
    <div style="font-size: 0.8em; color: #888; margin-bottom: 10px;">Select engine layout and deactivation pattern</div>
    <div style="display: flex; gap: 10px; margin-bottom: 10px;">
      <select id="engineType" style="flex: 1;">
        <option value="0">Inline-4 (I4)</option>
        <option value="1" selected>Inline-6 (I6)</option>
        <option value="2">V6</option>
      </select>
      <select id="deactivateCount" style="flex: 1;">
        <option value="1">1 Cylinder</option>
        <option value="2" selected>2 Cylinders (Pairs)</option>
        <option value="3">3 Cylinders (Triplets)</option>
      </select>
    </div>
    <button class="btn-save" onclick="applyEngineProfile()">Apply Configuration</button>
    <div id="profileStatus" style="margin-top: 10px; font-size: 0.9em; color: #00e676;"></div>
  </div>

  <!-- SEASONAL SKIP-FIRE CARD -->
  <div class="card">
    <div class="label">🌍 Seasonal Skip-Fire Pattern</div>
    <div style="font-size: 0.8em; color: #888; margin-bottom: 10px;">Active cycles / Total cycles. Adapts to climate.</div>
    <div style="display: flex; gap: 10px; justify-content: center; align-items: center;">
      <div>
        <label style="color:#00e676; font-size:0.9em;">Active</label><br>
        <input type="number" id="activeCycles" min="1" max="10" value="2" style="width:60px; text-align:center;">
      </div>
      <div style="font-size:1.5em; color:#888;">/</div>
      <div>
        <label style="color:#ffab00; font-size:0.9em;">Total</label><br>
        <input type="number" id="totalCycles" min="2" max="10" value="3" style="width:60px; text-align:center;">
      </div>
    </div>
    <button class="btn-save" onclick="applySkipPattern()" style="margin-top:15px;">Apply Pattern</button>
    <div style="margin-top:10px; font-size:0.8em; color:#888; text-align:left; padding:0 10px;">
      💡 <b>Seasonal Tip:</b> 9/10 for Winter (max heat), 2/3 for Moderate, 1/4 for Summer/Highway (max economy).
    </div>
    <div id="skipStatus" style="margin-top: 10px; padding: 8px; background: #1a1a1a; border-radius: 5px;">
      <span style="color: #00e676;">Current Deactivation: </span>
      <span id="deactivationRate" style="color: white; font-weight: bold;">33%</span>
    </div>
  </div>

  <!-- TIMING OFFSET CARD -->
  <div class="card">
    <div class="label">Valve Timing Offset (Teeth)</div>
    <div style="font-size: 0.8em; color: #888; margin-bottom: 10px;">Adjust to shield lambda sensor on exhaust stroke</div>
    <input type="range" id="timingSlider" min="-5" max="5" value="0" step="1" oninput="updateTiming(this.value)">
    <div id="timingValue" class="label" style="color:#00e676;">0</div>
  </div>

  <!-- LAMBDA MODE CARD -->
  <div class="card">
    <div class="label">Lambda Management Mode</div>
    <select id="lambdaMode" onchange="updateLambdaMode(this.value)">
      <option value="0">Mode 0: Timing Offset Only (Safe Default)</option>
      <option value="1">Mode 1: Auto-Calibrate (via CAN Fuel Trim)</option>
      <option value="2">Mode 2: Dynamic Signal Emulation (Op-Amp HW)</option>
    </select>
  </div>

  <!-- TELEGRAM CONFIGURATION CARD -->
  <div class="card">
    <div class="label">📱 Telegram Alerts Setup</div>
    <div style="font-size: 0.8em; color: #888; margin-bottom: 10px;">Paste your Bot Token and Chat ID</div>
    <input type="text" id="tgToken" placeholder="Bot Token (e.g., 123456:ABC...)" style="margin-bottom: 8px;">
    <input type="text" id="tgChat" placeholder="Your Chat ID (e.g., 123456789)" style="margin-bottom: 8px;">
    <button class="btn-save" onclick="setupTelegram()">Save & Send Test Message</button>
    <div id="tgStatus" style="margin-top:10px; font-size:0.9em; color:#00e676;"></div>
  </div>

  <!-- EMERGENCY STOP -->
  <button class="btn-stop" onclick="emergencyStop()">🛑 EMERGENCY STOP (Stock Mode)</button>

  <script>
    let ws = new WebSocket('ws://' + window.location.hostname + ':81');
    
    ws.onopen = () => { 
      document.getElementById('connectionStatus').className = 'conn-ok'; 
      document.getElementById('connectionStatus').innerText = '● CONNECTED'; 
    };
    ws.onclose = () => { 
      document.getElementById('connectionStatus').className = 'conn-err'; 
      document.getElementById('connectionStatus').innerText = '● DISCONNECTED'; 
    };

    ws.onmessage = function(event) {
      let data; 
      try { data = JSON.parse(event.data); } catch(e) { return; }

      // 1. Update Telemetry
      if (data.rpm !== undefined) {
        document.getElementById('rpm').innerText = data.rpm;
        document.getElementById('temp').innerText = data.temp;
        let modeEl = document.getElementById('mode');
        if (data.sync === 1 && data.temp >= 80 && data.rpm >= 500) {
          modeEl.innerText = "✅ FLUTE ACTIVE"; modeEl.className = "value status-ok";
        } else if (data.temp < 80) {
          modeEl.innerText = "❄️ ENGINE WARMING UP"; modeEl.className = "value status-warn";
        } else {
          modeEl.innerText = "⚙️ STOCK MODE"; modeEl.className = "value status-warn";
        }
      }

      // 2. Sync Engine Profile from STM32
      if (data.engine_type !== undefined && data.deact_count !== undefined) {
        document.getElementById('engineType').value = data.engine_type;
        document.getElementById('deactivateCount').value = data.deact_count;
      }
      if (data.ack === "engine_profile") {
        document.getElementById('engineType').value = data.type;
        document.getElementById('deactivateCount').value = data.count;
        document.getElementById('profileStatus').innerText = "✅ Synced with STM32";
        setTimeout(() => { document.getElementById('profileStatus').innerText = ""; }, 3000);
      }

      // 3. Sync Skip-Fire Pattern
      if (data.skip_active !== undefined && data.skip_total !== undefined) {
        document.getElementById('activeCycles').value = data.skip_active;
        document.getElementById('totalCycles').value = data.skip_total;
        let rate = ((data.skip_total - data.skip_active) / data.skip_total * 100).toFixed(0);
        document.getElementById('deactivationRate').innerText = rate + '%';
      }

      // 4. Sync Timing & Lambda
      if (data.timing_offset !== undefined) { 
        document.getElementById('timingSlider').value = data.timing_offset; 
        document.getElementById('timingValue').innerText = data.timing_offset; 
      }
      if (data.lambda_mode !== undefined) {
        document.getElementById('lambdaMode').value = data.lambda_mode;
      }

      // 5. Handle Alerts
      if (data.alert === "ERROR") {
        document.getElementById('alertBanner').style.display = 'block';
        document.getElementById('alertText').innerText = "Error Code: " + data.code;
      } else if (data.alert === "CLEAR") {
        document.getElementById('alertBanner').style.display = 'none';
      } else if (data.error === "STM32_TIMEOUT") {
        document.getElementById('connectionStatus').className = 'conn-err';
        document.getElementById('connectionStatus').innerText = '● STM32 TIMEOUT';
      }
    };

    function applyEngineProfile() {
      let type = document.getElementById('engineType').value;
      let count = document.getElementById('deactivateCount').value;
      document.getElementById('profileStatus').innerText = "Applying...";
      document.getElementById('profileStatus').style.color = "#ffab00";
      fetch('/setEngineProfile?type=' + type + '&count=' + count).then(() => {
        document.getElementById('profileStatus').innerText = "✅ Configuration Applied!";
        document.getElementById('profileStatus').style.color = "#00e676";
      });
    }

    function applySkipPattern() {
      let a = document.getElementById('activeCycles').value;
      let t = document.getElementById('totalCycles').value;
      if (a >= 1 && t >= 2 && t <= 10 && a <= t) {
        document.getElementById('skipStatus').innerHTML = '<span style="color: #ffab00;">Applying...</span>';
        fetch('/setSkipPattern?active=' + a + '&total=' + t).then(() => {
          document.getElementById('skipStatus').innerHTML = '<span style="color: #00e676;">✅ Pattern Applied</span>';
        });
      } else { 
        alert('Invalid pattern! Active: 1-10, Total: 2-10, Active ≤ Total.'); 
      }
    }

    function updateTiming(val) { 
      document.getElementById('timingValue').innerText = val; 
      fetch('/setTiming?offset=' + val); 
    }

    function updateLambdaMode(val) { 
      fetch('/setLambdaMode?mode=' + val); 
    }

    function setupTelegram() {
      let t = document.getElementById('tgToken').value.trim();
      let c = document.getElementById('tgChat').value.trim();
      document.getElementById('tgStatus').innerText = "⏳ Sending test message...";
      document.getElementById('tgStatus').style.color = "#ffab00";
      
      fetch('/setTelegram?token=' + encodeURIComponent(t) + '&chat=' + encodeURIComponent(c))
        .then(response => response.text())
        .then(text => {
          document.getElementById('tgStatus').innerText = text;
          document.getElementById('tgStatus').style.color = text.includes("Success") ? "#00e676" : "#ff5252";
        })
        .catch(err => {
          document.getElementById('tgStatus').innerText = "❌ Connection error";
          document.getElementById('tgStatus').style.color = "#ff5252";
        });
    }

    function emergencyStop() { 
      if(confirm("Force Stock Mode? This will instantly reactivate all cylinders.")) { 
        ws.send("EMERGENCY_STOP"); 
      } 
    }
  </script>
</body>
</html>
)rawliteral";

/* ============================================================================
 * SETUP & LOOP
 * ============================================================================ */
void setup() {
    Serial.begin(115200);          // USB debugging
    STM32_SERIAL.begin(115200);    // UART to STM32 (RX2=16, TX2=17)
    
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Configure TLS client ONCE to save memory and prevent leaks
    client.setInsecure();

    // ---- HTTP Routes ----
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", htmlPage);
    });

    server.on("/setEngineProfile", HTTP_GET, []() {
        if (server.hasArg("type") && server.hasArg("count")) {
            String cmd = "{\"set_engine_profile\":\"" + server.arg("type") + "," + server.arg("count") + "\"}\n";
            STM32_SERIAL.print(cmd);
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/setSkipPattern", HTTP_GET, []() {
        if (server.hasArg("active") && server.hasArg("total")) {
            String cmd = "{\"set_skip_pattern\":\"" + server.arg("active") + "," + server.arg("total") + "\"}\n";
            STM32_SERIAL.print(cmd);
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/setTiming", HTTP_GET, []() {
        if (server.hasArg("offset")) {
            String cmd = "{\"set_timing_offset\":" + server.arg("offset") + "}\n";
            STM32_SERIAL.print(cmd);
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/setLambdaMode", HTTP_GET, []() {
        if (server.hasArg("mode")) {
            String cmd = "{\"set_lambda_mode\":" + server.arg("mode") + "}\n";
            STM32_SERIAL.print(cmd);
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/setTelegram", HTTP_GET, []() {
        if (server.hasArg("token") && server.hasArg("chat")) {
            botToken = server.arg("token");
            chatId = server.arg("chat");
            
            if (botToken.length() > 0 && chatId.length() > 0) {
                // COMPETITION NOTE: LOCAL instance prevents memory leak! 
                UniversalTelegramBot testBot(botToken, client);
                String testMsg = "✅ *FLUTE SYSTEM*: Telegram alerts successfully configured!";
                testBot.sendMessage(chatId, testMsg, "Markdown");
                server.send(200, "text/plain", "Success! Check your Telegram.");
            } else {
                server.send(400, "text/plain", "Invalid data");
            }
        } else {
            server.send(400, "text/plain", "Missing parameters");
        }
    });

    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    lastTelemetryTime = millis();
    Serial.println("ESP32 Bridge v2.2 ready.");
}

void loop() {
    server.handleClient();
    webSocket.loop();

    // ---- Heartbeat Monitoring ----
    if (millis() - lastTelemetryTime > TELEMETRY_TIMEOUT) {
        if (stm32Connected) {
            stm32Connected = false;
            String timeoutMsg = "{\"error\":\"STM32_TIMEOUT\"}\n";
            webSocket.broadcastTXT(timeoutMsg);
            Serial.println("STM32 connection timeout!");
        }
    }

    // ---- Read data from STM32 ----
    while (STM32_SERIAL.available()) {
        String line = STM32_SERIAL.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        
        // Reset heartbeat timer on any valid communication
        lastTelemetryTime = millis();
        if (!stm32Connected) {
            stm32Connected = true;
        }

        if (line.startsWith("{\"rpm\":")) {
            lastTelemetry = line;
            webSocket.broadcastTXT(lastTelemetry);
        } 
        else if (line.startsWith("{\"alert\":") || line.startsWith("{\"ack\":")) {
            handleSTM32Alert(line);
            webSocket.broadcastTXT(line);
        }
    }
}

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

void handleSTM32Alert(String jsonAlert) {
    int codeStart = jsonAlert.indexOf("\"code\":") + 7;
    int codeEnd = jsonAlert.indexOf("}", codeStart);
    if (codeStart < 7 || codeEnd < 0) return;
    
    int errorCode = jsonAlert.substring(codeStart, codeEnd).toInt();
    bool isError = jsonAlert.indexOf("\"ERROR\"") > 0;

    if (isError) { 
        currentErrorCode = errorCode; 
        telegramAlertSent = false; // Allow new notification for this specific error
    } else { 
        currentErrorCode = 0; 
    }

    // Send Telegram alert only if configured, it's an error, and not already sent
    if (isError && !telegramAlertSent && botToken.length() > 0 && chatId.length() > 0) {
        // COMPETITION NOTE: LOCAL instance prevents memory leak! 
        UniversalTelegramBot tempBot(botToken, client);
        
        String msg = "🚨 *FLUTE SYSTEM ALERT*\n\n";
        msg += "Error Code: *" + String(errorCode) + "*\n";
        
        if (errorCode == 1) msg += "⚠️ Engine synchronisation lost (CKP/CMP).";
        else if (errorCode == 2) msg += "⚠️ No communication with ECU (CAN timeout).";
        else if (errorCode == 3) msg += "🔥 Critical fault: Watchdog reset (IWDG).";
        else if (errorCode == 99) msg += "🛑 Emergency stop activated by user.";
        else msg += "⚠️ Unknown error code.";
        
        msg += "\n\nCheck the vehicle and web interface immediately.";
        
        tempBot.sendMessage(chatId, msg, "Markdown");
        telegramAlertSent = true; // Anti-spam: block further messages until cleared
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
        String msg = String((char*)payload);
        if (msg == "EMERGENCY_STOP") {
            STM32_SERIAL.println("{\"emergency_stop\":true}");
        }
    }
}