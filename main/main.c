#include "adc_button.h"
#include "esp_log.h"
#include "xiaozhi_wifi.h"
#include <stdio.h>

#include "xiaozhi_active.h"
#include "xiaozhi_audio.h"
#include "xiaozhi_audio_decoder.h"
#include "xiaozhi_audio_encoder.h"
#include "xiaozhi_common.h"
#include "xiaozhi_lvgl.h"
#include "xiaozhi_sr.h"
#include "xiaozhi_ws.h"

void adc_button_cb(void *arg, void *usr_data);
// 创建所有的环形缓冲区
void create_all_ring_buffer(void);
// 注册被唤醒的回调
void xiaozhi_wakeup_callback(void);
// vad的状态发送改变的回调
void xiaozhi_sr_vad_change_callback(void);
// 处理文本帧数据的方法
void xiaozhi_process_text_frame(char *text_frame);
// 处理二进制音频数据帧的方法
void process_binary_audio_frame(const char *bin_data, int data_len);
/* 创建一个任务，用于处理小智的音频数据发送至服务器 */
void xiaozhi_ws_send_audio_task(void *pvParameters);
/* 封装一个测试任务，用于短接两个环形缓冲区 */
void xiaozhi_ringbuffer_test(void *pvParameters);
#define TAG "main"

xiaozhi_handle_t xiaozhi_handle;

typedef enum {
    SW2_SINGLE_CLICK,
    SW3_SINGLE_CLICK,
    SW3_LONG_PRESS_START,
} adc_button_event_t;
void app_main(void)
{
    // 创建所有的环形缓冲区
    create_all_ring_buffer();
    // 创建ws相关的事件标志组
    xiaozhi_handle.ws_event_group = xEventGroupCreate();
    // 注册被唤醒的回调
    xiaozhi_handle.wakeup_callback = xiaozhi_wakeup_callback;
    //  注册vad改变的回调
    xiaozhi_handle.vad_change_callback = xiaozhi_sr_vad_change_callback;
    // 注册处理文本帧数据的方法
    xiaozhi_handle.process_text_frame = xiaozhi_process_text_frame;
    // 注册处理二进制音频数据帧的方法
    xiaozhi_handle.process_binary_audio_frame = process_binary_audio_frame;

    // Initialize ADC button
    adc_button_init();
    // 注册sw2注册单击事件的回调
    adc_button_register_sw2_callback(BUTTON_SINGLE_CLICK, adc_button_cb, (void *)SW2_SINGLE_CLICK);
    // 注册sw3的单击事件回调
    adc_button_register_sw3_callback(BUTTON_SINGLE_CLICK, adc_button_cb, (void *)SW3_SINGLE_CLICK);
    // 注册sw3的长按事件回调
    adc_button_register_sw3_callback(BUTTON_LONG_PRESS_START, adc_button_cb, (void *)SW3_LONG_PRESS_START);
    // lvgl初始化
    xiaozhi_lvgl_init();
    // wifi的初始化~
    xiaozhi_wifi_init();
    // 获取验证码的初始化
    xiaozhi_active_init();
    // ES8311的初始化
    xiaozhi_audio_init();
    // ! 初始化小智的状态为IDLE
    xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_IDLE;
    // SR的初始化.0
    xiaozhi_sr_init();
    // 编码器encoder的初始化
    xiaozhi_audio_encoder_init();
    // 解码器decoder的初始化
    xiaozhi_audio_decoder_init();
    // ws初始化
    xiaozhi_ws_init();

    xTaskCreatePinnedToCoreWithCaps(xiaozhi_ws_send_audio_task, "xiaozhi_ws_send_audio_task", 16 * 1024, NULL, 5, NULL, 1, MALLOC_CAP_SPIRAM);

}

void adc_button_cb(void *arg, void *usr_data)
{
    uint8_t event_type = (uint8_t)usr_data;
    switch (event_type) {
        case SW2_SINGLE_CLICK:
            ESP_LOGI(TAG, "SW2 SINGLE CLICK");
            break;
        case SW3_SINGLE_CLICK:
            ESP_LOGI(TAG, "SW3 SINGLE CLICK");
            break;
        case SW3_LONG_PRESS_START:
            ESP_LOGI(TAG, "SW3 LONG PRESS START");
            xiaozhi_wifi_reset();
            break;
        default:
    }
}

void create_all_ring_buffer(void)
{
    // sr2encoder的环形缓冲区
    xiaozhi_handle.ring_buffer_sr2encoder_handle = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
    // encoder2ws的环形缓冲区
    xiaozhi_handle.ring_buffer_encoder2ws_handle = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    // ws2decoder的环形缓冲区
    xiaozhi_handle.ring_buffer_ws2decoder_handle = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
}

/* 封装一个测试任务，用于短接两个环形缓冲区 */
void xiaozhi_ringbuffer_test(void *pvParameters)
{
    // while (1) {
    //     size_t size = 0;
    //     // 从encoder2ws的环形缓冲区提取数据
    //     void *rev_pointer = xRingbufferReceive(xiaozhi_handle.ring_buffer_encoder2ws_handle, &size, portMAX_DELAY);
    //     // 放入至ws2decoder的环形缓冲区
    //     xRingbufferSend(xiaozhi_handle.ring_buffer_ws2decoder_handle, rev_pointer, size, portMAX_DELAY);
    //     // 释放环形缓冲区
    //     vRingbufferReturnItem(xiaozhi_handle.ring_buffer_encoder2ws_handle, rev_pointer);
    // }
}

