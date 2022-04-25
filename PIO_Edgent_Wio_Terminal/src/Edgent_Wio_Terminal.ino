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
static lv_color_t buf[ SCREEN_WIDTH * 10 ];

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT); /* TFT instance */

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

lv_obj_t *lbl_state;
lv_obj_t *btn_reset;

void keyboard_read(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
  uint32_t last_key = 0;
  if (digitalRead(BTN_OK) == LOW)         {  last_key = LV_KEY_ENTER; }
  else if (digitalRead(BTN_RIGHT) == LOW) {  last_key = LV_KEY_RIGHT; }
  else if (digitalRead(BTN_LEFT) == LOW)  {  last_key = LV_KEY_LEFT;  }
  else if (digitalRead(BTN_UP) == LOW)    {  last_key = LV_KEY_UP;    }
  else if (digitalRead(BTN_DOWN) == LOW)  {  last_key = LV_KEY_DOWN;  }
  else if (digitalRead(BTN_A) == LOW)     {  last_key = LV_KEY_PREV;  }
  else if (digitalRead(BTN_B) == LOW)     {  last_key = LV_KEY_ENTER; }
  else if (digitalRead(BTN_C) == LOW)     {  last_key = LV_KEY_NEXT;  }

  data->state = (last_key) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->key = last_key;
}

void btn1_event_handler(lv_event_t* e)
{
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_CLICKED) {
    if (BlynkState::get() != MODE_WAIT_CONFIG) {
      BlynkState::set(MODE_RESET_CONFIG);
    }
  }
}

void display_init()
{
  tft.begin();
  tft.setRotation( 3 );

  lv_init();
  lv_disp_draw_buf_init( &draw_buf, buf, NULL, SCREEN_WIDTH * 10 );

  /* Init the display */
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = display_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );
  
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

  /* Create simple UI */
  lbl_state = lv_label_create(lv_scr_act());

  btn_reset = lv_btn_create(lv_scr_act());
  lv_obj_add_event_cb(btn_reset, btn1_event_handler, LV_EVENT_ALL, NULL);
  lv_obj_align(btn_reset, LV_ALIGN_CENTER, 0, -40);
  lv_obj_add_state(btn_reset, LV_STATE_DISABLED);
  //lv_obj_clear_flag(btn_reset, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t * label_reset = lv_label_create(btn_reset);
  lv_label_set_text(label_reset, "Reset config");
  lv_obj_center(label_reset);
}

void display_update_state()
{
  const State m = BlynkState::get();
  if (MODE_WAIT_CONFIG == m) {
    lv_label_set_text(lbl_state, "Configuring");
  } else if (MODE_CONFIGURING == m) {
    lv_label_set_text(lbl_state, "Configuring...");
  } else if (MODE_CONNECTING_NET == m) {
    lv_label_set_text(lbl_state, "Connecting to WiFi");
  } else if (MODE_CONNECTING_CLOUD == m) {
    lv_label_set_text(lbl_state, "Connecting to Blynk");
  } else if (MODE_RUNNING == m) {
    lv_label_set_text(lbl_state, "Connected");
  } else if (MODE_OTA_UPGRADE == m) {
    lv_label_set_text(lbl_state, "Updating firmware");
  } else if (MODE_ERROR == m) {
    lv_label_set_text(lbl_state, "Error");
  }
  lv_obj_align(lbl_state, LV_ALIGN_CENTER, 0, 0);
  
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

  Serial.printf("LVGL v%d.%d.%d\n",
                lv_version_major(), lv_version_minor(), lv_version_patch());

  BlynkEdgent.begin();
}

void loop() {
  BlynkEdgent.run();
}
