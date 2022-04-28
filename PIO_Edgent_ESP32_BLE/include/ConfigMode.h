
#include <ConfigNimBLE.h>
//#include <ConfigBLE.h>

#include <ArduinoJson.h>

static int connectNetRetries    = WIFI_CLOUD_MAX_RETRIES;
static int connectBlynkRetries  = WIFI_CLOUD_MAX_RETRIES;

void restartMCU() {
  ESP.restart();
  while(1) {};
}

void getWiFiName(char* buff, size_t len, bool withPrefix = true) {
  const uint64_t chipId = ESP.getEfuseMac();
  uint32_t unique = 0;
  for (int i=0; i<4; i++) {
    unique = BlynkCRC32(&chipId, sizeof(chipId), unique);
  }
  unique &= 0xFFFFF;

  String devPrefix = CONFIG_DEVICE_PREFIX;
  String devName = String(BLYNK_DEVICE_NAME).substring(0, 29-7-devPrefix.length());

  if (withPrefix) {
    snprintf(buff, len, "%s %s-%05X", devPrefix.c_str(), devName.c_str(), unique);
  } else {
    snprintf(buff, len, "%s-%05X", devName.c_str(), unique);
  }
}

void enterConfigMode()
{
  char ssidBuff[64];
  getWiFiName(ssidBuff, sizeof(ssidBuff));

  WiFi.mode(WIFI_OFF);
  delay(100);
  
  ConfigBLE server;

  server.begin(ssidBuff);
  StaticJsonDocument<256> json;

  String ssid, pass, token, host, port;
  String ip, mask, gw, dns, dns2;
  bool forceSave = false;

  while (BlynkState::is(MODE_WAIT_CONFIG) || BlynkState::is(MODE_CONFIGURING)) {
    delay(10);
    app_loop();
    if (server.available()) {
      String cmd = server.read();
      DeserializationError error = deserializeJson(json, cmd);
      if (error) continue;
      String t = json["t"];
      if (t == "set") {
        bool foundInvalid = false;
        JsonObject obj = json.as<JsonObject>();
        for (JsonPair item : obj) {
          const JsonString& key = item.key();
          if      (key == "t")      { /* skip */ }
          else if (key == "ssid")   { ssid  = item.value().as<String>(); }
          else if (key == "pass")   { pass  = item.value().as<String>(); }
          else if (key == "blynk")  { token = item.value().as<String>(); }
          else if (key == "host")   { host  = item.value().as<String>(); }
          else if (key == "port")   { port  = item.value().as<String>(); }
          else if (key == "ip")     { ip    = item.value().as<String>(); }
          else if (key == "mask")   { mask  = item.value().as<String>(); }
          else if (key == "gw")     { gw    = item.value().as<String>(); }
          else if (key == "dns")    { dns   = item.value().as<String>(); }
          else if (key == "dns2")   { dns2  = item.value().as<String>(); }
          else if (key == "save")   { forceSave = true; }
          else                      { foundInvalid = true; }
        }
        if (!foundInvalid) {
          server.write(R"json({"t":"set_ok"})json");
        } else {
          server.write(R"json({"t":"set_fail"})json");
        }
      } else if (t == "connect") {
        DEBUG_PRINT("Applying configuration...");
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
          }

          server.write(R"json({"t":"connecting"})json");

          connectNetRetries = connectBlynkRetries = 1;
          BlynkState::set(MODE_SWITCH_TO_STA);
        } else {
          DEBUG_PRINT("Configuration invalid");
          server.write(R"json({"t":"connect_fail","msg":"Configuration invalid"})json");
        }
      } else if (t == "info") {
        // Configuring starts with board info request (may impact indication)
        BlynkState::set(MODE_CONFIGURING);

        DEBUG_PRINT("Sending board info...");
        const char* tmpl = BLYNK_TEMPLATE_ID;
        char ssidBuff[64];
        getWiFiName(ssidBuff, sizeof(ssidBuff));
        char buff[255];
        int len = snprintf(buff, sizeof(buff),
          R"json({"t":"info","tmpl_id":"%s","fw_type":"%s","fw_ver":"%s","ssid":"%s","mac":"%s","last_error":%d,"wifi_scan":1,"static_ip":1})json",
          tmpl ? tmpl : "Unknown",
          BLYNK_FIRMWARE_TYPE,
          BLYNK_FIRMWARE_VERSION,
          ssidBuff,
          WiFi.macAddress().c_str(),
          configStore.last_error
        );
        server.write(buff, len);
      } else if (t == "scan") {
        DEBUG_PRINT("Scanning networks...");

        server.write(R"json({"t":"scan_start"})json");

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

          char buff[128];
          for (int i = 0; i < wifi_nets; i++) {
            int id = indices[i];

            const char* sec;
            switch (WiFi.encryptionType(id)) {
            case WIFI_AUTH_WEP:          sec = "WEP"; break;
            case WIFI_AUTH_WPA_PSK:      sec = "WPA/PSK"; break;
            case WIFI_AUTH_WPA2_PSK:     sec = "WPA2/PSK"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: sec = "WPA/WPA2/PSK"; break;
            case WIFI_AUTH_OPEN:         sec = "OPEN"; break;
            default:                     sec = "unknown"; break;
            }

            int len = snprintf(buff, sizeof(buff),
              R"json({"t":"scan","ssid":"%s","bssid":"%s","rssi":%i,"sec":"%s","ch":%i})json",
              WiFi.SSID(id).c_str(),
              WiFi.BSSIDstr(id).c_str(),
              WiFi.RSSI(id),
              sec,
              WiFi.channel(id)
            );
            server.write(buff, len);
            delay(10);
          }
        } // wifi_nets > 0

        server.write(R"json({"t":"scan_end"})json");

      } else if (t == "reset") {
        BlynkState::set(MODE_RESET_CONFIG);
        server.write(R"json({"t":"reset_ok"})json");
      } else if (t == "reboot") {
        restartMCU();
      }
    } // server.available

    if (BlynkState::is(MODE_CONFIGURING) && !server.isConnected()) {
      BlynkState::set(MODE_WAIT_CONFIG);
    }
  }

}

void enterConnectNet() {
  BlynkState::set(MODE_CONNECTING_NET);
  DEBUG_PRINT(String("Connecting to WiFi: ") + configStore.wifiSSID);

  char ssidBuff[64];
  getWiFiName(ssidBuff, sizeof(ssidBuff));
  String hostname(ssidBuff);
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

    if (!configStore.getFlag(CONFIG_FLAG_VALID)) {
      configStore.last_error = BLYNK_PROV_ERR_NONE;
      configStore.setFlag(CONFIG_FLAG_VALID, true);
      config_save();
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
  delay(10);

  restartMCU();
}

