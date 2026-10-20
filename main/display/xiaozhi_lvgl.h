#pragma once
#include "esp_log.h"
#include "xiaozhi_lcd.h"
#include "esp_lvgl_port.h"
#include "font_emoji.h"

void xiaozhi_lvgl_init(void);

/* 显示hello world */
void xiaozhi_lvgl_show_hello_world(void);

/* 显示二维码 */
void xiaozhi_lvgl_show_qrcode(char *qrcode_data);

/* 删除二维码 */
void xiaozhi_lvgl_delete_qrcode(void);

/* 修改标题的方法 */
void xiaozhi_lvgl_set_title(char *title);
/* 修改文本的方法 */
void xiaozhi_lvgl_set_text(char *text);
/* 修改emoji的方法 */
void xiaozhi_lvgl_set_emoji(char *emoji_name);