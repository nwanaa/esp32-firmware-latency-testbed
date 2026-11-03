/*
 * DESAIN C - MULTITASKING FreeRTOS
 * Tiga task terpisah, semuanya di core 1 (core yang sama dengan loop() pada desain A dan B):
 *   taskKejadian (prioritas 3) - menunggu notifikasi, lalu menjalankan cekKejadian()
 *   taskKirim    (prioritas 2) - tugas jaringan + laporan memori
 *   taskSensor   (prioritas 1) - pembacaan sensor periodik dengan vTaskDelayUntil()
 * ISR pada pin kejadian HANYA mengirim notifikasi (deferred interrupt handling);
 * pemrosesan respons tetap dilakukan di dalam task, bukan di ISR.
 * Konfigurasi skenario ada di umum.h.
 */
#include "umum.h"

static TaskHandle_t hKejadian = NULL;
static volatile int nilaiTerakhir = 0;

void IRAM_ATTR isrKejadian() {
  BaseType_t bangunkan = pdFALSE;
  vTaskNotifyGiveFromISR(hKejadian, &bangunkan);
  if (bangunkan) portYIELD_FROM_ISR();
}

void taskKejadian(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    cekKejadian();
  }
}

void taskSensor(void*) {
  TickType_t terakhir = xTaskGetTickCount();
  for (;;) {
    nilaiTerakhir = bacaSensor();
    vTaskDelayUntil(&terakhir, pdMS_TO_TICKS(PERIODE_SENSOR_MS));
  }
}

void taskKirim(void*) {
  uint32_t tKirim = millis(), tLapor = millis();
  for (;;) {
    uint32_t sekarang = millis();
    if (sekarang - tKirim >= PERIODE_KIRIM_MS) { tKirim += PERIODE_KIRIM_MS; kirimMqtt(nilaiTerakhir); }
    jagaMqtt();
    if (sekarang - tLapor >= PERIODE_LAPOR_MS) { tLapor = sekarang; laporMemori(); }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  siapkanPin();
  siapkanJaringan();
  Serial.printf("# DUT DESAIN C (FreeRTOS), SKENARIO S%d\n", SKENARIO);

  xTaskCreatePinnedToCore(taskKejadian, "kejadian", 4096, NULL, 3, &hKejadian, 1);
  xTaskCreatePinnedToCore(taskKirim,    "kirim",    8192, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskSensor,   "sensor",   4096, NULL, 1, NULL, 1);

  attachInterrupt(digitalPinToInterrupt(PIN_KEJADIAN_IN), isrKejadian, CHANGE);
}

void loop() {
  vTaskDelete(NULL);   // loop() Arduino tidak dipakai pada desain ini
}
