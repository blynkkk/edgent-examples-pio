
#include <WiFiClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>

static const char configForm[] PROGMEM = R"html(
<!DOCTYPE HTML>
<html><head>
  <title>WiFi setup</title>
  <style>
  body {
    background-color: #fcfcfc;
    box-sizing: border-box;
  }
  body, input {
    font-family: Roboto, sans-serif;
    font-weight: 400;
    font-size: 16px;
  }
  .centered {
    position: fixed;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);

    padding: 20px;
    background-color: #ccc;
    border-radius: 4px;
  }
  td { padding:0 0 0 5px; }
  label { white-space:nowrap; }
  input { width: 20em; }
  input[name="port"] { width: 5em; }
  input[type="submit"], img { margin: auto; display: block; width: 30%; }
  </style>
</head><body>
<div class="centered">
  <form method="get" action="config">
    <input type="hidden" name="if" value="wifi">
    <table>
    <tr><td><label for="ssid">WiFi network:</label></td>  <td><input type="text" name="ssid" maxlength=64 required="required"></td></tr>
    <tr><td><label for="pass">WiFi password:</label></td> <td><input type="text" name="pass" maxlength=64></td></tr>
    <tr><td><label for="blynk">Auth token:</label></td>   <td><input type="text" name="blynk" placeholder="a0b1c2d..." pattern="[-_a-zA-Z0-9]{32}" minlength="32" maxlength="32" required="required"></td></tr>
    <tr><td><label for="host">Server:</label></td>        <td><input type="text" name="host" value="" maxlength=64 required="required"></td></tr>
    </table><br/>
    <input type="submit" value="Apply">
  </form>
</div>
</body></html>
)html";

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

static int connectNetRetries    = WIFI_CLOUD_MAX_RETRIES;
static int connectBlynkRetries  = WIFI_CLOUD_MAX_RETRIES;

static const char serverUpdateForm[] PROGMEM = R"html(
<html><body>
  <form method='POST' action='' enctype='multipart/form-data'>
    <input type='file' name='update'>
    <input type='submit' value='Update'>
  </form>
</body></html>
)html";

static inline
String macToString(byte mac[6]) {
  char buff[20];
  snprintf(buff, sizeof(buff), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buff);
}

static inline
const char* wifiSecToStr(wifi_auth_mode_t t) {
  switch (t) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA+WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2+WPA3";
    case WIFI_AUTH_WAPI_PSK:        return "WAPI";
#endif
    default:                        return "unknown";
  }
}

static
String getWiFiMacAddress() {
  return WiFi.macAddress();
}

static
String getWiFiApBSSID() {
  return WiFi.softAPmacAddress();
}

static
String getWiFiNetworkSSID() {
  return WiFi.SSID();
}

static
String getWiFiNetworkBSSID() {
  return WiFi.BSSIDstr();
}

void enterConfigMode()
{
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(2000);
  WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_IP, WIFI_AP_Subnet);
  WiFi.softAP(systemGetDeviceName().c_str());
  delay(500);

  // Set up DNS Server
  dnsServer.setTTL(300); // Time-to-live 300s
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure); // Return code for non-accessible domains
#ifdef WIFI_CAPTIVE_PORTAL_ENABLE
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP()); // Point all to our IP
  server.onNotFound(handleRoot);
#else
  dnsServer.start(DNS_PORT, CONFIG_AP_URL, WiFi.softAPIP());
  DEBUG_PRINT(String("AP URL:  ") + CONFIG_AP_URL);
#endif

  server.on("/update", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverUpdateForm);
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    if (!Update.hasError()) {
      server.send(200, "text/plain", "OK");
    } else {
      server.send(500, "text/plain", "FAIL");
    }
    edgentTimer.setTimeout(1000, systemReboot);
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      DEBUG_PRINT(String("Update: ") + upload.filename);
      //WiFiUDP::stop();

      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        DEBUG_PRINT(Update.errorString());
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        DEBUG_PRINT(Update.errorString());
      }
#ifdef BLYNK_PRINT
      BLYNK_PRINT.print(".");
