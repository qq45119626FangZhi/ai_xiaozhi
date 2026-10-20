idf_component_register(
  SRCS 
    "main.c"
    "adc_button/adc_button.c"
    "internet/xiaozhi_wifi.c"
    "internet/xiaozhi_active.c"
    "internet/xiaozhi_ws.c"
    "display/xiaozhi_lcd.c"
    "display/xiaozhi_lvgl.c"
    "audio/xiaozhi_audio.c"
    "audio/xiaozhi_audio_encoder.c"
    "audio/xiaozhi_audio_decoder.c"
    "audio/xiaozhi_sr.c"
  INCLUDE_DIRS 
    "."
    "adc_button"
    "internet"
    "display"
    "common"
    "audio"
    "vol_iot_desc"
    
  EMBED_FILES image.jpg
  )
