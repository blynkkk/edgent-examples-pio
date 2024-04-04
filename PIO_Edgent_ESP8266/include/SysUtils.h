#pragma once

#include <Print.h>

#if defined(BLYNK_USE_LITTLEFS)
  #include <LittleFS.h>
  #define BLYNK_FS LittleFS
#elif defined(BLYNK_USE_SPIFFS) && defined(ESP32)
  #include <SPIFFS.h>
  #define BLYNK_FS SPIFFS
#elif defined(BLYNK_USE_SPIFFS) && defined(ESP8266)
  #include <FS.h>
  #define BLYNK_FS SPIFFS
#endif

#if defined(BLYNK_FS) && defined(ESP8266)
  #define FILE_READ  "r"
  #define FILE_WRITE "w"
#endif

#if defined(BLYNK_NOINIT_ATTR)
  // OK, use it
#elif defined(ESP32)
  #define BLYNK_NOINIT_ATTR __NOINIT_ATTR //RTC_NOINIT_ATTR
#elif defined(ESP8266) || defined(ARDUINO_ARCH_SAMD)
  #define BLYNK_NOINIT_ATTR __attribute__((section(".noinit")))
#else
  #error "BLYNK_NOINIT_ATTR is not defined"
#endif

extern "C" {
  #include "user_interface.h"
}

static inline
String timeSpanToStr(const uint64_t t) {
  unsigned secs = t % 60;
  unsigned mins = (t / 60) % 60;
  unsigned hrs  = (t % (24UL*3600UL)) / 3600UL;
  unsigned days = (t / (24UL*3600UL));

  char buff[32];
  snprintf(buff, sizeof(buff), "%dd %dh %dm %ds", days, hrs, mins, secs);
  return buff;
}

static inline
String encodeUniquePart(uint32_t n, unsigned len)
{
  static constexpr char alphabet[] = { "0W8N4Y1HP5DF9K6JM3C2XA7R" };
  static constexpr int base = sizeof(alphabet)-1;

  char buf[16] = { 0, };
  char prev = 0;
  for (unsigned i = 0; i < len; n /= base) {
    char c = alphabet[n % base];
    if (c == prev) {
      c = alphabet[(n+1) % base];
    }
    prev = buf[i++] = c;
  }
  return String(buf);
}

static inline
String getDeviceRandomSuffix(unsigned size)
{
  static uint32_t unique = 0;
  static bool hasUnique = false;
  if (!hasUnique) {
#if defined(ESP32)
    const uint64_t chipId = ESP.getEfuseMac();
#elif defined(ESP8266) || defined(ARDUINO_ARCH_AMEBAD)
    uint8_t chipId[6] = { 0, };
    WiFi.macAddress(chipId);
#elif defined(PARTICLE)
    char chipId[24];
    String deviceID = System.deviceID();
    strncpy(chipId, deviceID.c_str(), sizeof(chipId));
#elif defined(ARDUINO_ARCH_SAMD)
    const uint32_t chipId[4] = {
      SERIAL_NUMBER_WORD_0,
      SERIAL_NUMBER_WORD_1,
      SERIAL_NUMBER_WORD_2,
      SERIAL_NUMBER_WORD_3
    };
#elif defined(ARDUINO_ARCH_RP2040)
    pico_unique_board_id_t chipId;
    pico_get_unique_board_id(&chipId);

#elif defined(ARDUINO_ARCH_NRF5)
    const uint32_t chipId[2] = {
      NRF_FICR->DEVICEID[1],
      NRF_FICR->DEVICEID[0]
    };
#else
    #warning "Platform not supported"
    const uint32_t chipId = 0;
    return "0000";
#endif

    for (int i=0; i<4; i++) {
      unique = BlynkCRC32(&chipId, sizeof(chipId), unique);
    }
    hasUnique = true;
  }
  return encodeUniquePart(unique, size);
}

static
void systemInit()
{
#if defined(BLYNK_USE_LITTLEFS)
  LittleFS.begin();
#elif defined(BLYNK_USE_SPIFFS)
  SPIFFS.begin(true);
#endif
}

static
String systemGetDeviceName(bool withPrefix = true)
{
  String devUnique = getDeviceRandomSuffix(4);
  String devPrefix = CONFIG_DEVICE_PREFIX;
  const int maxTmplName = 29 - (2 + devPrefix.length() + devUnique.length());
  String devName = String(BLYNK_TEMPLATE_NAME).substring(0, maxTmplName);
  if (withPrefix) {
    if (devName.length()) {
      return devPrefix + " " + devName + "-" + devUnique;
    } else {
      return devPrefix + "-" + devUnique;
    }
  } else {
    if (devName.length()) {
      return devName + "-" + devUnique;
    } else {
      return devUnique;
    }
  }
}

BLYNK_NOINIT_ATTR
class SystemStats {
public:
  struct {
    uint32_t total;
    uint32_t graceful;
  } resetCount;

public:
  SystemStats() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
    if (_magic != expectedMagic()) {
      clear();
    } else {
      resetCount.total++;
    }
#pragma GCC diagnostic pop
  }

  void clear() {
    memset(this, 0, sizeof(SystemStats));
    _magic = expectedMagic();
  }

private:
  static uint32_t expectedMagic() {
    return (MAGIC + __LINE__ + sizeof(SystemStats));
  }
  static const uint32_t MAGIC = 0x2f5385a4;
  uint32_t _magic;
} systemStats;

static inline
uint64_t systemUptime() {
#if defined(ESP32)
  return esp_timer_get_time() / 1000;
#elif defined(ESP8266)
  return micros64() / 1000;
#elif defined(PARTICLE)
  return System.millis();
#else
  // TODO: track overflow
  return millis();
#endif
}

static inline
void systemReboot() {
  systemStats.resetCount.graceful++;
  delay(50);
#if defined(ESP32) || defined(ESP8266)
  ESP.restart();
#elif defined(ARDUINO_ARCH_SAMD) || \
      defined(ARDUINO_ARCH_SAM)  || \
      defined(ARDUINO_ARCH_NRF5) || \
      defined(ARDUINO_ARCH_AMEBAD)
  NVIC_SystemReset();
#elif defined(ARDUINO_ARCH_RP2040)
  rp2040.reboot();
#elif defined(PARTICLE)
  System.reset();
#else
  #error "Platform not supported"
#endif
  while(1) {};
}

static inline
String systemGetResetReason() {
  return ESP.getResetReason();
}

static inline
String systemGetFlashMode() {
  switch (ESP.getFlashChipMode()) {
    case FM_QIO:       return "QIO";
    case FM_QOUT:      return "QOUT";
    case FM_DIO:       return "DIO";
    case FM_DOUT:      return "DOUT";
    case FM_UNKNOWN:   return "UNKNOWN";
    default :          return "<unknown>";
  }
}

