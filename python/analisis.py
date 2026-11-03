"""
analisis.py - mengolah semua CSV di folder data/ menjadi isi Tabel 3, Tabel 4,
uji statistik, dan Gambar 3 artikel.

Contoh:
    python analisis.py                 # baca data/, tulis ke hasil/
    python analisis.py --data data --hasil hasil

Keluaran (folder hasil/):
    tabel3_latensi.csv      -> Tabel 3 (median, persentil ke-95, maksimum, IQR; dalam us)
    tabel4_mer_jitter.csv   -> Tabel 4 (MER %, jitter ms)
    uji_statistik.txt       -> normalitas, Kruskal-Wallis, Dunn-Bonferroni, chi-kuadrat MER, kalibrasi
    gambar3_boxplot.png     -> Gambar 3 (300 dpi)

Butuh: pip install pandas numpy scipy matplotlib
"""
import argparse
import glob
import itertools
import os
import re

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from scipy import stats

PERIODE_SENSOR_MS = {"S1": 1000, "S2": 1000, "S3": 100}   # samakan dengan umum.h
NAMA_DESAIN = {"A": "A (blocking)", "B": "B (non-blocking)", "C": "C (FreeRTOS)"}
WARNA = {"A": "#2a78d6", "B": "#eb6834", "C": "#1baf7a"}
ALFA = 0.05


def muat(folder):
    data = {}
    for f in sorted(glob.glob(os.path.join(folder, "*_kejadian.csv"))):
        m = re.match(r"([ABC]|KAL)_(S[123]|KAL)_kejadian\.csv", os.path.basename(f))
        if not m:
            continue
        kunci = (m.group(1), m.group(2))
        kej = pd.read_csv(f)
        f_pen = f.replace("_kejadian.csv", "_penanda.csv")
        pen = pd.read_csv(f_pen) if os.path.exists(f_pen) else pd.DataFrame({"t_us": []})
        data[kunci] = (kej, pen)
    return data


def ringkas_latensi(kej):
    lat = kej.loc[kej.respons == 1, "latensi_us"].astype(float)
    if len(lat) == 0:
        return dict(n_respons=0, median=np.nan, p95=np.nan, maks=np.nan, iqr=np.nan)
    q1, q3 = np.percentile(lat, [25, 75])
    return dict(n_respons=len(lat), median=np.median(lat), p95=np.percentile(lat, 95),
                maks=lat.max(), iqr=q3 - q1)


def jitter_ms(pen, periode_ms):
    t = pen["t_us"].astype(float).values
    if len(t) < 3:
        return np.nan, np.nan, 0
    dt = np.diff(t)[1:] / 1000.0          # buang selang pertama (awal rekam)
    j = np.sqrt(np.mean((dt - periode_ms) ** 2))   # persamaan (3)
    return j, dt.mean(), len(dt)


