/*
 * umum.h - KONFIGURASI DAN FUNGSI BERSAMA UNTUK KETIGA DESAIN DUT.
 * File ini HARUS IDENTIK di folder dut_A_blocking, dut_B_nonblocking, dan dut_C_freertos.
 * Setelah mengubah satu salinan, salin ulang ke dua folder lainnya.
 */
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>   // Library Manager: "PubSubClient" oleh Nick O'Leary

// ===================== PILIH SKENARIO SEBELUM UPLOAD =====================
#define SKENARIO 2         // 1 = S1, 2 = S2, 3 = S3  (lihat Tabel 2 artikel)
// =========================================================================

// ---- Jaringan (diisi sesuai Wi-Fi dan alamat IP laptop yang menjalankan Mosquitto) ----
const char*    WIFI_SSID  = "YOUR_SSID";
const char*    WIFI_PASS  = "YOUR_PASSWORD";
const char*    MQTT_HOST  = "YOUR_BROKER_IP";   // IP laptop (cek dengan ipconfig / ip addr)
const uint16_t MQTT_PORT  = 1883;
const char*    MQTT_TOPIK = "joti/dut/sensor";

#if SKENARIO == 1
const bool     JARINGAN_AKTIF    = false;
const uint32_t PERIODE_SENSOR_MS = 1000;
#elif SKENARIO == 2
const bool     JARINGAN_AKTIF    = true;
const uint32_t PERIODE_SENSOR_MS = 1000;
#elif SKENARIO == 3
const bool     JARINGAN_AKTIF    = true;
const uint32_t PERIODE_SENSOR_MS = 100;
#else
#error "SKENARIO harus 1, 2, atau 3"
#endif
const uint32_t PERIODE_KIRIM_MS  = 5000;      // tugas jaringan 
const uint32_t PERIODE_LAPOR_MS  = 30000;     // laporan memori ke Serial

// ---- Pin DUT ----
const int PIN_KEJADIAN_IN  = 26;   // dari penguji GPIO25
const int PIN_RESPONS_OUT  = 27;   // ke penguji GPIO33
const int PIN_PENANDA_OUT  = 14;   // ke penguji GPIO35 (berubah tiap pembacaan sensor)
const int PIN_SENSOR_ADC   = 34;   // potensiometer / LDR (ADC1, aman dipakai bersama Wi-Fi)
const int JUMLAH_SAMPEL    = 32;   // pembacaan ADC dirata-rata

WiFiClient   klienWifi;
PubSubClient mqtt(klienWifi);

static bool statusPenanda = false;
static bool sudahRespons  = false;

void siapkanPin() {
  pinMode(PIN_KEJADIAN_IN, INPUT_PULLDOWN);
  pinMode(PIN_RESPONS_OUT, OUTPUT);
  digitalWrite(PIN_RESPONS_OUT, LOW);
  pinMode(PIN_PENANDA_OUT, OUTPUT);
  digitalWrite(PIN_PENANDA_OUT, LOW);
  analogReadResolution(12);
}

void siapkanJaringan() {
  if (!JARINGAN_AKTIF) {
    WiFi.mode(WIFI_OFF);
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);              // hemat daya Wi-Fi dimatikan agar kondisi sama di semua desain
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("# Menghubungkan Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print("."); }
  Serial.printf("\n# Wi-Fi OK, IP DUT: %s\n", WiFi.localIP().toString().c_str());
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

// Tugas periodik: membaca sensor. Pin penanda dibalik di AWAL pembacaan,
// sehingga penguji dapat mengukur selang antar-eksekusi (jitter).
int bacaSensor() {
  statusPenanda = !statusPenanda;
  digitalWrite(PIN_PENANDA_OUT, statusPenanda);
  uint32_t jumlah = 0;
  for (int k = 0; k < JUMLAH_SAMPEL; k++) jumlah += analogRead(PIN_SENSOR_ADC);
  return (int)(jumlah / JUMLAH_SAMPEL);
}

// Tugas jaringan: kirim nilai ke broker MQTT (menyambung ulang bila terputus).
void kirimMqtt(int nilai) {
  if (!JARINGAN_AKTIF) return;
  if (!mqtt.connected()) {
    mqtt.connect("joti-dut-esp32");
  }
  char pesan[48];
  snprintf(pesan, sizeof(pesan), "{\"t\":%lu,\"nilai\":%d}", millis(), nilai);
  mqtt.publish(MQTT_TOPIK, pesan);
}

void jagaMqtt() {
  if (JARINGAN_AKTIF) mqtt.loop();
}

// Tugas kejadian: respons = keluaran HIGH ketika masukan HIGH terdeteksi,
// kembali LOW setelah masukan terlihat LOW lagi.
void cekKejadian() {
  bool masuk = digitalRead(PIN_KEJADIAN_IN);
  if (masuk && !sudahRespons) {
    digitalWrite(PIN_RESPONS_OUT, HIGH);
    sudahRespons = true;
  } else if (!masuk && sudahRespons) {
    digitalWrite(PIN_RESPONS_OUT, LOW);
    sudahRespons = false;
  }
}

// Laporan memori: R,millis,sisa_heap,sisa_heap_minimum
void laporMemori() {
  Serial.printf("R,%lu,%u,%u\n", millis(), ESP.getFreeHeap(), ESP.getMinFreeHeap());
}
