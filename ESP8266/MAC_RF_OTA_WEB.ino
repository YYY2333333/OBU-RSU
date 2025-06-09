#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <U8g2lib.h>

// OLED 屏幕设置
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* SCL=*/14, /* SDA=*/2);

// Web 服务器实例
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// AP 模式的 Wi-Fi 设置
const char* apSSID = "ESP8266_MAC_Changer";
const char* apPassword = "12345678";

// MAC 地址映射
uint8_t aggressiveMAC[6] = {0x16, 0x98, 0x12, 0x34, 0x56, 0x44};
uint8_t moderateMAC[6]   = {0x16, 0x12, 0x12, 0x34, 0x56, 0x44};
uint8_t downMAC[6]       = {0x16, 0x13, 0x12, 0x34, 0x56, 0x44};

// 当前状态
uint8_t currentMAC[6];
float currentTxPower = 20.5; // 默认发射功率

// 初始化网页内容
String webpage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP8266 Config</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; }
    button, input { margin: 5px; font-size: 16px; }
    .mac-inputs { display: flex; justify-content: center; gap: 5px; margin: 10px auto; }
    .mac-inputs input { width: 30px; text-align: center; }
    .status { margin-top: 20px; }
  </style>
</head>
<body>
  <h1>ESP8266 MAC & Power Config</h1>
  <button onclick="sendCommand('aggressive')">Set Aggressive MAC</button>
  <button onclick="sendCommand('moderate')">Set Moderate MAC</button>
  <button onclick="sendCommand('down')">Set Down MAC</button>

  <div class="mac-inputs">
    <input id="mac0" maxlength="2" />
    <span>:</span>
    <input id="mac1" maxlength="2" />
    <span>:</span>
    <input id="mac2" maxlength="2" />
    <span>:</span>
    <input id="mac3" maxlength="2" />
    <span>:</span>
    <input id="mac4" maxlength="2" />
    <span>:</span>
    <input id="mac5" maxlength="2" />
  </div>
  <button onclick="setCustomMAC()">Set Custom MAC</button>

  <input id="power-input" type="number" min="0" max="20.5" step="0.5" placeholder="Set Tx Power (0-20.5 dBm)" />
  <button onclick="setTxPower()">Set Tx Power</button>

  <div class="status">
    <h2>Current MAC: <span id="mac-address">Loading...</span></h2>
    <h3>Current Handle: <span id="mac-handle">Loading...</span></h3>
    <h3>Current Tx Power: <span id="tx-power">Loading...</span> dBm</h3>
  </div>

  <script>
    function sendCommand(cmd) {
      fetch(`/${cmd}`).then(response => response.json()).then(data => updateStatus(data));
    }

    function setTxPower() {
      const power = document.getElementById('power-input').value;
      fetch(`/setTxPower?value=${power}`).then(response => response.json()).then(data => updateStatus(data));
    }

    function setCustomMAC() {
      const parts = [];
      for (let i = 0; i < 6; i++) {
        const part = document.getElementById('mac' + i).value.trim().toUpperCase();
        if (!/^[0-9A-F]{2}$/.test(part)) {
          alert("Invalid MAC format. Use 2-digit hex values.");
          return;
        }
        parts.push(part);
      }
      const mac = parts.join(":");
      fetch(`/setCustomMAC?value=${mac}`)
        .then(response => response.json())
        .then(data => updateStatus(data))
        .catch(() => { console.error("Failed to set MAC"); });
    }

    function updateStatus(data) {
      document.getElementById('mac-address').innerText = data.mac;
      document.getElementById('mac-handle').innerText = data.handle;
      document.getElementById('tx-power').innerText = data.txPower;
    }

    window.onload = function() {
      fetch('/getCurrentStatus').then(response => response.json()).then(data => updateStatus(data));
    }
  </script>
</body>
</html>
)rawliteral";

bool setMACAddress(const uint8_t mac[]) {
  WiFi.softAPdisconnect(true);
  delay(100);
  memcpy(currentMAC, mac, 6);
  return wifi_set_macaddr(SOFTAP_IF, (uint8_t*)mac);
}

bool setTxPower(float power) {
  if (power < 0 || power > 20.5) return false;
  currentTxPower = power;
  WiFi.setOutputPower(power);
  return true;
}

