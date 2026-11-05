# Timbangan ICN Depok, multi menu (ESP32-S3)

Timbangan untuk area ICN: operator memilih menu (Pirolisis / Data Sampah
Hari ini / Incenerator), menimbang, lalu menekan **KIRIM**. Data tersimpan di
`monitoring.xampah.com`. Status timbangan, live log, dan update firmware OTA
bisa dipantau dari dashboard **Depok → Fleet (OTA) → Timbangan ICN**.

```
Indikator timbangan --UART RX18 (inverted, 9600)--> ┌──────────┐ -- LCD ST7920 (7/6/5)
                                                    │ ESP32-S3 │ -- keypad 8-11 / 12-14
                                                    └────┬─────┘ -- ENTER 4, KIRIM 2, BACK 3
          HTTPS: POST /api/scavengers/save-*             │  MQTT TLS :8883 (status, log, OTA)
                                                         ▼
                                           monitoring.xampah.com
```

Semua pin **sama persis** dengan firmware sebelumnya.

## Alur operator (tidak berubah)

1. Menu utama: ketik **1** (Registrasi) atau **2** (Menimbang), lalu **ENTER**.
2. Pilih menu: **2** naik, **8** turun, **ENTER** pilih, **BACK** kembali ke
   menu utama (otomatis setelah 30 detik tanpa tombol).
3. Timbang, lalu **KIRIM**. **BACK** kembali ke daftar menu.

| Menu | Endpoint baru | Endpoint lama |
|---|---|---|
| 1. Pirolisis | `POST /api/scavengers/save-pirolisis` | `/api/save-pirolisis` |
| 2. Data Sampah Hari ini | `POST /api/scavengers/save-icn-today` | `/api/sampah_hari_ini` |
| 3. Incenerator | `POST /api/scavengers/save-incenerator` | `/api/incinerator` |

Body `{"user_id":3,"weight":12.34}` + header `X-API-KEY`. `user_id` default 3
("admin", seperti firmware lama), bisa diganti lewat `DP_USER_ID` di
`secrets.ini`.

## Perbaikan dibanding versi lama

| Masalah lama | Sekarang |
|---|---|
| Layar hasil kirim tampil 2x + HTTP blocking: perangkat beku hingga ~16 detik | tampil 1x; HTTP di task terpisah, LCD & tombol tetap jalan |
| Berat NaN → LCD redraw tiap 10 ms; `"weight":nan` bisa terkirim | baris tak terbaca diabaikan; berat NaN/0/negatif ditolak di perangkat |
| Kabel timbangan lepas, berat terakhir tetap tampil & bisa dikirim | tampil **N/A** setelah 3 detik tanpa data, KIRIM ditolak |
| ENTER di menu utama langsung memilih menu pertama; BACK di layar timbang langsung keluar ke menu utama | tombol yang masih ditekan saat pindah layar diabaikan sampai dilepas |
| Watchdog dimatikan; kalau hang tidak pernah pulih | watchdog 30 detik pada loop, loop tidak pernah blocking |
| WiFi dimatikan total lalu `begin()` diulang saat masih connecting | reconnect terjadwal, tidak mengganggu koneksi yang sedang berjalan |
| HTTP polos ke `103.82.241.190`, tanpa API key | HTTPS ke `monitoring.xampah.com` (sertifikat diverifikasi) + API key |
| Tidak bisa dipantau / diupdate dari jauh | status & live log via MQTT, update OTA dengan SHA-256 dan auto-rollback |

## Kredensial

Di VPS (repo `API_Integration_IoT-Laravel12`):

```bash
bash scripts/provision-device.sh depok-icn-scale-01 --site depok_icn_scale
```

Salin `~/rmc-api-secrets/devices/depok-icn-scale-01.secrets.ini` ke repo ini
sebagai `secrets.ini` (sudah di-gitignore). Panduan lengkap:
`API_Integration_IoT-Laravel12/docs/depok-gateway-setup.md`, bagian
*Timbangan ICN Depok*.

## Build & flash

```bash
pio run -e release -t upload   # flash lewat USB (WAJIB untuk pemasangan pertama)
pio device monitor             # 115200, lewat USB native
pio run -e release             # image OTA -> .pio/build/release/firmware.bin
pio test -e native             # unit test lib/IcnScaleCore (butuh gcc/clang)
```

`pio run -e release` menolak compile tanpa `DP_API_KEY`, `DP_MQTT_USER`,
`DP_MQTT_PASS`: image tanpa MQTT tidak akan bisa di-OTA lagi.

## Update OTA

1. Naikkan `FIRMWARE_VERSION` di `include/config.h`.
2. `pio run -e release`.
3. Dashboard → Depok → Fleet (OTA) → **Timbangan ICN** → *Firmware*:
   upload `firmware.bin` dengan versi yang sama.
4. *Rollout* → deploy ke `depok-icn-scale-01`. Jangan saat ada yang sedang
   menimbang: perangkat reboot ±10 detik di akhir.

Image OTA di-cek SHA-256 sebelum dipakai; perintah OTA untuk site lain
ditolak; image baru yang gagal online 3x boot otomatis kembali ke versi lama.

## Status di dashboard

Setiap 10 detik: RSSI, heap, alasan reboot, status timbangan (LIVE / No
reading / Not connected) dan berat saat ini, layar & menu yang dipilih,
jumlah kiriman berhasil/gagal beserta kode HTTP terakhir. Live log berisi
setiap langkah (`[SEND]`, `[APP]`, `[SCALE]`, `[WIFI]`, `[MQTT]`, `[OTA]`).
