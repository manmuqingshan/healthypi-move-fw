#include <zephyr/kernel.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <lvgl.h>
#include <stdio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/rtc.h>

#include "ui/move_ui.h"
#include "hw_module.h"

lv_obj_t *scr_today;

lv_obj_t *today_arcs;

lv_obj_t *cui_daily_mission_arc;
lv_obj_t *cui_daily_mission_arc_2;
lv_obj_t *cui_daily_mission_arc_3;

lv_obj_t *cui_steps_label;
lv_obj_t *cui_calories_label;
lv_obj_t *cui_time_label;

extern lv_style_t style_white_medium;

void draw_scr_today(enum scroll_dir m_scroll_dir)
{
    scr_today = lv_obj_create(NULL);

    lv_obj_clear_flag(scr_today, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    //draw_bg(scr_today);
    draw_header_minimal(scr_today, 320);

    //today_arcs = ui_dailymissiongroup_create(scr_today);

    lv_obj_t *cui_dailymissiongroup;
    cui_dailymissiongroup = lv_obj_create(scr_today);
    lv_obj_set_width(cui_dailymissiongroup, 360);
    lv_obj_set_height(cui_dailymissiongroup, 360);
    lv_obj_set_align(cui_dailymissiongroup, LV_ALIGN_CENTER);
    lv_obj_clear_flag(cui_dailymissiongroup, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(cui_dailymissiongroup, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cui_dailymissiongroup, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cui_dailymissiongroup, 0, LV_PART_MAIN);

    cui_daily_mission_arc = lv_arc_create(cui_dailymissiongroup);

    lv_obj_set_size(cui_daily_mission_arc, 360, 360);
    lv_obj_set_align(cui_daily_mission_arc, LV_ALIGN_CENTER);
    lv_obj_add_flag(cui_daily_mission_arc, LV_OBJ_FLAG_ADV_HITTEST); /// Flags
    lv_arc_set_value(cui_daily_mission_arc, 25);
    lv_arc_set_bg_angles(cui_daily_mission_arc, 90, 270);
    lv_obj_set_style_arc_color(cui_daily_mission_arc, lv_palette_lighten(LV_PALETTE_BLUE, 1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(cui_daily_mission_arc, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(cui_daily_mission_arc, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_color(cui_daily_mission_arc, lv_palette_darken(LV_PALETTE_BLUE, 4), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(cui_daily_mission_arc, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(cui_daily_mission_arc, true, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_width(cui_daily_mission_arc, 17, LV_PART_MAIN);      // Changes background arc width
    lv_obj_set_style_arc_width(cui_daily_mission_arc, 17, LV_PART_INDICATOR); // Changes set part width

    lv_obj_set_style_bg_color(cui_daily_mission_arc, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cui_daily_mission_arc, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    cui_daily_mission_arc_2 = lv_arc_create(cui_dailymissiongroup);

    lv_obj_set_size(cui_daily_mission_arc_2, 300, 300);
    lv_obj_set_align(cui_daily_mission_arc_2, LV_ALIGN_CENTER);
    lv_obj_add_flag(cui_daily_mission_arc_2, LV_OBJ_FLAG_ADV_HITTEST); /// Flags
    lv_arc_set_value(cui_daily_mission_arc_2, 75);
    lv_arc_set_bg_angles(cui_daily_mission_arc_2, 90, 270);
    lv_obj_set_style_arc_color(cui_daily_mission_arc_2, lv_palette_lighten(LV_PALETTE_GREEN,3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(cui_daily_mission_arc_2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(cui_daily_mission_arc_2, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_color(cui_daily_mission_arc_2, lv_palette_darken(LV_PALETTE_GREEN, 3) , LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(cui_daily_mission_arc_2, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(cui_daily_mission_arc_2, true, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_width(cui_daily_mission_arc_2, 17, LV_PART_MAIN);      // Changes background arc width
    lv_obj_set_style_arc_width(cui_daily_mission_arc_2, 17, LV_PART_INDICATOR); // Changes set part width

    lv_obj_set_style_bg_color(cui_daily_mission_arc_2, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cui_daily_mission_arc_2, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    cui_daily_mission_arc_3 = lv_arc_create(cui_dailymissiongroup);

    lv_obj_set_size(cui_daily_mission_arc_3, 240, 240);
    lv_obj_set_align(cui_daily_mission_arc_3, LV_ALIGN_CENTER);
    lv_obj_add_flag(cui_daily_mission_arc_3, LV_OBJ_FLAG_ADV_HITTEST); /// Flags
    lv_arc_set_value(cui_daily_mission_arc_3, 50);
    lv_arc_set_bg_angles(cui_daily_mission_arc_3, 90, 270);
    lv_obj_set_style_arc_color(cui_daily_mission_arc_3, lv_palette_lighten(LV_PALETTE_RED,3), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(cui_daily_mission_arc_3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(cui_daily_mission_arc_3, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_color(cui_daily_mission_arc_3, lv_palette_darken(LV_PALETTE_RED, 1), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(cui_daily_mission_arc_3, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(cui_daily_mission_arc_3, true, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_width(cui_daily_mission_arc_3, 17, LV_PART_MAIN);      // Changes background arc width
    lv_obj_set_style_arc_width(cui_daily_mission_arc_3, 17, LV_PART_INDICATOR); // Changes set part width

    lv_obj_set_style_bg_color(cui_daily_mission_arc_3, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cui_daily_mission_arc_3, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    /*lv_obj_t *label_title = lv_label_create(cui_dailymissiongroup);
    lv_label_set_text(label_title, "TODAY");
    lv_obj_align_to(label_title, NULL, LV_ALIGN_TOP_MID, 0, 65);
    */
    //lv_obj_set_style_text_color(label_title, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *cui_step;
    cui_step = lv_img_create(cui_dailymissiongroup);
    lv_img_set_src(cui_step, &img_steps_48px);
    lv_obj_set_width(cui_step, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(cui_step, LV_SIZE_CONTENT); /// 1
    lv_obj_add_flag(cui_step, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(cui_step, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_align_to(cui_step, NULL, LV_ALIGN_CENTER, 40, -40);

    cui_steps_label = lv_label_create(cui_dailymissiongroup);  
    lv_label_set_text(cui_steps_label, "888");
    lv_obj_align_to(cui_steps_label, cui_step, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_text_color(cui_steps_label, lv_palette_lighten(LV_PALETTE_BLUE, 1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(cui_steps_label, &lv_font_montserrat_34, LV_PART_MAIN | LV_STATE_DEFAULT);
    //lv_obj_add_style(cui_steps_label, &style_lbl_white, 0); 

    lv_obj_t *cui_calories;
    cui_calories = lv_img_create(cui_dailymissiongroup);
    lv_img_set_src(cui_calories, &img_calories_48px);
    lv_obj_set_width(cui_calories, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(cui_calories, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(cui_calories, LV_ALIGN_CENTER);
    lv_obj_add_flag(cui_calories, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(cui_calories, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_align_to(cui_calories, cui_step, LV_ALIGN_OUT_BOTTOM_MID, 0, 1);

    cui_calories_label = lv_label_create(cui_dailymissiongroup);
    lv_label_set_text(cui_calories_label, "100");
    lv_obj_align_to(cui_calories_label, cui_calories, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_text_color(cui_calories_label, lv_palette_lighten(LV_PALETTE_GREEN, 1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(cui_calories_label, &lv_font_montserrat_34, LV_PART_MAIN | LV_STATE_DEFAULT);
    //lv_obj_add_style(cui_calories_label, &style_lbl_white, 0);

    lv_obj_t *cui_time;
    cui_time = lv_img_create(cui_dailymissiongroup);
    lv_img_set_src(cui_time, &img_clock_48px);
    lv_obj_set_width(cui_time, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(cui_time, LV_SIZE_CONTENT); /// 1
    lv_obj_set_align(cui_time, LV_ALIGN_CENTER);
    lv_obj_add_flag(cui_time, LV_OBJ_FLAG_ADV_HITTEST);  /// Flags
    lv_obj_clear_flag(cui_time, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_align_to(cui_time, cui_calories, LV_ALIGN_OUT_BOTTOM_MID, 0, 1);

    cui_time_label = lv_label_create(cui_dailymissiongroup);
    lv_label_set_text(cui_time_label, "00:00");
    lv_obj_align_to(cui_time_label, cui_time, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_text_color(cui_time_label, lv_palette_lighten(LV_PALETTE_RED, 1), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(cui_time_label, &lv_font_montserrat_34, LV_PART_MAIN | LV_STATE_DEFAULT);

    hpi_disp_set_curr_screen(SCR_TODAY);
    hpi_show_screen(scr_today, m_scroll_dir);
}

void ui_dailymissiongroup_update(uint32_t steps_walk, uint32_t steps_run)
{
    if (cui_daily_mission_arc == NULL || cui_daily_mission_arc_2 == NULL)
        return;

    lv_arc_set_value(cui_daily_mission_arc, steps_walk);
    lv_arc_set_value(cui_daily_mission_arc_2, steps_run);
}
