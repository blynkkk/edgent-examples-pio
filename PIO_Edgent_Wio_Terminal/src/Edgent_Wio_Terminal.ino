/*
 * Required libraries:
 *  - Seeed Arduino rpcUnified
 *  - Seeed Arduino rpcWiFi
 *  - Seeed Arduino SFUD
 *  - Seeed Arduino FS
 *  - Seeed Arduino mbedtls
 *  - Seeed Arduino FreeRTOS
 *  - ArduinoOTA
 *  - ArduinoHttpClient
 *  
 * Please also update the WiFi module firmware:
 *   https://wiki.seeedstudio.com/Wio-Terminal-Network-Overview
 */

// Fill-in information from your Blynk Template here
//#define BLYNK_TEMPLATE_ID           "TMPLxxxxxx"
//#define BLYNK_DEVICE_NAME           "Device"

#define BLYNK_FIRMWARE_VERSION        "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

void display_update_state();
void display_run();

#include "BlynkEdgent.h"

#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lcd_backlight.hpp"

/*
 * Buzzer
 */

void buzzer_init() {
    pinMode(WIO_BUZZER, OUTPUT);
}

void playTone(int tone, int duration) {
    for (long i = 0; i < duration * 1000L; i += tone * 2) {
        digitalWrite(WIO_BUZZER, HIGH);
        delayMicroseconds(tone);
        digitalWrite(WIO_BUZZER, LOW);
        delayMicroseconds(tone);
    }
}

/*
 * LVGL configuration
 */

const uint16_t SCREEN_WIDTH  = 320;
const uint16_t SCREEN_HEIGHT = 240;

const int BTN_UP        = WIO_5S_UP;
const int BTN_DOWN      = WIO_5S_DOWN;
const int BTN_LEFT      = WIO_5S_LEFT;
const int BTN_RIGHT     = WIO_5S_RIGHT;
const int BTN_OK        = WIO_5S_PRESS;
const int BTN_A         = WIO_KEY_A;
const int BTN_B         = WIO_KEY_B;
const int BTN_C         = WIO_KEY_C;


static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ SCREEN_WIDTH * 16 ];

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT); /* TFT instance */
LCDBackLight backLight;

lv_obj_t *lbl_state, *lbl_rssi, *lbl_ssid, *lbl_ip, *lbl_mac, *btn_reset;
bool soundEnabled = false;

const char* STR_NONE = "---";

static lv_obj_t * create_text(lv_obj_t * parent, const char * icon, const char * txt)
{
    lv_obj_t * obj = lv_menu_cont_create(parent);

    if (icon) {
        lv_obj_t * img = lv_img_create(obj);
        lv_img_set_src(img, icon);
    }

    if (txt) {
        lv_obj_t * label = lv_label_create(obj);
        lv_label_set_text(label, txt);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_flex_grow(label, 1);
    }

    return obj;
}

static lv_obj_t * create_slider(lv_obj_t * parent, const char * icon, const char * txt, int32_t min, int32_t max, int32_t val)
{
    lv_obj_t * obj = create_text(parent, icon, txt);

    lv_obj_t * slider = lv_slider_create(obj);
    lv_obj_set_flex_grow(slider, 1);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);

    if (icon == NULL) {
        //lv_obj_add_flag(slider, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }

    return obj;
}

static lv_obj_t * create_switch(lv_obj_t * parent, const char * icon, const char * txt, bool chk)
{
    lv_obj_t * obj = create_text(parent, icon, txt);

    lv_obj_t * sw = lv_switch_create(obj);
    lv_obj_add_state(sw, chk ? LV_STATE_CHECKED : 0);

    return obj;
}

static lv_obj_t * create_status(lv_obj_t * parent, const char * icon, const char * txt, const char * txt2)
{
    lv_obj_t * obj = create_text(parent, icon, txt);

    lv_obj_t * label = lv_label_create(obj);
    lv_label_set_text(label, txt2);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    return obj;
}

static lv_obj_t * create_link(lv_obj_t * parent, const char * icon, const char * txt, const char * txt2)
{
    lv_obj_t * obj = create_text(parent, icon, txt);

    lv_obj_t * label = lv_label_create(obj);
    lv_label_set_text(label, txt2);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_group_add_obj(lv_group_get_default(), label);

    lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_DEFAULT);
    lv_obj_set_style_text_decor(label, LV_TEXT_DECOR_UNDERLINE, LV_STATE_FOCUSED);
    lv_obj_set_style_text_decor(label, LV_TEXT_DECOR_UNDERLINE, LV_STATE_PRESSED);

    return obj;
}