// 注册被唤醒的回调
void xiaozhi_wakeup_callback(void)
{
    if (xiaozhi_handle.xiaozhi_state == XIAOZHI_STATE_IDLE) {
        // 发起建立连接
        bool is_connected = xiaozhi_ws_connect_sync();
        if (is_connected) {
            // 发送hello
            xiaozhi_ws_send_hello();
            // 以阻塞的形式等待hello成功
            EventBits_t res = xEventGroupWaitBits(xiaozhi_handle.ws_event_group, WS_HELLO_SENT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
            if (!(res & WS_HELLO_SENT_BIT)) {
                ESP_LOGI(TAG, "send hello failed");
            } else {
                //! 发送iot的相关文本
                xiaozhi_ws_send_iot_state();
                xiaozhi_ws_send_iot_desc();
                // 发送唤醒词
                xiaozhi_ws_send_wkup_word();
            }
        }
    } else if (xiaozhi_handle.xiaozhi_state == XIAOZHI_STATE_SPEAKING) {
        // 发送终止消息，打断小智说话
        xiaozhi_ws_send_abort();
        // 发送唤醒词
        xiaozhi_ws_send_wkup_word();
    }
}

// vad的状态发送改变的回调
void xiaozhi_sr_vad_change_callback(void)
{
    // 检测到人声  1. 我们在说话  2. 小智在说话
    if (xiaozhi_handle.vad_current_state == VAD_SPEECH) {
        // if (xiaozhi_handle.xiaozhi_state != XIAOZHI_STATE_SPEAKING)
        if (xiaozhi_handle.xiaozhi_state == XIAOZHI_STATE_IDLE) {
            xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_LISTENING;
            // 发送开始监听
            xiaozhi_ws_send_start_listening();
        }
    }

    // 检测到静默 1. 我们没有说话  2. 小智在说话，但是卡顿了(网络原因)
    if (xiaozhi_handle.vad_current_state == VAD_SILENCE) {
        if (xiaozhi_handle.xiaozhi_state != XIAOZHI_STATE_SPEAKING) {
            xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_IDLE;
            // 发送停止监听
            xiaozhi_ws_send_stop_listening();
        }
    }
}
// 处理文本帧数据的方法
void xiaozhi_process_text_frame(char *text_frame)
{
    cJSON *root = cJSON_Parse(text_frame);
    if (root == NULL) {
        ESP_LOGI(TAG, "parse json failed");
        return;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (strcmp(type->valuestring, "hello") == 0) {
        // ! 修改hello回应成功的标志位
        xEventGroupSetBits(xiaozhi_handle.ws_event_group, WS_HELLO_SENT_BIT);
    }

    // 判断是否是emoji
    if (strcmp(type->valuestring, "llm") == 0) {
        char *emotion = cJSON_GetObjectItem(root, "emotion")->valuestring;
        xiaozhi_lvgl_set_emoji(emotion);
    }

    // 判断是否是文本内容
    if (strcmp(type->valuestring, "tts") == 0) {
        char *state = cJSON_GetObjectItem(root, "state")->valuestring;
        if (strcmp(state, "sentence_start") == 0) {
            char *text = cJSON_GetObjectItem(root, "text")->valuestring;
            xiaozhi_lvgl_set_text(text);
        }

        // 判断小智开始说话
        if (strcmp(state, "start") == 0) {
            // 小智开始说话
            xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_SPEAKING;
        }
        // 判断小智停止说话
        if (strcmp(state, "stop") == 0) {
            // 小智开始说话
            xiaozhi_handle.xiaozhi_state = XIAOZHI_STATE_IDLE;
        }
    }
    if (strcmp(type->valuestring, "iot") == 0) {
        cJSON *cmdsItem = cJSON_GetObjectItem(root, "commands");
        cJSON *cmdItem = cJSON_GetArrayItem(cmdsItem, 0);
        cJSON *parametersItem = cJSON_GetObjectItem(cmdItem, "parameters");

        char *method = cJSON_GetObjectItem(cmdItem, "method")->valuestring;
        if (strcmp(method, "SetVolume") == 0) {
            int volumne = cJSON_GetObjectItem(parametersItem, "volume")->valueint;
            xiaozhi_audio_volume_set(volumne);
        } else if (strcmp(method, "SetMute") == 0) {
            cJSON *muteItem = cJSON_GetObjectItem(parametersItem, "mute");
            bool is_mute = false;
            if (cJSON_IsBool(muteItem)) {
                is_mute = cJSON_IsTrue(muteItem);
            }
            xiaozhi_audio_mute_set(is_mute);
        }
    }
}

// 处理二进制音频数据帧的方法
void process_binary_audio_frame(const char *bin_data, int data_len)
{
    // 将二进制音频数据存入至ws2decoder的环形缓冲区
    xRingbufferSend(xiaozhi_handle.ring_buffer_ws2decoder_handle, bin_data, data_len, portMAX_DELAY);
}

/* 创建一个任务，用于处理小智的音频数据发送至服务器 */
void xiaozhi_ws_send_audio_task(void *pvParameters)
{
    while (1) {
        // 从encoder2ws的环形缓冲区中取出数据
        size_t rev_size = 0;
        void *rev_pointer = xRingbufferReceive(xiaozhi_handle.ring_buffer_encoder2ws_handle, &rev_size, portMAX_DELAY);
        // ! 一定要保证小智正在listening的时候才发送音频数据
        // ESP_LOGI(TAG, "rev_size: %d", xiaozhi_handle.xiaozhi_state);
        if (xiaozhi_handle.xiaozhi_state == XIAOZHI_STATE_LISTENING) {
            // 发送音频opus数据至服务器
            xiaozhi_ws_send_bin(rev_pointer, rev_size);
        }

        // 释放环形缓冲区
        vRingbufferReturnItem(xiaozhi_handle.ring_buffer_encoder2ws_handle, rev_pointer);
    }
}