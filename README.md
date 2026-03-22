# eDashboard

Repository organized into two main parts:

- `backend/`: Python FastAPI image backend (render, dither, binary output).
- `firmware/`: ESP32 PlatformIO firmware for the e-paper device.

## Project Layout

- `backend/`
- `firmware/`
- `docker-compose.yml`
- `README.md`

## Backend (Docker)

From repository root:

```bash
docker compose up --build
```

Main endpoints:

- `http://localhost:8090/health`
- `http://localhost:8090/latest/png`
- `http://localhost:8090/latest/dithered.png`
- `http://localhost:8090/latest/bin`
- `http://localhost:8090/latest/meta`

Backend reads location from `firmware/mysecrets.yaml` and non-secret runtime settings from `backend/config.yaml`.

## Firmware (PlatformIO)

From repository root:

```bash
cd firmware
pio run
```

Optional upload:

```bash
pio run -t upload
```

The firmware secrets header is generated from:

- `firmware/mysecrets.yaml` (preferred)
- `firmware/secrets.yaml` (fallback)

via `firmware/scripts/generate_secrets.py`.
