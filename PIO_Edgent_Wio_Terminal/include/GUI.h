
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

static lv_disp_draw_buf_t draw_buf;
static lv_color_t frame_buf[ SCREEN_WIDTH * 16 ];

lv_obj_t *lbl_state, *lbl_rssi, *lbl_ssid, *lbl_ip, *lbl_mac, *btn_reset;

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

static void make_text_focusable(lv_obj_t * obj)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_group_add_obj(lv_group_get_default(), obj);

    static lv_style_t outline;
    lv_style_init(&outline);
    lv_style_set_radius(&outline, lv_dpx(6));
    lv_style_set_outline_color(&outline, lv_theme_get_color_primary(obj));
    lv_style_set_outline_width(&outline, lv_dpx(3));
    lv_style_set_outline_pad(&outline, -lv_dpx(3));
    lv_style_set_outline_opa(&outline, LV_OPA_50);

    lv_obj_add_style(obj, &outline, LV_STATE_FOCUS_KEY);
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
    lv_obj_set_size(sw, (4 * LV_DPI_DEF) / 12, (4 * LV_DPI_DEF) / 22);

    return obj;
}

static lv_obj_t * create_status(lv_obj_t * parent, const char * icon, const char * txt, const char * txt2 = STR_NONE)
{
    lv_obj_t * obj = create_text(parent, icon, txt);

    lv_obj_t * label = lv_label_create(obj);
    lv_label_set_text(label, txt2);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    return obj;
}

#if 0
static lv_obj_t * create_link(lv_obj_t * parent, const char * icon, const char * txt, const char * txt2 = STR_NONE)
{
    lv_obj_t * obj = create_text(parent, icon, txt);

    lv_obj_t * label = lv_label_create(obj);
    lv_label_set_text(label, txt2);

    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    lv_group_add_obj(lv_group_get_default(), label);

    lv_obj_set_style_text_color(label, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_DEFAULT);
    lv_obj_set_style_text_decor(label, LV_TEXT_DECOR_UNDERLINE, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_text_decor(label, LV_TEXT_DECOR_UNDERLINE, LV_STATE_PRESSED);

    return obj;
}
#endif

static lv_obj_t * create_button(lv_obj_t * parent, const char * icon, const char * txt, const char * txt2)
{
    lv_obj_t * obj = create_text(parent, icon, txt);

    lv_obj_t * btn = lv_btn_create(obj);
    
    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, txt2);
    lv_obj_center(label);

    return obj;
}

static void msgbox_reset_clicked(lv_event_t* e)
{
    lv_obj_t * mbox = lv_event_get_current_target(e);
    if (lv_msgbox_get_active_btn(mbox) == 0) {
#ifndef EMULATOR
        if (BlynkState::get() != MODE_WAIT_CONFIG) {
            BlynkState::set(MODE_RESET_CONFIG);
        }
#endif
    }
    lv_msgbox_close(mbox);
}

static void btn_reset_clicked(lv_event_t* e)
{
    static const char * btns[] = { "OK", "Cancel", NULL };

    lv_obj_t * mbox = lv_msgbox_create(NULL, "Warning", "Device configuration will be reset", btns, false);
    lv_obj_add_event_cb(mbox, msgbox_reset_clicked, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);

    lv_btnmatrix_set_selected_btn(lv_msgbox_get_btns(mbox), 1);
    lv_group_focus_obj(lv_msgbox_get_btns(mbox));
}

static void brightness_changed(lv_event_t* e)
{
    lv_obj_t * slider = lv_event_get_target(e);
#ifndef EMULATOR
    backLight.setBrightness(lv_slider_get_value(slider));
#endif
}

bool soundEnabled = true;

