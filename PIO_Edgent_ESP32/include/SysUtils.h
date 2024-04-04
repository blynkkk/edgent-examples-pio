#pragma once

#include <Preferences.h>
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

static char BASE64[65] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

class Base64Writer
  : public Print
{
public:

  Base64Writer(Print &stream) : _stream(stream) {}

  void setWidth(int w) { _width = w; }
  void setDelay(unsigned t) { _delay = t; }

  ~Base64Writer() {
    flush();
  }

  virtual void flush() override {
    if (!_cur) return;
    convert();
    switch (_cur){
      case 1: _data[2] = '=';
      case 2: _data[3] = '=';
    }
    step();
    _stream.flush();
  }

  virtual size_t write(uint8_t b) override {
    if (_cur == 3) {
      convert();
      step();
    }
    _data[_cur++] = b;
    return 1;
  }

  using Print::write;

protected:

  void step() {
    _stream.write(_data, 4);
    memset(_data, 0, 4);
    _cur = 0;
    if (_width) {
      _col += 4;
      if (_col >= _width) {
        _stream.write('\n');
        _col = 0;
        if (_delay) { delay(_delay); };
      }
    }
  }
  void convert() {
    union{
      uint8_t input[3];
      struct {
        unsigned int D : 0x06;
        unsigned int C : 0x06;
        unsigned int B : 0x06;
        unsigned int A : 0x06;
      } output;
    } B64C = { { _data[2], _data[1], _data[0] } };

    _data[0] = pgm_read_byte(&BASE64[B64C.output.A]);
    _data[1] = pgm_read_byte(&BASE64[B64C.output.B]);
    _data[2] = pgm_read_byte(&BASE64[B64C.output.C]);
    _data[3] = pgm_read_byte(&BASE64[B64C.output.D]);
  }

  uint8_t   _data[4];
  Print&    _stream;
  char      _cur = 0;
  int       _width = 0, _col = 0;
  unsigned  _delay = 0;

};

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
  const esp_partition_t* lfs_pt = esp_partition_find_first(
                ESP_PARTITION_TYPE_DATA,
                ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
  if (lfs_pt) {
    LittleFS.begin(true, "/lfs", 5, lfs_pt->label);
  }
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
bool systemHasCoreDump()
{
  size_t size = 0;
  size_t address = 0;
  return (esp_core_dump_image_get(&address, &size) == ESP_OK);
}

static
void systemPrintCoreDump(Stream& stream)
{
  size_t size = 0;
  size_t address = 0;
  if (esp_core_dump_image_get(&address, &size) == ESP_OK) {
    const esp_partition_t* pt = NULL;
    pt = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, "coredump");
    if (pt) {
      uint8_t bf[256];
      Base64Writer b64(stream);
      b64.setWidth(120);
      b64.setDelay(10);

      stream.println(F("================= CORE DUMP START ================="));
      for (int16_t i = 0; i < (size / 256) + 1; i++) {
        int16_t toRead = (size - i * 256) > 256 ? 256 : (size - i * 256);
        esp_err_t er = esp_partition_read(pt, i * 256, bf, toRead);
        if (er == ESP_OK) {
          b64.write(bf, 256);
        } else {
          stream.printf("FAIL [%x]\n",er);
        }
      }
      b64.flush();
      stream.println(F("\n================= CORE DUMP END ==================="));
    } else {
      stream.println(F("Partition NULL"));
    }
  } else {
    stream.println(F("No coredump found"));
  }
}

static
void systemClearCoreDump()
{
  esp_core_dump_image_erase();
}

