/*
 * HealthyPi Move
 * 
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 Protocentral Electronics
 *
 * Author: Ashwin Whitchurch, Protocentral Electronics
 * Contact: ashwin@protocentral.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <lvgl.h>
#include <stdio.h>
#include <zephyr/logging/log.h>

#include "hpi_common_types.h"
#include "hw_module.h"
#include "ui/move_ui.h"

LOG_MODULE_REGISTER(scr_bpt_scr3, LOG_LEVEL_DBG);

lv_obj_t *scr_bpt_scr3;

// Externs
extern lv_style_t style_red_medium;
extern lv_style_t style_white_large;
extern lv_style_t style_white_medium;
extern lv_style_t style_scr_black;
extern lv_style_t style_tiny;

extern struct k_sem sem_fi_spo2_est_cancel;

void draw_scr_fi_sens_check(enum scroll_dir dir, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    
    scr_bpt_scr3 = lv_obj_create(NULL);
    lv_obj_add_style(scr_bpt_scr3, &style_scr_black, 0);
    lv_obj_clear_flag(scr_bpt_scr3, LV_OBJ_FLAG_SCROLLABLE); /// Flags

    lv_obj_t *cont_col = lv_obj_create(scr_bpt_scr3);
    lv_obj_set_size(cont_col, lv_pct(100), lv_pct(100));
    lv_obj_align_to(cont_col, NULL, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_set_flex_flow(cont_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_col, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_right(cont_col, -1, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_top(cont_col, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(cont_col, 1, LV_PART_MAIN);
    lv_obj_add_style(cont_col, &style_scr_black, 0);

    lv_obj_t *img_bpt = lv_img_create(cont_col);
    lv_img_set_src(img_bpt, &img_bpt_finger_90);
    

    lv_obj_t *label_info = lv_label_create(cont_col);
    lv_label_set_long_mode(label_info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_info, 300);
    lv_label_set_text(label_info, "Waiting for sensor to connect...");
    lv_obj_set_style_text_align(label_info, LV_TEXT_ALIGN_CENTER, 0);

    /* LVGL 9.2 compatible spinner widget */
    lv_obj_t *spinner = lv_spinner_create(cont_col);
    lv_obj_set_size(spinner, 100, 100);
    lv_spinner_set_anim_params(spinner, 1000, 270); /* 1000ms period, 270 degree arc */
    lv_obj_center(spinner);

    hpi_disp_set_curr_screen(SCR_SPL_FI_SENS_CHECK);
    hpi_show_screen(scr_bpt_scr3, dir);
}

void gesture_down_scr_fi_sens_check(void)
{
    // Cancel SpO2 estimation if in progress
    k_sem_give(&sem_fi_spo2_est_cancel);
    // Handle gesture down event
    hpi_load_screen(SCR_SPO2, SCROLL_DOWN);
}