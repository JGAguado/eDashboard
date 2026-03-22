# eDashboard Home Assistant Integration

This custom component runs the eDashboard rendering pipeline inside Home Assistant and exposes binary/image endpoints for ESPHome and reTerminal clients.

## configuration.yaml

```yaml
edashboard:
  refresh_seconds: 300
  city_label: eDashboard
  timezone: Europe/Vienna
  temp_unit: C
  wind_unit: km/h
  google_ical_url: ""
```

## Endpoints

- `/api/edashboard/health`
- `/api/edashboard/generate` (POST)
- `/api/edashboard/latest/png`
- `/api/edashboard/latest/dithered.png`
- `/api/edashboard/latest/bin`
- `/api/edashboard/latest/meta`

## Output Files

By default, generated files are stored in `/config/www/edashboard/output`.
There is no `latest/` folder. The files are:

- `latest_rgb.png`
- `latest_epd.png`
- `latest_epd.bin`
- `metadata.json`

## Home Assistant service

- `edashboard.generate_now`
