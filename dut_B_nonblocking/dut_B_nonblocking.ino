/*
 * DESAIN B - NON-BLOCKING KOOPERATIF
 * loop() tidak pernah menunggu. Setiap tugas dijalankan hanya bila jadwalnya tiba
 * (dicek dengan millis()), sehingga kejadian diperiksa pada setiap putaran.
 * Konfigurasi skenario ada di umum.h.
 */
#include "umum.h"

static uint32_t tSensor = 0;
static uint32_t tKirim  = 0;
static uint32_t tLapor  = 0;
static int nilaiTerakhir = 0;

void setup() {
  Serial.begin(115200);
  siapkanPin();
  siapkanJaringan();
  Serial.printf("# DUT DESAIN B (non-blocking), SKENARIO S%d\n", SKENARIO);
  tSensor = tKirim = tLapor = millis();
}

void loop() {
  uint32_t sekarang = millis();

  if (sekarang - tSensor >= PERIODE_SENSOR_MS) {
    tSensor += PERIODE_SENSOR_MS;          // jadwal tetap, tidak bergeser
    nilaiTerakhir = bacaSensor();
  }

  if (sekarang - tKirim >= PERIODE_KIRIM_MS) {
    tKirim += PERIODE_KIRIM_MS;
    kirimMqtt(nilaiTerakhir);
  }
  jagaMqtt();

  cekKejadian();

  if (sekarang - tLapor >= PERIODE_LAPOR_MS) { tLapor = sekarang; laporMemori(); }
}
