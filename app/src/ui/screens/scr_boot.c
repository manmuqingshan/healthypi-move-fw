#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <lvgl.h>
#include <stdio.h>
#include <app_version.h>

#include "ui/move_ui.h"

LOG_MODULE_REGISTER(boot_module, LOG_LEVEL_WRN);

lv_obj_t *scr_boot;
static lv_obj_t *ta_boot;

void scr_boot_add_final(bool status);

// Externs
extern lv_style_t style_red_medium;

void draw_scr_boot(void)
{
    scr_boot = lv_obj_create(NULL);
    lv_obj_clear_flag(scr_boot, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    lv_obj_set_style_bg_color(scr_boot, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(scr_boot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *label_hpi = lv_label_create(scr_boot);
    lv_label_set_text(label_hpi, "HealthyPi Move");
    lv_obj_align(label_hpi, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *label_boot = lv_label_create(scr_boot);
    lv_label_set_text(label_boot, "Booting v" APP_VERSION_STRING);
    lv_obj_align_to(label_boot, label_hpi, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    ta_boot = lv_textarea_create(scr_boot);
    lv_obj_align_to(ta_boot, label_boot, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_size(ta_boot, 350, 250);
    lv_obj_set_style_bg_opa(ta_boot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ta_boot, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /*lv_obj_t *btn_proceed = lv_btn_create(scr_boot);
    lv_obj_add_event_cb(btn_proceed, scr_boot_proceed_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn_proceed, LV_ALIGN_BOTTOM_MID, 0, -30);

    lv_obj_t *label_btn_proceed = lv_label_create(btn_proceed);
    lv_label_set_text(label_btn_proceed, "Proceed");
    lv_obj_center(label_btn_proceed);
    */

    // lv_obj_add_event_cb(ta, textarea_event_handler, LV_EVENT_READY, ta);
    lv_obj_add_state(ta_boot, LV_STATE_FOCUSED);
    lv_label_set_recolor(ta_boot, true);

    hpi_disp_set_curr_screen(SCR_SPL_BOOT);
    hpi_show_screen(scr_boot, SCROLL_RIGHT);
}

void scr_boot_add_status(char *dev_label, bool status, bool show_status)
{
    char buf[32];
    if (show_status)
    {
        sprintf(buf, "%s: %s\n", dev_label, status ? "OK" : "FAIL");
    }
    else
    {
        sprintf(buf, "%s\n", dev_label);
    }

    lv_textarea_add_text(ta_boot, buf);
    lv_textarea_cursor_down(ta_boot);
    //lv_obj_scroll_by(ta_boot, 0, -20, LV_ANIM_ON);
}

void scr_boot_add_final(bool status)
{
    char buf[32];
    sprintf(buf, "  \nCOMPLETE: %s\n", status ? "OK" : "FAIL");
    lv_textarea_add_text(ta_boot, buf);
}
