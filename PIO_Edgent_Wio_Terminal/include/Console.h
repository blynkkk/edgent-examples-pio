
#include <Blynk/BlynkConsole.h>

BlynkConsole    edgentConsole;

void console_init()
{
#ifdef BLYNK_PRINT
  edgentConsole.begin(BLYNK_PRINT);
#endif

  edgentConsole.print("\n>");

  edgentConsole.addCommand("reboot", []() {
    edgentConsole.print(R"json({"status":"OK","msg":"rebooting wifi module"})json" "\n");
    edgentTimer.setTimeout(50, systemReboot);
  });

  edgentConsole.addCommand("devinfo", []() {
    edgentConsole.printf(
        R"json({"name":"%s","board":"%s","tmpl_id":"%s","fw_type":"%s","fw_ver":"%s"})json" "\n",
        systemGetDeviceName().c_str(),
        BLYNK_TEMPLATE_NAME,
        BLYNK_TEMPLATE_ID,
        BLYNK_FIRMWARE_TYPE,
        BLYNK_FIRMWARE_VERSION
    );
  });

  edgentConsole.addCommand("connect", [](int argc, const char** argv) {
    if (argc < 2) {
      edgentConsole.print(R"json({"status":"error","msg":"invalid arguments. expected: <auth> <ssid> <pass>"})json" "\n");
      return;
    }
    String auth = argv[0];
    String ssid = argv[1];
    String pass = (argc >= 3) ? argv[2] : "";

    if (auth.length() != 32) {
      edgentConsole.print(R"json({"status":"error","msg":"invalid token size"})json" "\n");
      return;
    }

    edgentConsole.print(R"json({"status":"OK","msg":"trying to connect..."})json" "\n");

    configStore = configDefault;
    CopyString(ssid, configStore.wifiSSID);
    CopyString(pass, configStore.wifiPass);
    CopyString(auth, configStore.cloudToken);

    BlynkState::set(MODE_SWITCH_TO_STA);
  });

  edgentConsole.addCommand("config", [](int argc, const char** argv) {
    if (argc < 1 || 0 == strcmp(argv[0], "start")) {
      BlynkState::set(MODE_WAIT_CONFIG);
    } else if (0 == strcmp(argv[0], "erase")) {
      BlynkState::set(MODE_RESET_CONFIG);
    } else {
      edgentConsole.getStream().println(F("Available commands: start, erase"));
    }
  });

  edgentConsole.addCommand("wifi", [](int argc, const char* argv[]) {
    if (argc < 1 || 0 == strcmp(argv[0], "show")) {
      edgentConsole.printf(
          "mac:%s ip:%s (%s [%s] %ddBm)\n",
          getWiFiMacAddress().c_str(),
          WiFi.localIP().toString().c_str(),
          getWiFiNetworkSSID().c_str(),
          getWiFiNetworkBSSID().c_str(),
          WiFi.RSSI()
      );
    } else if (0 == strcmp(argv[0], "scan")) {
      int found = WiFi.scanNetworks();
      for (int i = 0; i < found; i++) {
        bool current = (WiFi.SSID(i) == WiFi.SSID());
        edgentConsole.printf(
            "%s %s [%s] %s ch:%d rssi:%d\n",
            (current ? "*" : " "), WiFi.SSID(i).c_str(),
            macToString(WiFi.BSSID(i)).c_str(),
            wifiSecToStr(WiFi.encryptionType(i)),
            WiFi.channel(i), WiFi.RSSI(i)
        );
      }
      WiFi.scanDelete();
    } else {
      edgentConsole.getStream().println(F("Available commands: show, scan"));
    }
  });

  edgentConsole.addCommand("sysinfo", []() {
    edgentConsole.printf(" Uptime:          %s\n",        timeSpanToStr(systemUptime() / 1000).c_str());
    edgentConsole.printf(" Reset graceful:  %lu / %lu\n", systemStats.resetCount.graceful,
                                                          systemStats.resetCount.total);
    edgentConsole.printf(" Stack unused:    %d\n",        uxTaskGetStackHighWaterMark(NULL));
  });

  edgentConsole.addCommand("sys", [](const BlynkParam &param) {
    const String tool = param[0].asStr();
    if (tool == "powersave") {
      const String cmd = param[1].asStr();
      if (!param[1].isValid() || cmd == "show") {
        edgentConsole.printf("WiFi powersave: %s\n", WiFi.getSleep() ? "on" : "off");
      } else if (cmd == "on") {
        WiFi.setSleep(true);
      } else if (cmd == "off") {
        WiFi.setSleep(false);
      }
    } else if (tool == "nodelay") {
      const String cmd = param[1].asStr();
      if (!param[1].isValid() || cmd == "show") {
        edgentConsole.printf("TCP nodelay: %s\n", _blynkWifiClient.getNoDelay() ? "on" : "off");
      } else if (cmd == "on") {
        _blynkWifiClient.setNoDelay(true);
      } else if (cmd == "off") {
        _blynkWifiClient.setNoDelay(false);
      }
    } else if (tool == "drop_stats") {
      systemStats.clear();
    } else {
      edgentConsole.getStream().println(F("Available commands: coredump [show|clear], partitions, powersave [show|on|off], nodelay [show|on|off], cpufreq [show|N(MHz), drop_stats]"));
    }
  });

}

BLYNK_WRITE(InternalPinDBG) {
  String cmd = String(param.asStr()) + "\n";
  edgentConsole.runCommand((char*)cmd.c_str());
}

