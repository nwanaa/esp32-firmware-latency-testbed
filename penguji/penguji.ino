/*
 * PENGUJI (ESP32 kedua) - membangkitkan pulsa kejadian dan mengukur waktu.
 *
 * Tugas:
 *   1. Mengirim N pulsa ke DUT (lebar tetap, jeda acak dengan seed tetap,
 *      sehingga urutan pulsa SAMA PERSIS untuk setiap desain dan skenario).
 *   2. Mencatat waktu tepi naik pulsa (t_in) dan tepi naik respons DUT (t_out)
 *      dengan interrupt + esp_timer_get_time() (resolusi 1 us).
 *   3. Mencatat waktu setiap penanda pembacaan sensor dari DUT (untuk jitter).
 *
 * Kedua tepi (pulsa dan respons) ditangkap dengan mekanisme interrupt yang sama,
 * sehingga keterlambatan masuk ISR pada penguji sebagian besar saling meniadakan.
 *
 * Rangkaian (lihat panduan):
 *   GPIO25 (PIN_PULSA_OUT) --+--> DUT GPIO26 (masukan kejadian)
 *                            +--> GPIO32 (PIN_PULSA_SENSE, jumper di penguji)
 *   DUT GPIO27 (keluaran respons) --> GPIO33 (PIN_RESPONS_IN)
 *   DUT GPIO14 (penanda sensor)   --> GPIO35 (PIN_PENANDA_IN)
 *   GND penguji <--> GND DUT  (WAJIB)
 *
 * Mode kalibrasi: lepas kabel dari DUT GPIO27, lalu hubungkan GPIO33 langsung
 * ke jalur pulsa (GPIO25). Latensi yang terukur = batas ketelitian alat ukur.
 *
 * Keluaran Serial (115200), dibaca oleh rekam.py:
 *   E,indeks,t_in_us,t_out_us,latensi_us,respons(1/0),macet(1/0)
 *   M,t_us
 *   DONE
 */

#include <Arduino.h>
#include "esp_timer.h"

// ===================== PARAMETER (isi ke Tabel/Metode artikel) =====================
const int      N_KEJADIAN      = 1000;   // jumlah pulsa per percobaan  -> N
const uint32_t LEBAR_PULSA_MS  = 20;     // lebar pulsa                 -> [20] ms
const uint32_t JEDA_MIN_MS     = 50;     // jeda acak minimum           -> [50] ms
const uint32_t JEDA_MAX_MS     = 500;    // jeda acak maksimum          -> [500] ms
const uint32_t SEED_ACAK       = 42;     // JANGAN diubah antar-percobaan
// ====================================================================================

const int PIN_PULSA_OUT   = 25;
const int PIN_PULSA_SENSE = 32;
const int PIN_RESPONS_IN  = 33;
const int PIN_PENANDA_IN  = 35;

volatile int64_t tIn = 0;
volatile int64_t tOut = 0;
volatile bool    adaRespons = false;
volatile bool    jendelaAktif = false;

// Penyangga cincin untuk penanda sensor (diisi ISR, dikuras loop utama)
const int UKURAN_BUF = 512;
volatile int64_t bufPenanda[UKURAN_BUF];
volatile uint16_t kepala = 0, ekor = 0;
volatile uint32_t penandaHilang = 0;

void IRAM_ATTR isrPulsa() {
  tIn = esp_timer_get_time();
}

void IRAM_ATTR isrRespons() {
  if (jendelaAktif && !adaRespons) {
    tOut = esp_timer_get_time();
    adaRespons = true;
  }
}

void IRAM_ATTR isrPenanda() {
  uint16_t berikut = (kepala + 1) % UKURAN_BUF;
  if (berikut == ekor) { penandaHilang++; return; }
  bufPenanda[kepala] = esp_timer_get_time();
  kepala = berikut;
}

void kurasPenanda() {
  while (ekor != kepala) {
    int64_t t = bufPenanda[ekor];
    ekor = (ekor + 1) % UKURAN_BUF;
    Serial.print("M,");
    Serial.println((long long)t);
  }
}

// delay yang tetap menguras penanda agar penyangga tidak penuh
void tungguSambilKuras(uint32_t ms) {
  uint32_t mulai = millis();
  while (millis() - mulai < ms) {
    kurasPenanda();
    delay(1);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PULSA_OUT, OUTPUT);
  digitalWrite(PIN_PULSA_OUT, LOW);
  pinMode(PIN_PULSA_SENSE, INPUT);
  pinMode(PIN_RESPONS_IN, INPUT_PULLDOWN);
  pinMode(PIN_PENANDA_IN, INPUT);          // GPIO35 tidak punya pull-up/down internal
  attachInterrupt(digitalPinToInterrupt(PIN_PULSA_SENSE), isrPulsa, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_RESPONS_IN), isrRespons, RISING);
  attachInterrupt(digitalPinToInterrupt(PIN_PENANDA_IN), isrPenanda, CHANGE);
  delay(500);
  Serial.println("# PENGUJI SIAP. Kirim 's' untuk mulai.");
}

void jalankanPercobaan() {
  randomSeed(SEED_ACAK);
  kepala = ekor = 0;
  penandaHilang = 0;
  Serial.printf("# MULAI N=%d lebar=%lu jeda=%lu-%lu seed=%lu\n",
                N_KEJADIAN, LEBAR_PULSA_MS, JEDA_MIN_MS, JEDA_MAX_MS, SEED_ACAK);

  for (int i = 0; i < N_KEJADIAN; i++) {
    bool macet = digitalRead(PIN_RESPONS_IN) == HIGH;  // keluaran DUT masih HIGH dari kejadian lalu
    adaRespons = false;
    tIn = 0;
    tOut = 0;
    jendelaAktif = true;
    digitalWrite(PIN_PULSA_OUT, HIGH);
    tungguSambilKuras(LEBAR_PULSA_MS);
    jendelaAktif = false;
    digitalWrite(PIN_PULSA_OUT, LOW);

    int64_t a = tIn, b = tOut;
    bool r = adaRespons;
    Serial.printf("E,%d,%lld,%lld,%lld,%d,%d\n", i, (long long)a,
                  (long long)(r ? b : -1), (long long)(r ? (b - a) : -1), r ? 1 : 0, macet ? 1 : 0);

    tungguSambilKuras(random(JEDA_MIN_MS, JEDA_MAX_MS + 1));
  }
  tungguSambilKuras(100);
  Serial.printf("# PENANDA_HILANG=%lu\n", penandaHilang);
  Serial.println("DONE");
}

void loop() {
     ekor = kepala;
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's') jalankanPercobaan();
  }
  delay(1);
}
