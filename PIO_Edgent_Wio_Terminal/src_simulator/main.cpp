#include <unistd.h>
#define SDL_MAIN_HANDLED        /*To fix SDL's "undefined reference to WinMain" issue*/
#include <SDL2/SDL.h>
#include "display/monitor.h"
#include "indev/mouse.h"
#include "indev/mousewheel.h"
#include "indev/keyboard.h"
#include "sdl/sdl.h"

#include <string>
#include <lvgl.h>

#define BLYNK_FIRMWARE_VERSION 	"1.0.0"
#define BLYNK_VERSION 			"1.0.0"

#include "GUI.h"

/**
 * A task to measure the elapsed time for LittlevGL
 * @param data unused
 * @return never return
 */
static int tick_thread(void * data)
{
    (void)data;

    while(1) {
        SDL_Delay(5);   /*Sleep for 5 millisecond*/
        lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
    }

    return 0;
}


void hal_setup(void)
{
    // Workaround for sdl2 `-m32` crash
    // https://bugs.launchpad.net/ubuntu/+source/libsdl2/+bug/1775067/comments/7
    #ifndef WIN32
        setenv("DBUS_FATAL_WARNINGS", "0", 1);
    #endif

    /* Add a display
     * Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/

    lv_disp_draw_buf_init(&draw_buf, frame_buf, NULL, SDL_HOR_RES * 16);    /*Initialize the display buffer*/

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);              /*Basic initialization*/
    disp_drv.flush_cb = sdl_display_flush;    /*Used when `LV_VDB_SIZE != 0` in lv_conf.h (buffered drawing)*/
    disp_drv.draw_buf = &draw_buf;
    disp_drv.hor_res = SDL_HOR_RES;
    disp_drv.ver_res = SDL_VER_RES;
    //disp_drv.disp_fill = monitor_fill;      /*Used when `LV_VDB_SIZE == 0` in lv_conf.h (unbuffered drawing)*/
    //disp_drv.disp_map = monitor_map;        /*Used when `LV_VDB_SIZE == 0` in lv_conf.h (unbuffered drawing)*/
    lv_disp_drv_register(&disp_drv);

    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);

    /* Add the mouse as input device
     * Use the 'mouse' driver which reads the PC's mouse*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);            /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;       /*This function will be called periodically (by the library) to get the mouse position and state*/
    lv_indev_t * mouse_indev = lv_indev_drv_register(&indev_drv);

    static lv_indev_drv_t indev_drv_2;
    lv_indev_drv_init(&indev_drv_2); /*Basic initialization*/
    indev_drv_2.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv_2.read_cb = sdl_keyboard_read;
    lv_indev_t *kb_indev = lv_indev_drv_register(&indev_drv_2);
    lv_indev_set_group(kb_indev, g);

    static lv_indev_drv_t indev_drv_3;
    lv_indev_drv_init(&indev_drv_3); /*Basic initialization*/
    indev_drv_3.type = LV_INDEV_TYPE_ENCODER;
    indev_drv_3.read_cb = sdl_mousewheel_read;
    lv_indev_t * enc_indev = lv_indev_drv_register(&indev_drv_3);
    lv_indev_set_group(enc_indev, g);

    sdl_init();

    /* Tick init.
     * You have to call 'lv_tick_inc()' in periodically to inform LittelvGL about how much time were elapsed
     * Create an SDL thread to do this*/
    SDL_CreateThread(tick_thread, "tick", NULL);

#ifdef SHOW_CUSTOM_CURSOR
    SDL_ShowCursor(SDL_DISABLE);

    LV_IMG_DECLARE(mouse_cursor_icon);
    lv_obj_t * cursor_obj = lv_img_create(lv_scr_act());
    lv_img_set_src(cursor_obj, &mouse_cursor_icon);
    lv_indev_set_cursor(mouse_indev, cursor_obj);
#endif
}

void hal_loop(void)
{
    while(1) {
        SDL_Delay(5);
        lv_task_handler();
    }
}


int main(void)
{
    lv_init();

    hal_setup();

    gui_init();

    lv_label_set_text_static(lbl_state, "Connected");
    lv_label_set_text_fmt(lbl_rssi, "%d dBm", -65);
    lv_label_set_text_static(lbl_mac, "AA:BB:CC:11:22:33");
    lv_label_set_text_static(lbl_ssid, "Test WiFi AP");
    lv_label_set_text_static(lbl_ip, "192.168.1.1");

    hal_loop();
}

