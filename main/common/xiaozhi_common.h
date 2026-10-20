#pragma once
#include "stdlib.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"
#include "freertos/ringbuf.h"
#include "freertos/event_groups.h"

/* 定义小智的状态 */
typedef enum
{
  XIAOZHI_STATE_IDLE,
  XIAOZHI_STATE_LISTENING,
  XIAOZHI_STATE_SPEAKING,
} xiaozhi_state_t;

typedef struct
{
  // websocket url
  char *websocket_url;
  // websocket token
  char *websocket_token;
  // 激活码
  char *activation_code;
  // 是否激活的标志
  bool is_activated;
  // 被唤醒的回调
  void (*wakeup_callback)(void);
  // 被唤醒的标志
  bool is_wakeup;
  // vad状态发生改变的回调
  void (*vad_change_callback)(void);
  // vad的当前状态
  vad_state_t vad_current_state;
  // vad的上一个状态
  vad_state_t vad_previous_state;

  // sr2encoder的环形缓冲区
  RingbufHandle_t ring_buffer_sr2encoder_handle;
  // encoder2ws的环形缓冲区
  RingbufHandle_t ring_buffer_encoder2ws_handle;
  // ws2decoder的环形缓冲区
  RingbufHandle_t ring_buffer_ws2decoder_handle;

  /* 定义和ws相关的事件标志组 */
  EventGroupHandle_t ws_event_group;

  /* 处理文本帧数据的方法 */
  void (*process_text_frame)(char *text_frame);
  /* 处理二进制音频数据帧的方法 */
  void (*process_binary_audio_frame)(const char *bin_data, int data_len);

  /* 小智的状态 */
  xiaozhi_state_t xiaozhi_state;

} xiaozhi_handle_t;

// ws客户端和服务器连接成功的掩码
#define WS_CONNECTED_BIT BIT0
// 发送hello成功的标志位掩码
#define WS_HELLO_SENT_BIT BIT1

extern xiaozhi_handle_t xiaozhi_handle;