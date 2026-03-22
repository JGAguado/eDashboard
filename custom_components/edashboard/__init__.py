from __future__ import annotations

from dataclasses import dataclass
from datetime import timedelta
from importlib import import_module
from pathlib import Path
import asyncio
import logging
import sys
from typing import Any

import voluptuous as vol

from homeassistant.const import CONF_LATITUDE, CONF_LONGITUDE, EVENT_HOMEASSISTANT_STOP
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.event import async_track_time_interval

from .const import (
    CONF_CITY_LABEL,
    CONF_GOOGLE_ICAL_URL,
    CONF_OUTPUT_DIR,
    CONF_PROJECT_ROOT,
    CONF_REFRESH_SECONDS,
    CONF_TEMP_UNIT,
    CONF_TIMEZONE,
    CONF_WIND_UNIT,
    DEFAULT_CITY_LABEL,
    DEFAULT_REFRESH_SECONDS,
    DOMAIN,
)
from .http import async_register_views

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = vol.Schema(
    {
        DOMAIN: vol.Schema(
            {
                vol.Optional(CONF_PROJECT_ROOT): cv.string,
                vol.Optional(CONF_OUTPUT_DIR): cv.string,
                vol.Optional(CONF_LATITUDE): vol.Coerce(float),
                vol.Optional(CONF_LONGITUDE): vol.Coerce(float),
                vol.Optional(CONF_TIMEZONE): cv.string,
                vol.Optional(CONF_TEMP_UNIT): vol.In(["C", "F"]),
                vol.Optional(CONF_WIND_UNIT): vol.In(["km/h", "mph", "m/s", "knots"]),
                vol.Optional(CONF_REFRESH_SECONDS): vol.All(vol.Coerce(int), vol.Range(min=15)),
                vol.Optional(CONF_CITY_LABEL): cv.string,
                vol.Optional(CONF_GOOGLE_ICAL_URL): cv.string,
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


@dataclass
class BackendModules:
    app_config_cls: Any
    dashboard_service_cls: Any


def _resolve_project_root(hass: HomeAssistant, cfg: dict[str, Any]) -> Path:
    configured = cfg.get(CONF_PROJECT_ROOT)
    if configured:
        return Path(configured).expanduser().resolve()

    # Repository checkout default: <repo>/custom_components/edashboard/__init__.py
    # -> <repo>
    return Path(__file__).resolve().parents[2]


def _load_backend_modules(project_root: Path) -> BackendModules:
    backend_app = project_root / "backend" / "app"
    if not backend_app.exists():
        raise RuntimeError(
            f"Backend app folder not found at {backend_app}. "
            "Set edashboard.project_root to your cloned project path."
        )

    root_str = str(project_root)
    if root_str not in sys.path:
        sys.path.insert(0, root_str)

    config_mod = import_module("backend.app.config")
    service_mod = import_module("backend.app.service")

    return BackendModules(
        app_config_cls=getattr(config_mod, "AppConfig"),
        dashboard_service_cls=getattr(service_mod, "DashboardService"),
    )


def _build_runtime_config(hass: HomeAssistant, cfg: dict[str, Any], modules: BackendModules, project_root: Path) -> DashboardRuntime:
    lat = float(cfg.get(CONF_LATITUDE, hass.config.latitude))
    lon = float(cfg.get(CONF_LONGITUDE, hass.config.longitude))

    timezone = str(cfg.get(CONF_TIMEZONE, hass.config.time_zone or "UTC"))

    is_metric = hass.config.units.is_metric
    temp_unit = str(cfg.get(CONF_TEMP_UNIT, "C" if is_metric else "F")).upper()
    wind_unit = str(cfg.get(CONF_WIND_UNIT, "km/h" if is_metric else "mph"))

    refresh_seconds = int(cfg.get(CONF_REFRESH_SECONDS, DEFAULT_REFRESH_SECONDS))
    city_label = str(cfg.get(CONF_CITY_LABEL, hass.config.location_name or DEFAULT_CITY_LABEL)).strip() or DEFAULT_CITY_LABEL
    google_ical_url = str(cfg.get(CONF_GOOGLE_ICAL_URL, "")).strip() or None

    output_dir = Path(cfg.get(CONF_OUTPUT_DIR, hass.config.path("www", "edashboard", "output"))).expanduser().resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    app_cfg = modules.app_config_cls(
        width=800,
        height=480,
        refresh_seconds=max(15, refresh_seconds),
        timezone=timezone,
        latitude=lat,
        longitude=lon,
        temp_unit=temp_unit,
        wind_unit=wind_unit,
        output_dir=output_dir,
        secrets_path=project_root / "firmware" / "mysecrets.yaml",
        backend_config_path=project_root / "backend" / "config.yaml",
        fonts_dir=project_root / "backend" / "assets" / "fonts",
        city_label=city_label,
        google_ical_url=google_ical_url,
    )

    service = modules.dashboard_service_cls(app_cfg, google_ical_url)

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
        project_root = _resolve_project_root(hass, cfg)
        modules = await hass.async_add_executor_job(_load_backend_modules, project_root)
        runtime = _build_runtime_config(hass, cfg, modules, project_root)
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
