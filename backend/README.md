# eDashboard Server-Client Backend

This backend runs in Docker and generates display assets every minute:

- `latest_rgb.png`: full-color source render
- `latest_epd.png`: 7-color dithered image for Spectra displays
- `latest_epd.bin`: packed binary payload for ESP32 download
- `metadata.json`: generation metadata and SHA256

The visual layout is weather-dashboard oriented, inspired by InkyPi's weather plugin style.

## Run With Docker

From the project root:

```bash
docker compose up --build
```

Service URL:

- `http://localhost:8090/health`
- `http://localhost:8090/latest/png`
- `http://localhost:8090/latest/dithered.png`
- `http://localhost:8090/latest/bin`
- `http://localhost:8090/latest/meta`
- `POST http://localhost:8090/generate` to force refresh

## Configuration

The service reads location secrets from `firmware/mysecrets.yaml`:

- `latitude`
- `longitude`

The service reads non-secret backend settings from `backend/config.yaml`:

- `refresh_seconds`
- `city_label`
- `timezone`
- `temp_unit`
- `wind_unit`
- `google_ical_url` (optional, for next-event line)

Optional environment variables in `docker-compose.yml`:

- `REFRESH_SECONDS` (default `60`)
- `CITY_LABEL` (title text)
- `BACKEND_CONFIG_PATH` (defaults to `/app/backend/config.yaml`)
- `TIMEZONE`, `TEMP_UNIT`, `WIND_UNIT`, `GOOGLE_ICAL_URL`

## Binary Format (`EDB7-v1`)

`latest_epd.bin` uses little-endian header + packed 4bpp color indices.

Header layout:

1. `magic` (4 bytes): `EDB7`
2. `version` (1 byte): `1`
3. `width` (uint16)
4. `height` (uint16)
5. `format` (1 byte): `1` = packed 4bpp indices
6. `palette_count` (1 byte): `7`
7. `reserved` (uint16)
8. `payload_len` (uint32)

Payload:

- Two pixels per byte: high nibble = first pixel, low nibble = second pixel.
- Pixel order is row-major from top-left to bottom-right.

Palette index map:

- `0` white `(255,255,255)`
- `1` black `(0,0,0)`
- `2` green `(0,160,70)`
- `3` blue `(0,95,200)`
- `4` red `(220,0,0)`
- `5` yellow `(245,205,0)`
- `6` orange `(250,120,0)`

## Notes for ESP32 Client

ESP32 flow should be:

1. GET `/latest/bin`
2. Validate `magic == EDB7`, dimensions match panel
3. Read payload and unpack nibbles into palette indices
4. Map indices to panel color constants and draw to the frame buffer
5. Refresh display
