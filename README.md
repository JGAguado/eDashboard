# eDashboard

Home Assistant custom component that generates dashboard images and EDB7 binary payloads for reTerminal/ESPHome clients.

## Project Layout

- `backend/`
- `custom_components/edashboard/`
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

Backend reads runtime settings from `backend/config.yaml`.

## Home Assistant Custom Component

This repository now includes a Home Assistant custom component in `custom_components/edashboard/`.
It runs the same backend renderer pipeline from Home Assistant and exposes endpoints for ESPHome/reTerminal clients.

### 1) Copy/Install the component

Import this repository in HACS as a custom repository, then install eDashboard.

Manual alternative: copy `custom_components/edashboard` into your Home Assistant config directory:

```bash
/config/custom_components/edashboard
```

Make sure this repository is available on the Home Assistant host (for example `/config/eDashboard`), because the integration reuses code and assets from `backend/`.

### 2) Add configuration

In `configuration.yaml`:

```yaml
edashboard:
	project_root: /config/eDashboard
	refresh_seconds: 300
	city_label: Vienna
	timezone: Europe/Vienna
	temp_unit: C
	wind_unit: km/h
	google_ical_url: ""
	# optional overrides:
	# latitude: 48.2082
	# longitude: 16.3738
	# output_dir: /config/www/edashboard/output
```

If `latitude` and `longitude` are omitted, Home Assistant core location is used.

### 3) Restart Home Assistant

After restart, the integration automatically generates images/binary at the configured interval and exposes:

- `/api/edashboard/health`
- `/api/edashboard/generate` (POST)
- `/api/edashboard/latest/png`
- `/api/edashboard/latest/dithered.png`
- `/api/edashboard/latest/bin`
- `/api/edashboard/latest/meta`

There is also a Home Assistant service:

- `edashboard.generate_now`

### 4) Point ESPHome/reTerminal to Home Assistant endpoint

Use Home Assistant as the image backend URL, for example:

```text
http://homeassistant.local:8123/api/edashboard/latest/bin
```

or with a fixed IP:

```text
http://192.168.1.10:8123/api/edashboard/latest/bin
```

Note: endpoints are intentionally exposed without authentication so LAN devices can fetch the binary directly.
