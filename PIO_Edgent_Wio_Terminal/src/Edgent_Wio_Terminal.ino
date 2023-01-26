/*************************************************************
  Blynk is a platform with iOS and Android apps to control
  ESP32, Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build mobile and web interfaces for any
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: https://www.blynk.io
    Sketch generator:           https://examples.blynk.cc
    Blynk community:            https://community.blynk.cc
    Follow us:                  https://www.fb.com/blynkapp
                                https://twitter.com/blynk_app

  Blynk library is licensed under MIT license
 *************************************************************
  Blynk.Edgent implements:
  - Blynk.Inject - Dynamic WiFi credentials provisioning
  - Blynk.Air    - Over The Air firmware updates
  - Simple LVGL-based UI

  NOTE: Please update the WiFi module firmware, if needed:
    https://wiki.seeedstudio.com/Wio-Terminal-Network-Overview
 *************************************************************/

/* Fill in information from your Blynk Template here */
//#define BLYNK_TEMPLATE_ID           "TMPxxxxxxx"
//#define BLYNK_TEMPLATE_NAME         "Device"

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
 * Basic IO
 */

const int BTN_UP        = WIO_5S_UP;
const int BTN_DOWN      = WIO_5S_DOWN;
const int BTN_LEFT      = WIO_5S_LEFT;
const int BTN_RIGHT     = WIO_5S_RIGHT;
const int BTN_OK        = WIO_5S_PRESS;
const int BTN_A         = WIO_KEY_A;
const int BTN_B         = WIO_KEY_B;
const int BTN_C         = WIO_KEY_C;

LCDBackLight backLight;

void board_init() {
    backLight.initialize();
    backLight.setBrightness(backLight.getMaxBrightness()/2);

    pinMode(WIO_BUZZER, OUTPUT);

    /* Init keypad */
    pinMode(BTN_UP,    INPUT_PULLUP);
    pinMode(BTN_DOWN,  INPUT_PULLUP);
    pinMode(BTN_LEFT,  INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_OK,    INPUT_PULLUP);
    pinMode(BTN_A,     INPUT_PULLUP);
    pinMode(BTN_B,     INPUT_PULLUP);
    pinMode(BTN_C,     INPUT_PULLUP);
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

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Comment this out to use 5-way switch as a keypad
#define EMULATE_MOUSE

#include <GUI.h>

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);

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

#ifdef EMULATE_MOUSE

void mouse_read(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    static int16_t last_x = SCREEN_WIDTH - 20, last_y = SCREEN_HEIGHT - 24;
    static const int speed = 3;

    if (digitalRead(BTN_RIGHT) == LOW) { last_x += speed; }
    if (digitalRead(BTN_LEFT) == LOW)  { last_x -= speed; }
    if (digitalRead(BTN_UP) == LOW)    { last_y -= speed; }
    if (digitalRead(BTN_DOWN) == LOW)  { last_y += speed; }

    last_x = max(0, min(last_x, SCREEN_WIDTH-2));
    last_y = max(0, min(last_y, SCREEN_HEIGHT-2));

    data->point.x = last_x;
    data->point.y = last_y;

    if (digitalRead(BTN_OK) == LOW) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

#else

void keyboard_read(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    uint32_t last_key = 0;
    if (digitalRead(BTN_OK) == LOW)         { last_key = LV_KEY_ENTER; }
    else if (digitalRead(BTN_RIGHT) == LOW) { last_key = LV_KEY_RIGHT; }
    else if (digitalRead(BTN_LEFT) == LOW)  { last_key = LV_KEY_LEFT;  }
    else if (digitalRead(BTN_UP) == LOW)    { last_key = LV_KEY_PREV;  }
    else if (digitalRead(BTN_DOWN) == LOW)  { last_key = LV_KEY_NEXT;  }

    data->state = (last_key) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->key = last_key;
}

#endif

void encoder_read(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    static uint32_t last_time = 0;
    uint32_t t = millis();

    if (t - last_time < 200)                { data->enc_diff =  0; }
    else if (digitalRead(BTN_C) == LOW)     { data->enc_diff = -1; last_time = t; }
    else if (digitalRead(BTN_A) == LOW)     { data->enc_diff =  1; last_time = t; }
    else                                    { last_time = 0; }

    if (digitalRead(BTN_B) == LOW) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void display_init()
{
    tft.begin();
    tft.setRotation(3);

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t frame_buf[ SCREEN_WIDTH * 16 ];

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, frame_buf, NULL, SCREEN_WIDTH * 16);

    /* Init the display */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    /* Default group */
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_group_set_focus_cb(g, group_focus_cb);

    /* Input drivers */
#ifdef EMULATE_MOUSE
    static lv_indev_drv_t ptr_drv;
    lv_indev_drv_init(&ptr_drv);
    ptr_drv.type = LV_INDEV_TYPE_POINTER;
    ptr_drv.read_cb = mouse_read;
    lv_indev_t* mouse_indev = lv_indev_drv_register(&ptr_drv);

    LV_IMG_DECLARE(mouse_cursor_icon);
    lv_obj_t * cursor_obj = lv_img_create(lv_scr_act());
    lv_img_set_src(cursor_obj, &mouse_cursor_icon);
    lv_indev_set_cursor(mouse_indev, cursor_obj);
#else
    static lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = keyboard_read;
    lv_indev_t* kb_indev = lv_indev_drv_register(&kb_drv);
    lv_indev_set_group(kb_indev, g);
#endif

    static lv_indev_drv_t enc_drv;
    lv_indev_drv_init(&enc_drv);
    enc_drv.type = LV_INDEV_TYPE_ENCODER;
    enc_drv.read_cb = encoder_read;
    lv_indev_t * enc_indev = lv_indev_drv_register(&enc_drv);
    lv_indev_set_group(enc_indev, g);

}

void display_update_state()
{
    const State m = BlynkState::get();
    if (MODE_WAIT_CONFIG == m) {
        lv_label_set_text_static(lbl_ip, STR_NONE);
        lv_label_set_text_static(lbl_ssid, STR_NONE);
        lv_label_set_text_static(lbl_rssi, STR_NONE);
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

  board_init();
  display_init();
  gui_init();

  edgentTimer.setInterval(3000, []() {
    String mac = WiFi.macAddress();
    mac.toUpperCase();
    lv_label_set_text(lbl_mac, mac.c_str());

    if (WiFi.isConnected()) {
        lv_label_set_text_fmt(lbl_rssi, "%d dBm", WiFi.RSSI());
    } else {
        lv_label_set_text_static(lbl_rssi, STR_NONE);
    }
  });

  BlynkEdgent.begin();
}

void loop() {
  BlynkEdgent.run();
}
