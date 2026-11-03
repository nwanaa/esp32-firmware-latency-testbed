/*
 * DESAIN A - BLOCKING
 * Semua tugas dijalankan berurutan di loop() lalu menunggu dengan delay().
 * Selama delay() berjalan, kejadian tidak diperiksa.
 * Konfigurasi skenario ada di umum.h.
 */
#include "umum.h"

static uint32_t hitungLoop = 0;
static uint32_t tLapor = 0;

void setup() {
  Serial.begin(115200);
  siapkanPin();
  siapkanJaringan();
  Serial.printf("# DUT DESAIN A (blocking), SKENARIO S%d\n", SKENARIO);
}

void loop() {
  int nilai = bacaSensor();

  // kirim setiap (PERIODE_KIRIM / PERIODE_SENSOR) putaran, gaya khas kode blocking
  hitungLoop++;
  if (hitungLoop % (PERIODE_KIRIM_MS / PERIODE_SENSOR_MS) == 0) {
    kirimMqtt(nilai);
  }
  jagaMqtt();

  cekKejadian();

  if (millis() - tLapor >= PERIODE_LAPOR_MS) { tLapor = millis(); laporMemori(); }

  delay(PERIODE_SENSOR_MS);
}
