#pragma once

#include <Preferences.h>

extern "C" {
  #include "esp_partition.h"
  #include "esp_ota_ops.h"
  #include "esp_core_dump.h"
  #include "esp_task_wdt.h"

  #ifdef ESP_IDF_VERSION_MAJOR // IDF 4+
  #if CONFIG_IDF_TARGET_ESP32 // ESP32/PICO-D4
  #include "esp32/rom/rtc.h"
  #elif CONFIG_IDF_TARGET_ESP32S2
  #include "esp32s2/rom/rtc.h"
  #elif CONFIG_IDF_TARGET_ESP32C3
  #include "esp32c3/rom/rtc.h"
  #elif CONFIG_IDF_TARGET_ESP32S3
  #include "esp32s3/rom/rtc.h"
  #else
  #error Target CONFIG_IDF_TARGET is not supported
  #endif
  #else // ESP32 Before IDF 4.0
  #include "rom/rtc.h"
  #endif
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
  static constexpr char alphabet[] = { "0W8N4Y1HP5DF9K6JM3C2UA7R" };
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
String systemGetDeviceName(bool withPrefix = true)
{
  String devUnique = getDeviceRandomSuffix(4);
  String devPrefix = CONFIG_DEVICE_PREFIX;
  const int maxTmplName = 31 - (2 + devPrefix.length() + devUnique.length());
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

// This generates a random ID.
// Note: It will change each time device flash is erased.
static inline
String systemGetDeviceUID() {
  static String result;
  if (!result.length()) {
    Preferences prefs;
    if (!prefs.begin("system")) {
      return "none";
    }
    if (prefs.isKey("uid")) {
      result = prefs.getString("uid");
    }
    if (!result.length()) {
      // Generate and store unique Device ID
      union {
        uint32_t dwords[3];
        uint8_t  bytes[12];
      } uid;
#if defined(ESP32)
      uid.dwords[0] = esp_random();
      uid.dwords[1] = esp_random();
      uid.dwords[2] = esp_random();
#elif defined(ESP8266)
      uid.dwords[0] = ESP.random();
      uid.dwords[1] = ESP.random();
      uid.dwords[2] = ESP.random();
#elif defined(ARDUINO_ARCH_AMEBAD)
      rtw_get_random_bytes(uid.bytes, sizeof(uid.bytes));
#endif

      char str[32];
      const uint8_t* id = uid.bytes;
      snprintf(str, sizeof(str),
               "%02x%02x%02x%02x-%02x%02x%02x%02x-%02x%02x%02x%02x",
               id[0], id[1],  id[2],  id[3],
               id[4], id[5],  id[6],  id[7],
               id[8], id[9], id[10], id[11]);
      result = str;
      if (!prefs.putString("uid", result)) {
        result = "";
        return "none";
      }
    }
  }
  return result;
}

__NOINIT_ATTR
//RTC_NOINIT_ATTR
class SysNoInit {
public:
  struct {
    uint32_t total;
    uint32_t graceful;
  } resetCount;

public:
  SysNoInit() {
    if (_magic != MAGIC) {
      memset(this, 0, sizeof(SysNoInit));
      _magic = MAGIC;
    } else {
      resetCount.total++;
    }
  }

private:
  static const uint32_t MAGIC = 0x2f5385a4;
  uint32_t _magic;
} systemNoInitData;

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
  systemNoInitData.resetCount.graceful++;
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
  int reason = rtc_get_reset_reason(0);
  switch (reason) {
    case 1 : return "POWERON_RESET"; break;          /**<1,  Vbat power on reset*/
    case 3 : return "SW_RESET"; break;               /**<3,  Software reset digital core*/
    case 4 : return "OWDT_RESET"; break;             /**<4,  Legacy watch dog reset digital core*/
    case 5 : return "DEEPSLEEP_RESET"; break;        /**<5,  Deep Sleep reset digital core*/
    case 6 : return "SDIO_RESET"; break;             /**<6,  Reset by SLC module, reset digital core*/
    case 7 : return "TG0WDT_SYS_RESET"; break;       /**<7,  Timer Group0 Watch dog reset digital core*/
    case 8 : return "TG1WDT_SYS_RESET"; break;       /**<8,  Timer Group1 Watch dog reset digital core*/
    case 9 : return "RTCWDT_SYS_RESET"; break;       /**<9,  RTC Watch dog Reset digital core*/
    case 10 : return "INTRUSION_RESET"; break;       /**<10, Instrusion tested to reset CPU*/
    case 11 : return "TGWDT_CPU_RESET"; break;       /**<11, Time Group reset CPU*/
    case 12 : return "SW_CPU_RESET"; break;          /**<12, Software reset CPU*/
    case 13 : return "RTCWDT_CPU_RESET"; break;      /**<13, RTC Watch dog Reset CPU*/
    case 14 : return "EXT_CPU_RESET"; break;         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : return "RTCWDT_BROWN_OUT_RESET"; break;/**<15, Reset when the vdd voltage is not stable*/
    case 16 : return "RTCWDT_RTC_RESET"; break;      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : return "<unknown>";
  }
}

static inline
String systemGetFlashMode() {
  switch (ESP.getFlashChipMode()) {
    case FM_QIO:       return "QIO";
    case FM_QOUT:      return "QOUT";
    case FM_DIO:       return "DIO";
    case FM_DOUT:      return "DOUT";
    case FM_FAST_READ: return "FAST_READ";
    case FM_SLOW_READ: return "SLOW_READ";
    case FM_UNKNOWN:   return "UNKNOWN";
    default :          return "<unknown>";
  }
}

static
void systemPrintCoreDump(Stream& stream)
{
  size_t size = 0;
  size_t address = 0;
  if (esp_core_dump_image_get(&address, &size) == ESP_OK)
  {
    const esp_partition_t *pt = NULL;
    pt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");

    if (pt != NULL)
    {
      uint8_t bf[256];
      char str_dst[640];
      int16_t toRead;

      for (int16_t i = 0; i < (size / 256) + 1; i++)
      {
        strcpy(str_dst, "");
        toRead = (size - i * 256) > 256 ? 256 : (size - i * 256);

        esp_err_t er = esp_partition_read(pt, i * 256, bf, toRead);
        if (er != ESP_OK)
        {
          stream.printf("FAIL [%x]\n",er);
          break;
        }

        for (int16_t j = 0; j < 256; j++)
        {
          char str_tmp[3];

          sprintf(str_tmp, "%02x", bf[j]);
          strcat(str_dst, str_tmp);
        }

        printf("%s", str_dst);
      }
    }
    else
    {
      stream.println("Partition NULL");
    }
    esp_core_dump_image_erase();
  }
  else
  {
    stream.println("esp_core_dump_image_get() FAIL");
  }
}

