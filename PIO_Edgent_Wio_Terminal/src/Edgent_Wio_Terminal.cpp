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
/* Read more: https://bit.ly/BlynkInject */
//#define BLYNK_TEMPLATE_ID           "TMPxxxxxxx"
//#define BLYNK_TEMPLATE_NAME         "Device"

#define BLYNK_FIRMWARE_VERSION        "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

#include "BlynkEdgent.h"
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

const int SCREEN_WIDTH  = 320;
const int SCREEN_HEIGHT = 240;

LCDBackLight backLight;
TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);


void board_init() {
    backLight.initialize();
    backLight.setBrightness(backLight.getMaxBrightness()/2);

    tft.begin();
    tft.setRotation(3);

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

void refreshScreen() {
    /*
     * TODO: draw graphics using tft
     */
}

/*
 * Main
 */

void setup()
{
  Serial.begin(115200);
  delay(100);

  board_init();

  BlynkEdgent.begin();

  refreshScreen();
  edgentTimer.setInterval(3000, refreshScreen);
}

void loop() {
  BlynkEdgent.run();
}