static lv_obj_t * create_button(lv_obj_t * parent, const char * icon, const char * txt, const char * txt2)
{
    lv_obj_t * obj = create_text(parent, icon, txt);

    lv_obj_t * btn = lv_btn_create(obj);
    
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, txt2);
    lv_obj_center(label);

    return obj;
}

static void btn_reset_clicked(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (BlynkState::get() != MODE_WAIT_CONFIG) {
        BlynkState::set(MODE_RESET_CONFIG);
    }
}

static void brightness_changed(lv_event_t* e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    backLight.setBrightness(lv_slider_get_value(slider));
}

static void sound_switched(lv_event_t* e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    soundEnabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

void display_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

void display_run()
{
    lv_timer_handler();
}

void keyboard_read(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    uint32_t last_key = 0;
    if (digitalRead(BTN_OK) == LOW)         { last_key = LV_KEY_ENTER; }
    else if (digitalRead(BTN_RIGHT) == LOW) { last_key = LV_KEY_RIGHT; }
    else if (digitalRead(BTN_LEFT) == LOW)  { last_key = LV_KEY_LEFT;  }
    else if (digitalRead(BTN_UP) == LOW)    { last_key = LV_KEY_PREV;  }
    else if (digitalRead(BTN_DOWN) == LOW)  { last_key = LV_KEY_NEXT;  }
    else if (digitalRead(BTN_C) == LOW)     { last_key = LV_KEY_PREV;  }
    else if (digitalRead(BTN_B) == LOW)     { last_key = LV_KEY_ESC;   }
    else if (digitalRead(BTN_A) == LOW)     { last_key = LV_KEY_NEXT;  }
    
    static uint32_t prev_key = 0;
    if (soundEnabled && prev_key == 0 && last_key != 0) {
        playTone(1915, 10);
    }
    prev_key = last_key;

    data->state = (last_key) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->key = last_key;
}

void display_init()
{
    tft.begin();
    tft.setRotation(3);

    backLight.initialize();
    backLight.setBrightness(backLight.getMaxBrightness()/2);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 16);

    /* Init the display */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    /* Init keypad */
    pinMode(BTN_UP,    INPUT_PULLUP);
    pinMode(BTN_DOWN,  INPUT_PULLUP);
    pinMode(BTN_LEFT,  INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_OK,    INPUT_PULLUP);
    pinMode(BTN_A,     INPUT_PULLUP);
    pinMode(BTN_B,     INPUT_PULLUP);
    pinMode(BTN_C,     INPUT_PULLUP);

    static lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = keyboard_read;
    lv_indev_t* kb_indev = lv_indev_drv_register(&kb_drv);
    
    /* Default group */
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(kb_indev, g);

    /* Create simple Menu */

    lv_obj_t * menu = lv_menu_create(lv_scr_act());

    lv_color_t bg_color = lv_obj_get_style_bg_color(menu, 0);
    lv_obj_set_style_bg_color(menu, lv_color_darken(lv_obj_get_style_bg_color(menu, 0), 40), 0);
    lv_obj_set_size(menu, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_center(menu);

    lv_obj_t * cont;
    lv_obj_t * section;
    
    lv_obj_t * sub_about_page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(sub_about_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(sub_about_page);
    section = lv_menu_section_create(sub_about_page);

    String lv_ver = String("v") + lv_version_major() + "." +
                    lv_version_minor() + "." + lv_version_patch();
                    
    String wifi_ver = String("v") + rpc_system_version();

    create_status(section, NULL, "Firmware", "v" BLYNK_FIRMWARE_VERSION);
    create_status(section, NULL, "Build", __DATE__ " " __TIME__);
    create_status(section, NULL, "Blynk", "v" BLYNK_VERSION);
    create_status(section, NULL, "LVGL", lv_ver.c_str());
    create_status(section, NULL, "WiFi FW", wifi_ver.c_str());

    lv_obj_t * sub_status_page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(sub_status_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(sub_status_page);
    section = lv_menu_section_create(sub_status_page);
    
    cont = create_status(section, NULL, "Network SSID", STR_NONE);
    lbl_ssid = lv_obj_get_child(cont, 1);
    
    cont = create_status(section, NULL, "IP address", STR_NONE);
    lbl_ip = lv_obj_get_child(cont, 1);

    cont = create_status(section, NULL, "RSSI", STR_NONE);
    lbl_rssi = lv_obj_get_child(cont, 1);

    cont = create_status(section, NULL, "MAC", STR_NONE);
    lbl_mac = lv_obj_get_child(cont, 1);
    
    lv_menu_cont_create(sub_status_page);
    section = lv_menu_section_create(sub_status_page);
    cont = create_button(section, NULL, "Configuration", "Reset");
    btn_reset = lv_obj_get_child(cont, 1);
    lv_obj_add_event_cb(btn_reset, btn_reset_clicked, LV_EVENT_CLICKED, NULL);

    /* Create a root page */
    static char devName[64];
    getWiFiName(devName, sizeof(devName));
    
    lv_obj_t * root_page = lv_menu_page_create(menu, devName);
    lv_obj_set_style_pad_hor(root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    section = lv_menu_section_create(root_page);

    cont = create_slider(section, LV_SYMBOL_IMAGE, "Brightness", 1, backLight.getMaxBrightness(), backLight.getBrightness());
    lv_obj_add_event_cb(lv_obj_get_child(cont, 2), brightness_changed, LV_EVENT_VALUE_CHANGED, menu);

    cont = create_switch(section, LV_SYMBOL_AUDIO, "Sound", soundEnabled);
    lv_obj_add_event_cb(lv_obj_get_child(cont, 2), sound_switched, LV_EVENT_VALUE_CHANGED, menu);
    
    cont = create_link(section, LV_SYMBOL_WIFI, "Status", " ");
    lbl_state = lv_obj_get_child(cont, 2);
    lv_menu_set_load_page_event(menu, lbl_state, sub_status_page);

    lv_menu_cont_create(root_page);
    section = lv_menu_section_create(root_page);
    cont = create_link(section, LV_SYMBOL_SETTINGS, "About", ">");
    cont = lv_obj_get_child(cont, 2);
    lv_menu_set_load_page_event(menu, cont, sub_about_page);

    lv_menu_set_page(menu, root_page);
}

void display_update_state()
{
    const State m = BlynkState::get();
    if (MODE_WAIT_CONFIG == m) {
        lv_label_set_text_static(lbl_ip, STR_NONE);
        lv_label_set_text_static(lbl_ssid, STR_NONE);
        lv_label_set_text_static(lbl_state, "Configuring");
    } else if (MODE_CONFIGURING == m) {
        lv_label_set_text_static(lbl_state, "Configuring...");
    } else if (MODE_CONNECTING_NET == m) {
        lv_label_set_text_static(lbl_ssid, configStore.wifiSSID);
        lv_label_set_text_static(lbl_state, "Connecting to WiFi");
    } else if (MODE_CONNECTING_CLOUD == m) {
        lv_label_set_text(lbl_ip, WiFi.localIP().toString().c_str());
        lv_label_set_text_static(lbl_state, "Connecting to Blynk");
    } else if (MODE_RUNNING == m) {
        lv_label_set_text_static(lbl_state, "Connected");
    } else if (MODE_OTA_UPGRADE == m) {
        lv_label_set_text_static(lbl_state, "Updating firmware");
    } else if (MODE_ERROR == m) {
        lv_label_set_text_static(lbl_state, "Error");
    }

    if (m == MODE_CONNECTING_NET || m == MODE_CONNECTING_CLOUD || m == MODE_RUNNING)
    {
        lv_obj_clear_state(btn_reset, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(btn_reset, LV_STATE_DISABLED);
    }
    display_run();
}

/*
 * Main
 */

void setup()
{
  Serial.begin(115200);
  delay(100);

  display_init();
  buzzer_init();

  edgentTimer.setInterval(3000, []() {
    String mac = WiFi.macAddress();
    mac.toUpperCase();
    lv_label_set_text(lbl_mac, mac.c_str());

    if (WiFi.isConnected()) {
        String rssi = String(WiFi.RSSI()) + " dBm";
        lv_label_set_text(lbl_rssi, rssi.c_str());
    } else {
        lv_label_set_text_static(lbl_rssi, STR_NONE);
    }
  });

  BlynkEdgent.begin();
}

void loop() {
  BlynkEdgent.run();
}
