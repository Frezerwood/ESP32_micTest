#include <Arduino.h>
#include <driver/i2s.h>

#define MIC_WS   25
#define MIC_SCK  26
#define MIC_SD   33

#define SPK_LRC  14
#define SPK_BCLK 27
#define SPK_DIN  22

#define SAMPLE_RATE 16000
#define RECORD_SECONDS 3
#define TOTAL_SAMPLES (SAMPLE_RATE * RECORD_SECONDS)

// Буфер в оперативной памяти (~96 КБ, свободно помещается в ESP32)
int16_t audio_buffer[TOTAL_SAMPLES];
float volume = 0.5;

void setupI2SMic() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = MIC_SCK,
    .ws_io_num = MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_SD
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void setupI2SSpeaker() {
  const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = SPK_BCLK,
    .ws_io_num = SPK_LRC,
    .data_out_num = SPK_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pin_config);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupI2SMic();
  setupI2SSpeaker();
}

void loop() {
  int32_t raw_buf[512];
  size_t bytes_read = 0;
  size_t bytes_written = 0;

  // --- ЭТАП 1: ЗАПИСЬ ---
  Serial.println("\n>>> ГОВОРИТЕ (идет запись 3 сек)...");
  i2s_zero_dma_buffer(I2S_NUM_0);
  
  int recorded = 0;
  int32_t max_peak = 0;

  while (recorded < TOTAL_SAMPLES) {
    i2s_read(I2S_NUM_0, raw_buf, sizeof(raw_buf), &bytes_read, portMAX_DELAY);
    int samples = bytes_read / sizeof(int32_t);

    for (int i = 0; i < samples && recorded < TOTAL_SAMPLES; i++) {
      // INMP441 передает 24 бита со сдвигом влево в 32-битном слове.
      // Сдвиг >> 11 дает уверенную громкость голоса без потери разрядности
      int32_t val = raw_buf[i] >> 11; 
      
      if (val > 32767) val = 32767;
      if (val < -32768) val = -32768;

      audio_buffer[recorded++] = (int16_t)(val * volume);
      if (abs(val) > max_peak) max_peak = abs(val);
    }
  }

  Serial.print("Запись окончена. Пиковый уровень: ");
  Serial.println(max_peak);

  // --- ЭТАП 2: ВОСПРОИЗВЕДЕНИЕ ---
  Serial.println("<<< ВОСПРОИЗВЕДЕНИЕ...");
  i2s_zero_dma_buffer(I2S_NUM_1);
  i2s_write(I2S_NUM_1, audio_buffer, TOTAL_SAMPLES * sizeof(int16_t), &bytes_written, portMAX_DELAY);
  i2s_zero_dma_buffer(I2S_NUM_1);

  delay(1500);
}