static void sound_switched(lv_event_t* e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    soundEnabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void group_focus_cb(lv_group_t* g)
{
    if (soundEnabled) {
#ifndef EMULATOR
        playTone(1915, 10);
#endif
    }
}

static void menu_opened(lv_event_t* e)
{
    // TODO: Does not always focus the back button. LVGL bug?
    lv_obj_t * menu = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_t * btn = lv_menu_get_main_header_back_btn(menu);
    if (!lv_menu_back_btn_is_root(menu, btn)) {
        lv_group_focus_obj(btn);
    }
}

static void gui_init()
{
#ifdef EMULATOR
    #define MENU_DARKEN 20
    #define BLYNK_FIRMWARE_VERSION "1.0.0"
    #define BLYNK_VERSION "1.0.0"

    static char devName[] = "Emulator";

    std::string lv_ver = "v" + std::to_string(lv_version_major()) +
                         "." + std::to_string(lv_version_minor()) +
                         "." + std::to_string(lv_version_patch());
    std::string wifi_ver = "---";

    const int maxBrightness = 100, currBrightness = 50;
#else
    #define MENU_DARKEN 40

    static char devName[64];
    getWiFiName(devName, sizeof(devName));

    String lv_ver = String("v") + lv_version_major() +
                           "."  + lv_version_minor() +
                           "."  + lv_version_patch();

    String wifi_ver = String("v") + rpc_system_version();

    const int maxBrightness  = backLight.getMaxBrightness(),
              currBrightness = backLight.getBrightness();

#endif

    /* Create simple Menu */

    lv_obj_t * menu = lv_menu_create(lv_scr_act());
    lv_obj_add_event_cb(menu, menu_opened, LV_EVENT_VALUE_CHANGED, menu);

    lv_color_t bg_color = lv_obj_get_style_bg_color(menu, 0);
    lv_obj_set_style_bg_color(menu, lv_color_darken(bg_color, MENU_DARKEN), 0);
    lv_obj_set_size(menu, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_center(menu);

    lv_obj_t * cont;
    lv_obj_t * section;
    
    lv_obj_t * sub_about_page = lv_menu_page_create(menu, "System information");
    lv_obj_set_style_pad_hor(sub_about_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(sub_about_page);
    section = lv_menu_section_create(sub_about_page);

    create_status(section, NULL, "Firmware", "v" BLYNK_FIRMWARE_VERSION);
    create_status(section, NULL, "Build", __DATE__ " " __TIME__);
    create_status(section, NULL, "Blynk", "v" BLYNK_VERSION);
    create_status(section, NULL, "LVGL", lv_ver.c_str());
    create_status(section, NULL, "WiFi FW", wifi_ver.c_str());

    lv_obj_t * sub_status_page = lv_menu_page_create(menu, "Network status");
    lv_obj_set_style_pad_hor(sub_status_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(sub_status_page);
    section = lv_menu_section_create(sub_status_page);
    
    cont = create_status(section, NULL, "Network SSID");
    lbl_ssid = lv_obj_get_child(cont, 1);
    
    cont = create_status(section, NULL, "IP address");
    lbl_ip = lv_obj_get_child(cont, 1);

    cont = create_status(section, NULL, "RSSI");
    lbl_rssi = lv_obj_get_child(cont, 1);

    cont = create_status(section, NULL, "MAC");
    lbl_mac = lv_obj_get_child(cont, 1);
    
    lv_menu_separator_create(sub_status_page);
    section = lv_menu_section_create(sub_status_page);

    cont = create_button(section, NULL, "Configuration", "Reset");
    btn_reset = lv_obj_get_child(cont, 1);
    lv_obj_add_event_cb(btn_reset, btn_reset_clicked, LV_EVENT_CLICKED, NULL);

    /* Create a root page */
    lv_obj_t * root_page = lv_menu_page_create(menu, devName);
    lv_obj_set_style_pad_hor(root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    section = lv_menu_section_create(root_page);

    cont = create_slider(section, LV_SYMBOL_IMAGE, "Brightness", 1, maxBrightness, currBrightness);
    lv_obj_add_event_cb(lv_obj_get_child(cont, 2), brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);

    cont = create_switch(section, LV_SYMBOL_AUDIO, "Sound", soundEnabled);
    lv_obj_add_event_cb(lv_obj_get_child(cont, 2), sound_switched, LV_EVENT_VALUE_CHANGED, NULL);
    
    cont = create_status(section, LV_SYMBOL_WIFI, "Status");
    make_text_focusable(cont);
    lbl_state = lv_obj_get_child(cont, 2);
    lv_menu_set_load_page_event(menu, cont, sub_status_page);

    lv_menu_separator_create(root_page);
    section = lv_menu_section_create(root_page);

    cont = create_text(section, LV_SYMBOL_SETTINGS, "About");
    make_text_focusable(cont);
    lv_menu_set_load_page_event(menu, cont, sub_about_page);

    lv_menu_set_page(menu, root_page);
}
