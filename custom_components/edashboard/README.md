# eDashboard Home Assistant Integration

This custom component runs the eDashboard rendering pipeline inside Home Assistant and exposes binary/image endpoints for ESPHome and reTerminal clients.

## configuration.yaml

```yaml
edashboard:
  refresh_seconds: 300
  location: Vienna, Austria
  temp_unit: C
  wind_unit: km/h
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

## ESPHome client example

```yaml
substitutions:
  device_name: reterminal_1002
  friendly_name: eDashboard
  image_url: "http://homeassistant.local:8123/api/edashboard/latest/dithered.png"

esphome:
  name: ${device_name}
  friendly_name: ${friendly_name}

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

http_request:
  verify_ssl: false

spi:
  clk_pin: GPIO7
  mosi_pin: GPIO9

online_image:
  - id: dashboard_image
    url: ${image_url}
    format: png
    type: RGB565
    buffer_size: 65536
    on_download_finished:
      - component.update: epaper_display

display:
  - platform: epaper_spi
    id: epaper_display
    model: Seeed-reTerminal-E1002
    update_interval: never
    lambda: |-
      it.image(0, 0, id(dashboard_image));
```
