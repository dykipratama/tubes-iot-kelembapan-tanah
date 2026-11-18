# Real-Time Monitoring & Klasifikasi Cerdas Kelembapan Tanah (ESP32)

Repositori Tugas Besar Mata Kuliah Internet of Things — Program Studi S1 Teknik Telekomunikasi, Universitas Telkom Purwokerto (2026).

## Anggota Kelompok
- **Stefanus Dicky Pratama Terang** (2311101103) — Hardware & Sensor
- **Abadi Bastanta Sitepu** (2311101099) — Eksperimen Protokol
- **Muhammad Wildan Tsaqif** (2311101085) — Analisis Data & Laporan

## Deskripsi Singkat
Sistem pemantauan kelembapan tanah real-time menggunakan mikrokontroler ESP32 DevKit V1, sensor kelembapan tanah analog (pin `GPIO 34` / `ADC1_CH6`), dan platform cloud ThingSpeak (Channel ID `3566273`) dengan interval pengiriman 30 detik. Sistem ini mencakup klasifikasi tiga kondisi media tanah (**Kering**, **Lembap**, **Sangat Basah**) berbasis kalibrasi linier dua titik serta pengujian komparasi performa protokol **HTTP POST** dan **MQTT (QoS 0 & QoS 1)** pada payload 20 byte dan 500 byte.

## Dua Program Utama ESP32 (`firmware/`)
1. **`firmware/monitoring_thingspeak/monitoring_thingspeak.ino`** (Kodingan 1 — Sistem Monitoring Real-Time Bagian A):
   Membaca sinyal tegangan analog dari sensor kelembapan tanah pada pin `GPIO 34`, mengonversinya menjadi persentase kelembapan (`0%–100%` menggunakan kalibrasi `ADC 3200` untuk kering dan `ADC 1300` untuk basah), lalu mengirimkannya ke server ThingSpeak (`api.thingspeak.com`) melalui HTTP POST setiap 30 detik.
2. **`firmware/eksperimen_protokol/eksperimen_s1_s6_final.ino`** (Kodingan 2 — Eksperimen Komparasi Protokol S1–S6 Bagian B):
   Menjalankan pengujian enam skenario pengiriman pesan secara berurutan (**HTTP POST** ke `httpbin.org` serta **MQTT QoS 0 dan QoS 1** ke `broker.hivemq.com`) dengan ukuran payload `20 byte` dan `500 byte` (masing-masing 100 pesan per skenario, jeda 2 detik) serta mencetak metrik `RTT` dan `RSSI` dalam format CSV ke Serial Monitor.

## Struktur Direktori
```text
tubes-iot-kelembapan-tanah/
├── README.md
├── firmware/
│   ├── monitoring_thingspeak/
│   │   ├── monitoring_thingspeak.ino
│   │   └── secrets.h.example
│   └── eksperimen_protokol/
│       ├── eksperimen_s1_s6_final.ino
│       └── secrets.h.example
├── data/
│   ├── monitoring/
│   │   ├── feeds.csv
│   │   └── label_log.csv
│   └── eksperimen/
│       ├── exp_S1.csv
│       ├── exp_S2.csv
│       ├── exp_S3.csv
│       ├── exp_S4.csv
│       ├── exp_S5.csv
│       ├── exp_S6.csv
│       └── exp_metadata.txt
├── scripts/
│   ├── requirements.txt
│   ├── analisis_dataset.py
│   └── analisis_eksperimen.py
└── results/
    └── fig_C1_rtt_boxplot_cdf.png
