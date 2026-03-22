from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import os
from typing import Any

import yaml


@dataclass
class AppConfig:
    width: int = 800
    height: int = 480
    refresh_seconds: int = 60
    timezone: str = "UTC"
    latitude: float = 0.0
    longitude: float = 0.0
    temp_unit: str = "C"
    wind_unit: str = "km/h"
    output_dir: Path = Path("backend/output")
    secrets_path: Path = Path("firmware/mysecrets.yaml")
    backend_config_path: Path = Path("backend/config.yaml")
    fonts_dir: Path = Path("firmware/aux_resources/fonts")
    city_label: str = "eDashboard"
    google_ical_url: str | None = None


class ConfigError(RuntimeError):
    pass


def _read_yaml(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise ConfigError(f"Secrets file not found: {path}")
    text = path.read_text(encoding="utf-8")
    data = yaml.safe_load(text) or {}
    if not isinstance(data, dict):
        raise ConfigError("Secrets file must contain a top-level YAML object")
    return data


def _read_yaml_optional(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8")
    data = yaml.safe_load(text) or {}
    if not isinstance(data, dict):
        raise ConfigError(f"Backend config file must contain a top-level YAML object: {path}")
    return data


def load_config() -> AppConfig:
    repo_root = Path(os.getenv("APP_ROOT", ".")).resolve()
    secrets_path = Path(os.getenv("SECRETS_PATH", repo_root / "firmware/mysecrets.yaml")).resolve()
    backend_config_path = Path(os.getenv("BACKEND_CONFIG_PATH", repo_root / "backend/config.yaml")).resolve()
    output_dir = Path(os.getenv("OUTPUT_DIR", repo_root / "backend/output")).resolve()
    fonts_dir = Path(os.getenv("FONTS_DIR", repo_root / "firmware/aux_resources/fonts")).resolve()

    payload = _read_yaml(secrets_path)
    backend_payload = _read_yaml_optional(backend_config_path)

    latitude = float(payload.get("latitude", 0.0))
    longitude = float(payload.get("longitude", 0.0))
    if latitude == 0.0 and longitude == 0.0:
        raise ConfigError("latitude and longitude are required in firmware/mysecrets.yaml")

    timezone = str(os.getenv("TIMEZONE", backend_payload.get("timezone", "UTC"))).strip() or "UTC"
    temp_unit = str(os.getenv("TEMP_UNIT", backend_payload.get("temp_unit", "C"))).strip().upper()
    wind_unit = str(os.getenv("WIND_UNIT", backend_payload.get("wind_unit", "km/h"))).strip()
    google_ical_url = str(os.getenv("GOOGLE_ICAL_URL", backend_payload.get("google_ical_url", ""))).strip() or None

    refresh_seconds = int(os.getenv("REFRESH_SECONDS", str(backend_payload.get("refresh_seconds", 60))))
    city_label = str(os.getenv("CITY_LABEL", backend_payload.get("city_label", "eDashboard"))).strip() or "eDashboard"

    return AppConfig(
        timezone=timezone,
        latitude=latitude,
        longitude=longitude,
        temp_unit=temp_unit,
        wind_unit=wind_unit,
        refresh_seconds=max(15, refresh_seconds),
        output_dir=output_dir,
        secrets_path=secrets_path,
        backend_config_path=backend_config_path,
        fonts_dir=fonts_dir,
        city_label=city_label,
        google_ical_url=google_ical_url,
    )
