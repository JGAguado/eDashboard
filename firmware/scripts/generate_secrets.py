"""
PlatformIO pre-build script — generates include/secrets_gen.h from secrets file

This is a minimal YAML parser that handles only the flat key: value pairs
used in secrets files. No external YAML library is required.
"""

import os
import re

Import("env")  # noqa: F821  — PlatformIO magic


def parse_yaml(path):
    """Parse a very simple YAML file (flat key: value, no nesting except simple lists)."""
    data = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip()
            # Skip comments and blanks
            if not line or line.lstrip().startswith("#"):
                continue
            # Skip list items (we handle those separately)
            if line.lstrip().startswith("- "):
                continue
            m = re.match(r'^(\w+)\s*:\s*(.*)', line)
            if m:
                key = m.group(1)
                val = m.group(2).strip()
                # Remove surrounding quotes
                if (val.startswith('"') and val.endswith('"')) or \
                   (val.startswith("'") and val.endswith("'")):
                    val = val[1:-1]
                data[key] = val
    return data


def generate(source, target):
    data = parse_yaml(source)

    lines = [
        '#pragma once\n',
        '',
        '// ============================================================',
        '// AUTO-GENERATED from secrets.yaml — DO NOT EDIT BY HAND',
        '// ============================================================',
        '',
    ]

    def define_str(key, yaml_key, default=""):
        val = data.get(yaml_key, default)
        lines.append(f'#define {key} "{val}"')

    def define_num(key, yaml_key, default="0"):
        val = data.get(yaml_key, default)
        lines.append(f'#define {key} {val}')

    def define_float(key, yaml_key, default="0.0"):
        val = data.get(yaml_key, default)
        # Ensure it has a decimal point
        if '.' not in str(val):
            val = str(val) + ".0"
        lines.append(f'#define {key} {val}')

    # WiFi
    define_str("SECRET_WIFI_SSID", "wifi_ssid", "YOUR_WIFI_SSID")
    define_str("SECRET_WIFI_PASSWORD", "wifi_password", "YOUR_WIFI_PASSWORD")

    # Location
    define_float("SECRET_LATITUDE", "latitude", "40.4168")
    define_float("SECRET_LONGITUDE", "longitude", "-3.7038")

    # OpenWeatherMap
    define_str("SECRET_OWM_API_KEY", "openweathermap_api_key", "YOUR_OWM_API_KEY")

    # Google Calendar
    define_str("SECRET_GOOGLE_ICAL_URL", "google_ical_url", "YOUR_GOOGLE_ICAL_URL")

    # Time
    define_str("SECRET_TIMEZONE", "timezone", "CET-1CEST,M3.5.0,M10.5.0/3")
    define_str("SECRET_NTP_SERVER", "ntp_server", "pool.ntp.org")

    # Display
    define_str("SECRET_TEMP_UNIT", "temp_unit", "C")
    define_str("SECRET_WIND_UNIT", "wind_unit", "km/h")

    # Backend image URL
    define_str("SECRET_BACKEND_IMAGE_URL", "backend_image_url", "")

    # Sleep settings
    define_num("SECRET_NIGHT_START", "night_mode_start", "23")
    define_num("SECRET_NIGHT_END", "night_mode_end", "7")
    define_num("SECRET_SLEEP_MINUTES", "sleep_minutes", "30")

    lines.append('')  # trailing newline

    with open(target, "w", encoding="utf-8") as f:
        f.write('\n'.join(lines))

    print(f"[generate_secrets] Generated {target}")


# Paths
project_dir = env.subst("$PROJECT_DIR")
mysecrets_yaml = os.path.join(project_dir, "mysecrets.yaml")
fallback_yaml = os.path.join(project_dir, "secrets.yaml")
source_yaml = mysecrets_yaml if os.path.exists(mysecrets_yaml) else fallback_yaml
target_h = os.path.join(project_dir, "include", "secrets_gen.h")

# Ensure include dir exists
os.makedirs(os.path.dirname(target_h), exist_ok=True)

# Always regenerate to prevent stale secrets from previous builds.
generate(source_yaml, target_h)
