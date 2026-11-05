# ESP32 firmware latency testbed

Source code and measurement scripts for the article **"Experimental Evaluation of Responsiveness in Blocking, Non-Blocking, and FreeRTOS Multitasking Firmware Designs on ESP32 Sensor Nodes"** (Journal of Technology and Informatics, JoTI). The article link will be added after publication.

The experiment compares three firmware designs on one ESP32 sensor node (DUT) while a second ESP32 (tester) generates external events and timestamps the responses with 1 µs resolution.

| Design | Folder | Description |
|---|---|---|
| A | `dut_A_blocking` | Blocking, `delay(T)` |
| B | `dut_B_nonblocking` | Cooperative non-blocking, `millis()` scheduling |
| C | `dut_C_freertos` | FreeRTOS tasks pinned to core 1 (event task priority 3, network task 2, sensor task 1) |

## Repository layout

```
penguji/            tester firmware (pulse generator and timestamping)
dut_A_blocking/     DUT firmware, design A
dut_B_nonblocking/  DUT firmware, design B
dut_C_freertos/     DUT firmware, design C
python/             rekam.py (serial recorder), analisis.py (analysis), requirements.txt
python/data/        raw CSV recordings
python/hasil/       analysis output: tables, statistics, boxplot
```

Variable and comment names in the code are in Indonesian (`penguji` = tester, `kejadian` = event, `respons` = response, `skenario` = scenario).

## Wiring

| Tester | DUT | Signal |
|---|---|---|
| GPIO25 | GPIO26 | event pulse (20 ms) |
| GPIO32 | (jumper to tester GPIO25, on the tester) | pulse loopback, rising edge = t_in |
| GPIO33 | GPIO27 | DUT response, rising edge = t_out |
| GPIO35 | GPIO14 | sensor-task marker (for jitter) |
| GND | GND | common ground (required) |

The light sensor is an LDR from 3V3 to DUT GPIO34 (ADC1) with a 1.5 kΩ resistor from GPIO34 to GND.

For the calibration (loopback) test, disconnect the DUT and connect tester GPIO33 directly to the pulse line (GPIO25).

## Software versions used

- Arduino IDE 2.3.10, arduino-esp32 core 3.3.12, board "ESP32 Dev Module", 240 MHz
- PubSubClient 2.8
- Mosquitto 2.1.2 (MQTT broker on a laptop)
- Python 3.14.3, SciPy 1.18.1

## How to reproduce

1. **Configure the DUT.** Open `umum.h` and set `WIFI_SSID`, `WIFI_PASS`, and `MQTT_HOST` (the laptop IP running Mosquitto). The file must be identical in the three DUT folders.
2. **Start the broker** (scenarios S2 and S3 only). Example Mosquitto config (`joti.conf`):
   ```
   listener 1883 0.0.0.0
   allow_anonymous true
   ```
   ```
   mosquitto -c joti.conf -v
   ```
3. **Upload the tester firmware** (`penguji/penguji.ino`) to the tester ESP32.
4. **Select the scenario.** In `umum.h`, set `#define SKENARIO` to 1, 2, or 3, then upload one DUT design.

   | Scenario | Network task | Sensor period T |
   |---|---|---|
   | S1 | off | 1000 ms |
   | S2 | on | 1000 ms |
   | S3 | on | 100 ms |

5. **Record.** Install the dependencies (`pip install -r python/requirements.txt`) and run, for example for design A in scenario S1:
   ```
cd python
python rekam.py --port COM7 --desain A --skenario S1
   ```
   Replace `COM7` with the tester's serial port. Use `--kalibrasi` for the loopback calibration. Repeat for each design and scenario. Each run sends 1000 pulses (20 ms wide, random gap 50–500 ms, fixed seed 42, so the pulse sequence is identical in every run).
6. **Analyse.**
   ```
   python analisis.py --data data --hasil hasil
   ```
   This produces the latency table, the MER and jitter table, the statistical tests, and the boxplot.

## Notes

- The DUT's own serial output is not used for timing. All timestamps come from the tester.
- Keep the tester's Wi-Fi off and the DUT's Wi-Fi power saving disabled (already set in `umum.h`).
- Each design and scenario was recorded once with 1000 events.

## License

MIT