def dunn_bonferroni(kelompok):
    """Uji Dunn dengan koreksi ties dan Bonferroni. kelompok: dict nama -> array."""
    nama = list(kelompok)
    semua = np.concatenate([kelompok[k] for k in nama])
    rank = stats.rankdata(semua)
    N = len(semua)
    _, hit = np.unique(semua, return_counts=True)
    koreksi = np.sum(hit ** 3 - hit) / (12 * (N - 1))
    rerata, ukuran, i0 = {}, {}, 0
    for k in nama:
        n = len(kelompok[k])
        rerata[k] = rank[i0:i0 + n].mean()
        ukuran[k] = n
        i0 += n
    pasangan = list(itertools.combinations(nama, 2))
    hasil = []
    for a, b in pasangan:
        se = np.sqrt((N * (N + 1) / 12 - koreksi) * (1 / ukuran[a] + 1 / ukuran[b]))
        z = (rerata[a] - rerata[b]) / se
        p = 2 * (1 - stats.norm.cdf(abs(z)))
        hasil.append((a, b, z, p, min(1.0, p * len(pasangan))))
    return hasil


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default="data")
    ap.add_argument("--hasil", default="hasil")
    a = ap.parse_args()
    os.makedirs(a.hasil, exist_ok=True)
    data = muat(a.data)
    if not data:
        raise SystemExit(f"Tidak ada file *_kejadian.csv di {a.data}/")

    baris3, baris4 = [], []
    for sk in ["S1", "S2", "S3"]:
        for d in ["A", "B", "C"]:
            if (d, sk) not in data:
                continue
            kej, pen = data[(d, sk)]
            n = len(kej)
            r = ringkas_latensi(kej)
            mer = 100.0 * (n - kej.respons.sum()) / n          # persamaan (2)
            j, rerata_dt, n_dt = jitter_ms(pen, PERIODE_SENSOR_MS[sk])
            baris3.append(dict(skenario=sk, desain=NAMA_DESAIN[d], n_respons=r["n_respons"],
                               median_us=r["median"], p95_us=r["p95"], maks_us=r["maks"], iqr_us=r["iqr"]))
            baris4.append(dict(skenario=sk, desain=d, n_kejadian=n, n_respons=int(kej.respons.sum()),
                               mer_persen=mer, macet=int(kej.macet.sum()), jitter_ms=j,
                               rerata_selang_ms=rerata_dt, n_selang=n_dt))
    t3 = pd.DataFrame(baris3).round(1)
    t4 = pd.DataFrame(baris4).round(2)
    t3.to_csv(os.path.join(a.hasil, "tabel3_latensi.csv"), index=False)
    t4.to_csv(os.path.join(a.hasil, "tabel4_mer_jitter.csv"), index=False)

    # ---------------- uji statistik ----------------
    out = []
    if ("KAL", "KAL") in data:
        r = ringkas_latensi(data[("KAL", "KAL")][0])
        out.append("KALIBRASI ALAT UKUR (latensi loopback, us)")
        out.append(f"  n={r['n_respons']} median={r['median']:.1f} p95={r['p95']:.1f} maks={r['maks']:.1f} IQR={r['iqr']:.1f}")
        out.append("  -> laporkan sebagai batas ketelitian pengukuran di bagian Metode.\n")

    for sk in ["S1", "S2", "S3"]:
        kel = {d: data[(d, sk)][0].query("respons == 1").latensi_us.astype(float).values
               for d in "ABC" if (d, sk) in data}
        if not kel:
            continue
        out.append(f"===== SKENARIO {sk} =====")
        for d, v in kel.items():
            if len(v) >= 3:
                sampel = v if len(v) <= 5000 else np.random.default_rng(0).choice(v, 5000, replace=False)
                w, p = stats.shapiro(sampel)
                out.append(f"  Shapiro-Wilk {d}: W={w:.3f}, p={p:.4g}, n={len(v)} -> {'normal' if p > ALFA else 'tidak normal'}")
            else:
                out.append(f"  Shapiro-Wilk {d}: n={len(v)} terlalu sedikit")
        valid = {d: v for d, v in kel.items() if len(v) >= 2}
        if len(valid) >= 2:
            h, p = stats.kruskal(*valid.values())
            out.append(f"  Kruskal-Wallis latensi: H({len(valid) - 1}) = {h:.2f}, p = {p:.4g}")
            for x, y, z, pm, pb in dunn_bonferroni(valid):
                out.append(f"    Dunn {x} vs {y}: z={z:.2f}, p_bonferroni={pb:.4g} {'(berbeda signifikan)' if pb < ALFA else ''}")
        else:
            out.append("  Kruskal-Wallis tidak dapat dihitung (kurang dari 2 kelompok dengan respons).")
        tabel = [[int(data[(d, sk)][0].respons.sum()), int((data[(d, sk)][0].respons == 0).sum())]
                 for d in "ABC" if (d, sk) in data]
        if len(tabel) >= 2 and np.all(np.array(tabel).sum(axis=0) > 0):
            c2, p, dof, _ = stats.chi2_contingency(tabel)
            out.append(f"  Chi-kuadrat MER: chi2({dof}) = {c2:.2f}, p = {p:.4g}")
        out.append("")
    teks = "\n".join(out)
    with open(os.path.join(a.hasil, "uji_statistik.txt"), "w") as f:
        f.write(teks)

    # ---------------- Gambar 3 ----------------
    sken = [sk for sk in ["S1", "S2", "S3"] if any((d, sk) in data for d in "ABC")]
    fig, axes = plt.subplots(1, len(sken), figsize=(6.3, 2.6), sharey=True, dpi=300)
    axes = np.atleast_1d(axes)
    for ax, sk in zip(axes, sken):
        ds = [d for d in "ABC" if (d, sk) in data]
        nilai = [data[(d, sk)][0].query("respons == 1").latensi_us.astype(float).values for d in ds]
        posisi = [i for i, v in enumerate(nilai) if len(v)]
        bp = ax.boxplot([nilai[i] for i in posisi], positions=posisi, widths=0.5, patch_artist=True,
                        showfliers=True, medianprops=dict(color="black", linewidth=1),
                        flierprops=dict(marker="o", markersize=2, alpha=0.4, markeredgewidth=0),
                        whiskerprops=dict(linewidth=0.8), capprops=dict(linewidth=0.8),
                        boxprops=dict(linewidth=0.8))
        for patch, i in zip(bp["boxes"], posisi):
            patch.set_facecolor(WARNA[ds[i]])
            patch.set_alpha(0.85)
        for i, v in enumerate(nilai):
            if len(v) == 0:
                ax.text(i, 1, "tidak ada\nrespons", ha="center", va="bottom", fontsize=6, color="#555555")
        ax.set_xticks(range(len(ds)))
        ax.set_xticklabels([f"{d}\nn={len(v)}" for d, v in zip(ds, nilai)], fontsize=7)
        ax.set_title(sk, fontsize=8)
        ax.set_yscale("log")
        ax.tick_params(axis="y", labelsize=7)
        ax.grid(axis="y", linewidth=0.3, color="#cccccc")
        ax.set_axisbelow(True)
        for s in ["top", "right"]:
            ax.spines[s].set_visible(False)
    axes[0].set_ylabel("Latensi respons (µs, skala log)", fontsize=7)
    fig.text(0.5, 0.005, "Desain firmware (A = blocking, B = non-blocking, C = FreeRTOS)", ha="center", fontsize=7)
    fig.tight_layout(rect=(0, 0.05, 1, 1))
    fig.savefig(os.path.join(a.hasil, "gambar3_boxplot.png"), dpi=300)

    pd.set_option("display.width", 140)
    print("\nTABEL 3 - Latensi (us)\n", t3.to_string(index=False))
    print("\nTABEL 4 - MER dan jitter\n", t4.to_string(index=False))
    print("\n" + teks)
    print(f"Semua hasil tersimpan di folder {a.hasil}/")


if __name__ == "__main__":
    main()