void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.drawStr(0, 10, "Current MAC:");
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          currentMAC[0], currentMAC[1], currentMAC[2],
          currentMAC[3], currentMAC[4], currentMAC[5]);
  u8g2.drawStr(0, 25, macStr);

  if (memcmp(currentMAC, aggressiveMAC, 6) == 0) {
    u8g2.drawStr(0, 40, "Aggressive");
  } else if (memcmp(currentMAC, moderateMAC, 6) == 0) {
    u8g2.drawStr(0, 40, "Moderate");
  } else if (memcmp(currentMAC, downMAC, 6) == 0) {
    u8g2.drawStr(0, 40, "Down");
  } else {
    u8g2.drawStr(0, 40, "Custom");
  }

  char txPowerStr[20];
  sprintf(txPowerStr, "Tx Power: %.1f dBm", currentTxPower);
  u8g2.drawStr(0, 55, txPowerStr);

  u8g2.sendBuffer();
}

String getCurrentStatus() {
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
          currentMAC[0], currentMAC[1], currentMAC[2],
          currentMAC[3], currentMAC[4], currentMAC[5]);

  String handle = "Custom";
  if (memcmp(currentMAC, aggressiveMAC, 6) == 0) handle = "Aggressive";
  else if (memcmp(currentMAC, moderateMAC, 6) == 0) handle = "Moderate";
  else if (memcmp(currentMAC, downMAC, 6) == 0) handle = "Down";

  return "{\"mac\": \"" + String(macStr) +
         "\", \"handle\": \"" + handle +
         "\", \"txPower\": \"" + String(currentTxPower) + "\"}";
}

void handleAggressive() {
  if (setMACAddress(aggressiveMAC)) {
    updateDisplay();
    server.send(200, "application/json", getCurrentStatus());
    WiFi.softAP(apSSID, apPassword);
  } else {
    server.send(500, "text/plain", "Failed to set Aggressive MAC");
  }
}

void handleModerate() {
  if (setMACAddress(moderateMAC)) {
    updateDisplay();
    server.send(200, "application/json", getCurrentStatus());
    WiFi.softAP(apSSID, apPassword);
  } else {
    server.send(500, "text/plain", "Failed to set Moderate MAC");
  }
}

void handleDown() {
  if (setMACAddress(downMAC)) {
    updateDisplay();
    server.send(200, "application/json", getCurrentStatus());
    WiFi.softAP(apSSID, apPassword);
  } else {
    server.send(500, "text/plain", "Failed to set Down MAC");
  }
}

void handleSetTxPower() {
  if (server.hasArg("value")) {
    float power = server.arg("value").toFloat();
    if (setTxPower(power)) {
      updateDisplay();
      server.send(200, "application/json", getCurrentStatus());
    } else {
      server.send(400, "text/plain", "Invalid Tx Power Value");
    }
  } else {
    server.send(400, "text/plain", "No Power Value Provided");
  }
}

void handleGetCurrentStatus() {
  server.send(200, "application/json", getCurrentStatus());
}

void handleSetCustomMAC() {
  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "No MAC Value Provided");
    return;
  }

  String macStr = server.arg("value");
  macStr.replace(":", "");
  macStr.toUpperCase();

  if (macStr.length() != 12) {
    server.send(400, "text/plain", "MAC must be 12 hex characters");
    return;
  }

  uint8_t newMAC[6];
  for (int i = 0; i < 6; i++) {
    String byteStr = macStr.substring(i * 2, i * 2 + 2);
    char *endptr;
    long val = strtol(byteStr.c_str(), &endptr, 16);
    if (*endptr != '\0' || val < 0 || val > 255) {
      server.send(400, "text/plain", "Invalid MAC format");
      return;
    }
    newMAC[i] = (uint8_t)val;
  }

  if (setMACAddress(newMAC)) {
    updateDisplay();
    server.send(200, "application/json", getCurrentStatus());
    WiFi.softAP(apSSID, apPassword);
  } else {
    server.send(500, "text/plain", "Failed to set Custom MAC");
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", webpage); });
  server.on("/aggressive", HTTP_GET, handleAggressive);
  server.on("/moderate", HTTP_GET, handleModerate);
  server.on("/down", HTTP_GET, handleDown);
  server.on("/setTxPower", HTTP_GET, handleSetTxPower);
  server.on("/getCurrentStatus", HTTP_GET, handleGetCurrentStatus);
  server.on("/setCustomMAC", HTTP_GET, handleSetCustomMAC);
  httpUpdater.setup(&server, "/update");
  server.begin();
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 10, "Initializing...");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword, 1);

  wifi_get_macaddr(SOFTAP_IF, currentMAC);
  updateDisplay();
  setupWebServer();
}

void loop() {
  server.handleClient();
}
