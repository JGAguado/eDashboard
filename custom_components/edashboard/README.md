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

## Home Assistant service

- `edashboard.generate_now`