#endif
    } else if (upload.status == UPLOAD_FILE_END) {
#ifdef BLYNK_PRINT
      BLYNK_PRINT.println();
#endif
      DEBUG_PRINT("Finishing...");
      if (Update.end(true)) { //true to set the size to the current progress
        DEBUG_PRINT("Update Success. Rebooting");
      } else {
        DEBUG_PRINT(Update.errorString());
      }
    }
  });
  server.on("/config", []() {
    DEBUG_PRINT("Applying configuration...");
    String ssid = server.arg("ssid");
    String ssidManual = server.arg("ssidManual");
    String pass = server.arg("pass");
    if (ssidManual != "") {
      ssid = ssidManual;
    }
    String token = server.arg("blynk");
    String host  = server.arg("host");
    String port  = server.arg("port_ssl");

    String ip   = server.arg("ip");
    String mask = server.arg("mask");
    String gw   = server.arg("gw");
    String dns  = server.arg("dns");
    String dns2 = server.arg("dns2");

    bool forceSave  = server.arg("save").toInt();

    String content;

    DEBUG_PRINT(String("WiFi SSID: ") + ssid + " Pass: " + pass);
    DEBUG_PRINT(String("Blynk cloud: ") + token + " @ " + host + ":" + port);

    if (token.length() == 32 && ssid.length() > 0) {
      configStore = configDefault;
      CopyString(ssid, configStore.wifiSSID);
      CopyString(pass, configStore.wifiPass);
      CopyString(token, configStore.cloudToken);
      if (host.length()) {
        CopyString(host,  configStore.cloudHost);
      }
      if (port.length()) {
        configStore.cloudPort = port.toInt();
      }

      IPAddress addr;

      if (ip.length() && addr.fromString(ip)) {
        configStore.staticIP = addr;
        configStore.setFlag(CONFIG_FLAG_STATIC_IP, true);
      } else {
        configStore.setFlag(CONFIG_FLAG_STATIC_IP, false);
      }
      if (mask.length() && addr.fromString(mask)) {
        configStore.staticMask = addr;
      }
      if (gw.length() && addr.fromString(gw)) {
        configStore.staticGW = addr;
      }
      if (dns.length() && addr.fromString(dns)) {
        configStore.staticDNS = addr;
      }
      if (dns2.length() && addr.fromString(dns2)) {
        configStore.staticDNS2 = addr;
      }

      if (forceSave) {
        configStore.setFlag(CONFIG_FLAG_VALID, true);
        config_save();

        content = R"json({"status":"ok","msg":"Configuration saved"})json";
      } else {
        content = R"json({"status":"ok","msg":"Trying to connect..."})json";
      }
      server.send(200, "application/json", content);

      connectNetRetries = connectBlynkRetries = 1;
      BlynkState::set(MODE_SWITCH_TO_STA);
    } else {
      DEBUG_PRINT("Configuration invalid");
      content = R"json({"status":"error","msg":"Configuration invalid"})json";
      server.send(500, "application/json", content);
    }
  });
  server.on("/board_info.json", []() {
    // Configuring starts with board info request (may impact indication)
    BlynkState::set(MODE_CONFIGURING);

    DEBUG_PRINT("Sending board info...");
    const char* tmpl = BLYNK_TEMPLATE_ID;

    char buff[512];
    snprintf(buff, sizeof(buff),
      R"json({"board":"%s","tmpl_id":"%s","fw_type":"%s","fw_ver":"%s","uid":"%s","ssid":"%s","bssid":"%s","mac":"%s","last_error":%d,"wifi_scan":true,"static_ip":true})json",
      BLYNK_TEMPLATE_NAME,
      tmpl ? tmpl : "Unknown",
      BLYNK_FIRMWARE_TYPE,
      BLYNK_FIRMWARE_VERSION,
      systemGetDeviceUID().c_str(),
      systemGetDeviceName().c_str(),
      getWiFiApBSSID().c_str(),
      getWiFiMacAddress().c_str(),
      configStore.last_error
    );
    server.send(200, "application/json", buff);
  });
  server.on("/wifi_scan.json", []() {
    DEBUG_PRINT("Scanning networks...");
    int wifi_nets = WiFi.scanNetworks(true, true);
    const uint32_t t = millis();
    while (wifi_nets < 0 &&
           millis() - t < 20000)
    {
      delay(20);
      wifi_nets = WiFi.scanComplete();
    }
    DEBUG_PRINT(String("Found networks: ") + wifi_nets);

    if (wifi_nets > 0) {
      // Sort networks
      int indices[wifi_nets];
      for (int i = 0; i < wifi_nets; i++) {
        indices[i] = i;
      }
      for (int i = 0; i < wifi_nets; i++) {
        for (int j = i + 1; j < wifi_nets; j++) {
          if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
            std::swap(indices[i], indices[j]);
          }
        }
      }

      wifi_nets = BlynkMin(15, wifi_nets); // Show top 15 networks

      // TODO: skip empty names
      String result = "[\n";

      char buff[256];
      for (int i = 0; i < wifi_nets; i++){
        int id = indices[i];

        String ssid = WiFi.SSID(id);
        ssid.replace("\"", "\\\"");

        snprintf(buff, sizeof(buff),
          R"json(  {"ssid":"%s","bssid":"%s","rssi":%i,"sec":"%s","ch":%i})json",
          ssid.c_str(),
          WiFi.BSSIDstr(id).c_str(),
          WiFi.RSSI(id),
          wifiSecToStr(WiFi.encryptionType(id)),
          WiFi.channel(id)
        );

        result += buff;
        if (i != wifi_nets-1) result += ",\n";
      }
      WiFi.scanDelete();
      server.send(200, "application/json", result + "\n]");
    } else {
      server.send(200, "application/json", "[]");
    }
  });
  server.on("/reset", []() {
    BlynkState::set(MODE_RESET_CONFIG);
    server.send(200, "application/json", R"json({"status":"ok","msg":"Configuration reset"})json");
  });
  server.on("/reboot", []() {
    edgentTimer.setTimeout(50, systemReboot);
    server.send(200, "application/json", R"json({"status":"ok","msg":"Rebooting"})json");
  });

