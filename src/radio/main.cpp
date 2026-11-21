#include <Arduino.h>
#include <WiFi.h>
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#define SPK_BCLK 27
#define SPK_LRC  14
#define SPK_DIN  22


const char* ssid = "Frzwd";
const char* password = "vekc0453";

// Можно также протестировать поток с меньшим битрейтом (64 кбит/с вместо 128 кбит/с):
// "http://ice1.somafm.com/groovesalad-64-aac" (требует AudioGeneratorAAC) или MP3 128k:
const char* mp3_url = "http://ice1.somafm.com/groovesalad-128-mp3";

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceHTTPStream *file = nullptr;
AudioFileSourceBuffer *buff = nullptr;
AudioOutputI2S *out = nullptr;

// Задача воспроизведения на отдельном ядре
void audioTask(void *pvParameters) {
  while (true) {
    if (mp3 && mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        Serial.println("Поток завершен.");
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    // Короткая уступка планировщику без торможения декодера
    taskYIELD();
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  // КРИТИЧНО для аудио: запрещаем модулю Wi-Fi засыпать между пакетами
  WiFi.setSleep(false);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(1, 1, 1, 1), IPAddress(8, 8, 8, 8));
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi подключен!");
  Serial.printf("Свободная память (Heap) до буфера: %u байт\n", ESP.getFreeHeap());

  // Увеличиваем DMA-буферы I2S (последний параметр dma_buf_count = 16 вместо стандартных 8)
  out = new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S, 16);
  out->SetPinout(SPK_BCLK, SPK_LRC, SPK_DIN);
  out->SetOutputModeMono(true);
  out->SetGain(0.4); // Безопасная громкость для XHX 8 Ом 1 Вт

  file = new AudioFileSourceHTTPStream(mp3_url);
  file->SetReconnect(3, 500); // Автопереподключение при обрыве сокета

  // Увеличиваем кольцевой буфер с 8 КБ до 48 КБ (49152 байт)
  buff = new AudioFileSourceBuffer(file, 49152);

  mp3 = new AudioGeneratorMP3();
  mp3->begin(buff, out);

  Serial.printf("Свободная память (Heap) после буфера: %u байт\n", ESP.getFreeHeap());

  // Запускаем декодирование в отдельной задаче с высоким приоритетом на Core 1
  xTaskCreatePinnedToCore(
    audioTask,
    "AudioTask",
    8192,
    NULL,
    configMAX_PRIORITIES - 2,
    NULL,
    1
  );
}

void loop() {
  // Основной loop свободен для датчиков/логики, но не должен блокироваться длинными delay()
  vTaskDelay(pdMS_TO_TICKS(1000));
}