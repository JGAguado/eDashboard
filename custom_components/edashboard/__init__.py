from __future__ import annotations

from dataclasses import dataclass
from datetime import timedelta
from pathlib import Path
import asyncio
import logging
from typing import Any

import voluptuous as vol

from homeassistant.const import CONF_LATITUDE, CONF_LONGITUDE, EVENT_HOMEASSISTANT_STOP
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.event import async_track_time_interval

from .const import (
    CONF_LOCATION,
    CONF_OUTPUT_DIR,
    CONF_REFRESH_SECONDS,
    CONF_TEMP_UNIT,
    CONF_WIND_UNIT,
    DEFAULT_REFRESH_SECONDS,
    DOMAIN,
)
from .http import async_register_views
from .backend_app.config import AppConfig
from .backend_app.data_sources import geocode_location
from .backend_app.service import DashboardService

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = vol.Schema(
    {
        DOMAIN: vol.Schema(
            {
                vol.Optional(CONF_OUTPUT_DIR): cv.string,
                vol.Optional(CONF_LATITUDE): vol.Coerce(float),
                vol.Optional(CONF_LONGITUDE): vol.Coerce(float),
                vol.Optional(CONF_LOCATION): cv.string,
                vol.Optional(CONF_TEMP_UNIT): vol.In(["C", "F"]),
                vol.Optional(CONF_WIND_UNIT): vol.In(["km/h", "mph", "m/s", "knots"]),
                vol.Optional(CONF_REFRESH_SECONDS): vol.All(vol.Coerce(int), vol.Range(min=15)),
            }
        )
    },
    extra=vol.ALLOW_EXTRA,
)


@dataclass
class DashboardRuntime:
    service: Any
    output_dir: Path
    refresh_seconds: int
    generate_lock: Any
    unsub_interval: Any | None = None
    last_error: str | None = None
    last_success: str | None = None

def _build_runtime_config(
    hass: HomeAssistant,
    cfg: dict[str, Any],
    lat: float,
    lon: float,
    timezone: str,
    location_name: str,
) -> DashboardRuntime:

    # Home Assistant unit APIs changed across versions. Keep defaults stable and
    # infer metric from configured temperature unit when available.
    ha_temp_unit = str(getattr(hass.config.units, "temperature_unit", "")).upper()
    inferred_metric = "C" in ha_temp_unit

    temp_unit = str(cfg.get(CONF_TEMP_UNIT, "C" if inferred_metric else "F")).upper()
    wind_unit = str(cfg.get(CONF_WIND_UNIT, "km/h" if inferred_metric else "mph"))

    refresh_seconds = int(cfg.get(CONF_REFRESH_SECONDS, DEFAULT_REFRESH_SECONDS))

    output_dir = Path(cfg.get(CONF_OUTPUT_DIR, hass.config.path("www", "edashboard", "output"))).expanduser().resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    app_cfg = AppConfig(
        width=800,
        height=480,
        refresh_seconds=max(15, refresh_seconds),
        timezone=timezone,
        latitude=lat,
        longitude=lon,
        temp_unit=temp_unit,
        wind_unit=wind_unit,
        output_dir=output_dir,
        secrets_path=None,
        backend_config_path=Path(__file__).resolve().parent / "config.yaml",
        fonts_dir=Path(__file__).resolve().parent / "assets" / "fonts",
    )

    service = DashboardService(app_cfg, location_name)

    return DashboardRuntime(
        service=service,
        output_dir=output_dir,
        refresh_seconds=max(15, refresh_seconds),
        generate_lock=None,
    )


async def _generate_once(hass: HomeAssistant, runtime: DashboardRuntime, reason: str) -> dict[str, Any] | None:
    async with runtime.generate_lock:
        _LOGGER.debug("Generating eDashboard payload (%s)", reason)
        try:
            metadata = await hass.async_add_executor_job(runtime.service.generate_once)
        except Exception as exc:  # noqa: BLE001
            runtime.last_error = str(exc)
            _LOGGER.exception("eDashboard generation failed (%s)", reason)
            return None

        runtime.last_error = None
        runtime.last_success = str(metadata.get("generated_at", ""))
        return metadata


async def async_setup(hass: HomeAssistant, config: dict[str, Any]) -> bool:
    cfg = config.get(DOMAIN, {})

    try:
        location = str(cfg.get(CONF_LOCATION, "")).strip()
        if location:
            geo = await hass.async_add_executor_job(geocode_location, location)
            lat = float(geo["latitude"])
            lon = float(geo["longitude"])
            timezone = str(geo["timezone"])
            location_name = str(geo.get("name") or location)
            country = str(geo.get("country") or "").strip()
            if country:
                location_name = f"{location_name}, {country}"
        else:
            lat = float(cfg.get(CONF_LATITUDE, hass.config.latitude))
            lon = float(cfg.get(CONF_LONGITUDE, hass.config.longitude))
            timezone = str(hass.config.time_zone or "UTC")
            location_name = str(hass.config.location_name or "Home").strip() or "Home"

        runtime = _build_runtime_config(hass, cfg, lat, lon, timezone, location_name)
    except Exception as exc:  # noqa: BLE001
        _LOGGER.error("Failed to initialize eDashboard integration: %s", exc)
        return False

    runtime.generate_lock = asyncio.Lock()

    hass.data[DOMAIN] = runtime

    await async_register_views(hass)

    async def _handle_generate(call: ServiceCall) -> None:
        await _generate_once(hass, runtime, "service")

    if not hass.services.has_service(DOMAIN, "generate_now"):
        hass.services.async_register(DOMAIN, "generate_now", _handle_generate)

    await _generate_once(hass, runtime, "startup")

    async def _scheduled(_now) -> None:
        await _generate_once(hass, runtime, "scheduled")

    runtime.unsub_interval = async_track_time_interval(
        hass,
        _scheduled,
        timedelta(seconds=runtime.refresh_seconds),
    )

    async def _on_stop(_event) -> None:
        if runtime.unsub_interval:
            runtime.unsub_interval()
        if hass.services.has_service(DOMAIN, "generate_now"):
            hass.services.async_remove(DOMAIN, "generate_now")
        await hass.async_add_executor_job(runtime.service.stop)

    hass.bus.async_listen_once(EVENT_HOMEASSISTANT_STOP, _on_stop)

    _LOGGER.info(
        "eDashboard initialized. output_dir=%s refresh=%ss",
        runtime.output_dir,
        runtime.refresh_seconds,
    )

    return True