#ifdef BLYNK_FS
  if (BLYNK_FS.exists("/index.html")) {
    server.serveStatic("/img/favicon.png", BLYNK_FS, "/img/favicon.png");
    server.serveStatic("/img/logo.png", BLYNK_FS, "/img/logo.png");
    server.serveStatic("/", BLYNK_FS, "/index.html");
  } else
#endif
  { /* if no BLYNK_FS or index.html not found */
    server.on("/", []() {
      server.send(200, "text/html", configForm);
    });
  }

  server.begin();

  while (BlynkState::is(MODE_WAIT_CONFIG) || BlynkState::is(MODE_CONFIGURING)) {
    delay(10);
    dnsServer.processNextRequest();
    server.handleClient();
    app_loop();
    if (BlynkState::is(MODE_CONFIGURING) && WiFi.softAPgetStationNum() == 0) {
      BlynkState::set(MODE_WAIT_CONFIG);
    }
  }

  server.stop();
}

void enterConnectNet() {
  BlynkState::set(MODE_CONNECTING_NET);
  DEBUG_PRINT(String("Connecting to WiFi: ") + configStore.wifiSSID);

  // Needed for setHostname to work
  WiFi.enableSTA(false);

  String hostname = systemGetDeviceName();
  hostname.replace(" ", "-");
  WiFi.setHostname(hostname.c_str());

  if (configStore.getFlag(CONFIG_FLAG_STATIC_IP)) {
    if (!WiFi.config(configStore.staticIP,
                    configStore.staticGW,
                    configStore.staticMask,
                    configStore.staticDNS,
                    configStore.staticDNS2)
    ) {
      DEBUG_PRINT("Failed to configure Static IP");
      config_set_last_error(BLYNK_PROV_ERR_CONFIG);
      BlynkState::set(MODE_ERROR);
      return;
    }
  }

  WiFi.begin(configStore.wifiSSID, configStore.wifiPass);

  unsigned long timeoutMs = millis() + WIFI_NET_CONNECT_TIMEOUT;
  while ((timeoutMs > millis()) && (WiFi.status() != WL_CONNECTED))
  {
    delay(10);
    app_loop();

    if (!BlynkState::is(MODE_CONNECTING_NET)) {
      WiFi.disconnect();
      return;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    IPAddress localip = WiFi.localIP();
    if (configStore.getFlag(CONFIG_FLAG_STATIC_IP)) {
      BLYNK_LOG_IP("Using Static IP: ", localip);
    } else {
      BLYNK_LOG_IP("Using Dynamic IP: ", localip);
    }

    connectNetRetries = WIFI_CLOUD_MAX_RETRIES;
    BlynkState::set(MODE_CONNECTING_CLOUD);
  } else if (--connectNetRetries <= 0) {
    config_set_last_error(BLYNK_PROV_ERR_NETWORK);
    BlynkState::set(MODE_ERROR);
  }
}

void enterConnectCloud() {
  BlynkState::set(MODE_CONNECTING_CLOUD);

  Blynk.config(configStore.cloudToken, configStore.cloudHost, configStore.cloudPort);
  Blynk.connect(0);

  unsigned long timeoutMs = millis() + WIFI_CLOUD_CONNECT_TIMEOUT;
  while ((timeoutMs > millis()) &&
        (WiFi.status() == WL_CONNECTED) &&
        (!Blynk.isTokenInvalid()) &&
        (Blynk.connected() == false))
  {
    delay(10);
    Blynk.run();
    app_loop();
    if (!BlynkState::is(MODE_CONNECTING_CLOUD)) {
      Blynk.disconnect();
      return;
    }
  }

  if (millis() > timeoutMs) {
    DEBUG_PRINT("Timeout");
  }

  if (Blynk.isTokenInvalid()) {
    config_set_last_error(BLYNK_PROV_ERR_TOKEN);
    BlynkState::set(MODE_WAIT_CONFIG); // TODO: retry after timeout
  } else if (WiFi.status() != WL_CONNECTED) {
    BlynkState::set(MODE_CONNECTING_NET);
  } else if (Blynk.connected()) {
    BlynkState::set(MODE_RUNNING);
    connectBlynkRetries = WIFI_CLOUD_MAX_RETRIES;

    if (0 != strcmp(configStore.version, BLYNK_FIRMWARE_VERSION)) {
      Blynk.logEvent("sys_ota", String("Firmware updated to ") + BLYNK_FIRMWARE_VERSION);
      configStore.setFwVer(BLYNK_FIRMWARE_VERSION);
      configStore.setFlag(CONFIG_FLAG_VALID, false);
    }

    if (!configStore.getFlag(CONFIG_FLAG_VALID)) {
      configStore.last_error = BLYNK_PROV_ERR_NONE;
      configStore.setFlag(CONFIG_FLAG_VALID, true);
      config_save();

      Blynk.sendInternal("meta", "set", "Device UID",   systemGetDeviceUID());
      Blynk.sendInternal("meta", "set", "Hotspot Name", systemGetDeviceName());
      Blynk.sendInternal("meta", "set", "Network",      configStore.wifiSSID);
    }
  } else if (--connectBlynkRetries <= 0) {
    config_set_last_error(BLYNK_PROV_ERR_CLOUD);
    BlynkState::set(MODE_ERROR);
  }
}

void enterSwitchToSTA() {
  BlynkState::set(MODE_SWITCH_TO_STA);

  DEBUG_PRINT("Switching to STA...");

  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);

  BlynkState::set(MODE_CONNECTING_NET);
}

void enterError() {
  BlynkState::set(MODE_ERROR);

  unsigned long timeoutMs = millis() + 10000;
  while (timeoutMs > millis() || g_buttonPressed)
  {
    delay(10);
    app_loop();
    if (!BlynkState::is(MODE_ERROR)) {
      return;
    }
  }
  DEBUG_PRINT("Restarting after error.");
  systemReboot();
}

