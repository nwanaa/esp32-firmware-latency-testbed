"""
rekam.py - merekam satu percobaan dari ESP32 PENGUJI ke file CSV.

Contoh:
    python rekam.py --port COM7 --desain A --skenario S1
    python rekam.py --port COM7 --kalibrasi

Hasil (folder data/):
    <desain>_<skenario>_kejadian.csv  -> indeks,t_in_us,t_out_us,latensi_us,respons,macet
    <desain>_<skenario>_penanda.csv   -> t_us
    <desain>_<skenario>_log.txt       -> semua baris mentah + catatan waktu rekam
Kalibrasi disimpan sebagai KAL_KAL_*.csv.

Butuh: pip install pyserial
"""
import argparse
import datetime
import os
import sys
import time

import serial

# tanda ESP32 reset / data serial rusak (hanya diperiksa SETELAH "# MULAI";
# tulisan boot saat port dibuka adalah hal normal)
TANDA_SAMPAH = ("�", "rst:0x", "Brownout", "Guru Meditation", "PENGUJI SIAP")


def angka_semua(bagian):
    """True jika semua field adalah bilangan bulat (boleh negatif)."""
    return all(p.lstrip("-").isdigit() for p in bagian)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="port Serial ESP32 PENGUJI, mis. COM7")
    ap.add_argument("--desain", choices=["A", "B", "C"])
    ap.add_argument("--skenario", choices=["S1", "S2", "S3"])
    ap.add_argument("--kalibrasi", action="store_true", help="rekam uji kalibrasi alat ukur (tanpa DUT)")
    ap.add_argument("--folder", default="data")
    ap.add_argument("--timeout", type=int, default=1800, help="batas waktu rekam (detik)")
    a = ap.parse_args()

    if a.kalibrasi:
        nama = "KAL_KAL"
    else:
        if not (a.desain and a.skenario):
            sys.exit("Isi --desain dan --skenario, atau pakai --kalibrasi")
        nama = f"{a.desain}_{a.skenario}"

    os.makedirs(a.folder, exist_ok=True)
    f_kej = os.path.join(a.folder, f"{nama}_kejadian.csv")
    f_pen = os.path.join(a.folder, f"{nama}_penanda.csv")
    f_log = os.path.join(a.folder, f"{nama}_log.txt")
    if os.path.exists(f_kej):
        jawab = input(f"{f_kej} sudah ada. Timpa? (y/n) ")
        if jawab.lower() != "y":
            sys.exit("Dibatalkan.")

    ser = serial.Serial(a.port, 115200, timeout=1)
    time.sleep(2)                      # beri waktu ESP32 siap setelah port dibuka (port dibuka = ESP32 reset)
    ser.reset_input_buffer()
    ser.write(b"s")
    print(f"Mulai merekam {nama} ... (tekan Ctrl+C untuk batal)")

    n_kej = n_pen = n_resp = n_sampah = 0
    mulai_rekam = False
    selesai_normal = False
    t0 = time.time()
    t_mulai = None

    # encoding UTF-8 eksplisit supaya tidak memakai cp1252 bawaan Windows
    with open(f_kej, "w", encoding="utf-8", newline="") as fk, \
         open(f_pen, "w", encoding="utf-8", newline="") as fp, \
         open(f_log, "w", encoding="utf-8", newline="") as fl:
        fk.write("indeks,t_in_us,t_out_us,latensi_us,respons,macet\n")
        fp.write("t_us\n")
        fl.write(f"# direkam {datetime.datetime.now().isoformat()} port={a.port} nama={nama}\n")
        try:
            while True:
                if time.time() - t0 > a.timeout:
                    print("Batas waktu habis sebelum DONE. Data mungkin tidak lengkap.")
                    break
                baris = ser.readline().decode("utf-8", errors="replace").strip()
                if not baris:
                    continue
                fl.write(baris + "\n")        # log mentah tetap disimpan sebagai bukti

                if baris.startswith("# MULAI"):
                    mulai_rekam = True
                    t_mulai = time.time()
                    print(baris)
                    continue
                if not mulai_rekam:
                    continue                  # tulisan boot sebelum MULAI diabaikan (normal)

                # setelah MULAI: baris aneh = tanda penguji reset / data rusak
                if any(t in baris for t in TANDA_SAMPAH):
                    n_sampah += 1
                    print(f"  ! baris mencurigakan (#{n_sampah}) di kejadian {n_kej}: {baris[:60]!r}")
                    continue

                if baris.startswith("E,"):
                    bagian = baris.split(",")          # E, indeks, t_in, t_out, latensi, respons, macet
                    if len(bagian) != 7 or not angka_semua(bagian[1:]):
                        n_sampah += 1
                        print(f"  ! baris E rusak (#{n_sampah}): {baris[:60]!r}")
                        continue
                    fk.write(baris[2:] + "\n")
                    n_kej += 1
                    n_resp += int(bagian[5])
                    if n_kej % 100 == 0:
                        menit = (time.time() - t_mulai) / 60
                        print(f"  {n_kej} kejadian, {n_resp} direspons, {n_pen} penanda ({menit:.1f} menit)")
                elif baris.startswith("M,"):
                    bagian = baris.split(",")          # M, t_us
                    if len(bagian) != 2 or not angka_semua(bagian[1:]):
                        n_sampah += 1
                        print(f"  ! baris M rusak (#{n_sampah}): {baris[:60]!r}")
                        continue
                    fp.write(baris[2:] + "\n")
                    n_pen += 1
                elif baris.startswith("#"):
                    print(baris)
                elif baris == "DONE":
                    selesai_normal = True
                    break
        except KeyboardInterrupt:
            print("\nDibatalkan dengan Ctrl+C. Data belum lengkap.")
    ser.close()

    print(f"Selesai: {n_kej} kejadian ({n_resp} direspons), {n_pen} penanda, {n_sampah} baris mencurigakan.")
    if not selesai_normal:
        print("PERINGATAN: DONE tidak diterima, rekaman tidak lengkap. Ulangi percobaan.")
    elif n_sampah > 0:
        print("PERINGATAN: ada baris mencurigakan saat merekam. Periksa log.txt sebelum memakai data ini.")
    print(f"Tersimpan: {f_kej}, {f_pen}, {f_log}")


if __name__ == "__main__":
    main()