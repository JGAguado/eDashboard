from __future__ import annotations

from pathlib import Path
from typing import Any
import json
import logging
import re

from aiohttp import web

from homeassistant.components.http import HomeAssistantView
from homeassistant.core import HomeAssistant

from .const import DOMAIN

_LOGGER = logging.getLogger(__name__)


def _sanitize_dashboard_name(raw_name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_-]+", "_", raw_name.strip()).strip("_").lower()


def _request_hass(request: web.Request) -> HomeAssistant:
    hass = request.app.get("hass")
    if hass is None:
        hass = request.config_dict.get("hass")
    if hass is None:
        raise web.HTTPServiceUnavailable(text="Home Assistant context is not available in this request")
    return hass


def _state(hass: HomeAssistant) -> dict[str, Any]:
    state = hass.data.get(DOMAIN)
    if not isinstance(state, dict):
        raise web.HTTPServiceUnavailable(text="eDashboard integration is not loaded")
    runtimes = state.get("runtimes")
    if not isinstance(runtimes, dict) or not runtimes:
        raise web.HTTPServiceUnavailable(text="eDashboard integration has no configured dashboards")
    return state


def _runtime(hass: HomeAssistant, dashboard: str | None = None) -> Any:
    state = _state(hass)
    runtimes = state["runtimes"]
    name = (dashboard or state.get("default_dashboard") or "").strip().lower()
    runtime = runtimes.get(name)
    if runtime is None:
        raise web.HTTPNotFound(text=f"Unknown dashboard: {name}")
    return runtime


async def _generate_now(hass: HomeAssistant, dashboard: str | None = None) -> dict[str, Any]:
    runtime = _runtime(hass, dashboard)
    async with runtime.generate_lock:
        try:
            metadata = await hass.async_add_executor_job(runtime.service.generate_once)
        except Exception as exc:  # noqa: BLE001
            runtime.last_error = str(exc)
            _LOGGER.exception("Manual HTTP generation failed")
            raise web.HTTPInternalServerError(text=str(exc)) from exc

        runtime.last_error = None
        runtime.last_success = str(metadata.get("generated_at", ""))
        return metadata


def _ensure_file(path: Path, message: str) -> None:
    if not path.exists():
        raise web.HTTPNotFound(text=message)
    if not path.is_file():
        raise web.HTTPNotFound(text=message)


_NO_CACHE_HEADERS = {
    "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0",
    "Pragma": "no-cache",
    "Expires": "0",
}


async def _latest_file_bytes_response(
    runtime: Any,
    path: Path,
    not_found_message: str,
    content_type: str,
) -> web.Response:
    # Keep reads consistent with the generation transaction to avoid returning
    # mixed versions when a file is requested while a refresh is running.
    async with runtime.generate_lock:
        _ensure_file(path, not_found_message)
        try:
            body = path.read_bytes()
        except OSError as exc:
            _LOGGER.exception("Failed reading dashboard output file: %s", path)
            raise web.HTTPInternalServerError(text=f"Unable to read generated file: {exc}") from exc
    return web.Response(body=body, content_type=content_type, headers=_NO_CACHE_HEADERS)


class EDashboardHealthView(HomeAssistantView):
    url = "/api/edashboard/health"
    name = "api:edashboard:health"
    requires_auth = False

    async def get(self, request: web.Request) -> web.Response:
        hass = request.app["hass"]
        state = _state(hass)
        runtimes = state["runtimes"]
        dashboards: dict[str, Any] = {}
        for name, runtime in runtimes.items():
            dashboards[name] = {
                "last_success": runtime.last_success,
                "last_error": runtime.last_error,
                "output_dir": str(runtime.output_dir),
                "refresh_seconds": runtime.refresh_seconds,
            }

        payload = {
            "status": "ok",
            "default_dashboard": state.get("default_dashboard"),
            "dashboards": dashboards,
        }
        return web.json_response(payload)


class EDashboardGenerateView(HomeAssistantView):
    url = "/api/edashboard/generate"
    name = "api:edashboard:generate"
    requires_auth = False

    async def post(self, request: web.Request) -> web.Response:
        hass = request.app["hass"]
        dashboard = request.query.get("dashboard")
        metadata = await _generate_now(hass, dashboard)
        return web.json_response(metadata)


class EDashboardMetaView(HomeAssistantView):
    url = "/api/edashboard/latest/meta"
    name = "api:edashboard:latest_meta"
    requires_auth = False

    async def get(self, request: web.Request) -> web.Response:
        hass = request.app["hass"]
        runtime = _runtime(hass)
        async with runtime.generate_lock:
            path = runtime.service.metadata_path
            _ensure_file(path, "No metadata generated yet")

            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except Exception as exc:  # noqa: BLE001
                raise web.HTTPInternalServerError(text=f"Invalid metadata file: {exc}") from exc

        return web.json_response(data)


class EDashboardLatestPngView(HomeAssistantView):
    url = "/api/edashboard/latest/png"
    name = "api:edashboard:latest_png"
    requires_auth = False

    async def get(self, request: web.Request) -> web.Response:
        hass = request.app["hass"]
        runtime = _runtime(hass)
        return await _latest_file_bytes_response(
            runtime,
            runtime.service.rgb_path,
            "No PNG generated yet",
            "image/png",
        )


class EDashboardLatestDitheredView(HomeAssistantView):
    url = "/api/edashboard/latest/dithered.png"
    name = "api:edashboard:latest_dithered"
    requires_auth = False

    async def get(self, request: web.Request) -> web.Response:
        hass = request.app["hass"]
        runtime = _runtime(hass)
        return await _latest_file_bytes_response(
            runtime,
            runtime.service.dithered_path,
            "No dithered PNG generated yet",
            "image/png",
        )


class EDashboardLatestEpdView(HomeAssistantView):
    url = "/api/edashboard/latest/epd"
    name = "api:edashboard:latest_epd"
    requires_auth = False

    async def get(self, request: web.Request) -> web.Response:
        hass = request.app["hass"]
        runtime = _runtime(hass)
        return await _latest_file_bytes_response(
            runtime,
            runtime.service.dithered_path,
            "No dithered PNG generated yet",
            "image/png",
        )


class EDashboardLatestBinView(HomeAssistantView):
    url = "/api/edashboard/latest/bin"
    name = "api:edashboard:latest_bin"
    requires_auth = False

    async def get(self, request: web.Request) -> web.Response:
        hass = request.app["hass"]
        runtime = _runtime(hass)
        return await _latest_file_bytes_response(
            runtime,
            runtime.service.binary_path,
            "No binary payload generated yet",
            "application/octet-stream",
        )


class EDashboardNamedDitheredView(HomeAssistantView):
    url = "/api/edashboard/{dashboard}"
    name = "api:edashboard:named_dithered"
    requires_auth = False

    async def get(self, request: web.Request) -> web.Response:
        try:
            hass = _request_hass(request)
            dashboard_raw = request.match_info.get("dashboard", "")
            if not dashboard_raw.strip():
                raise web.HTTPNotFound(text="Dashboard name is required")

            base_output = Path(hass.config.path("www", "edashboard", "output")).resolve()
            names = [
                dashboard_raw.strip(),
                dashboard_raw.strip().lower(),
                _sanitize_dashboard_name(dashboard_raw),
            ]

            unique_names: list[str] = []
            for raw in names:
                name = _sanitize_dashboard_name(raw)
                if name and name not in unique_names:
                    unique_names.append(name)

            path_candidates: list[Path] = []
            for name in unique_names:
                # Multi-dashboard output layout.
                path_candidates.append(base_output / name / "latest_epd.png")

            # Single-dashboard fallback layout.
            path_candidates.append(base_output / "latest_epd.png")

            # Runtime fallback path (if integration state is available).
            try:
                runtime = _runtime(hass, unique_names[0] if unique_names else None)
                runtime_path = Path(runtime.service.dithered_path)
                if runtime_path not in path_candidates:
                    path_candidates.append(runtime_path)
            except web.HTTPException:
                pass

            for candidate in path_candidates:
                if not candidate.exists() or not candidate.is_file():
                    continue
                try:
                    body = candidate.read_bytes()
                except OSError as exc:
                    _LOGGER.exception("Failed reading dashboard output file: %s", candidate)
                    continue
                return web.Response(body=body, content_type="image/png", headers=_NO_CACHE_HEADERS)

            raise web.HTTPNotFound(text=f"No generated dithered image found for dashboard: {dashboard_raw}")
        except web.HTTPException:
            raise
        except Exception as exc:  # noqa: BLE001
            _LOGGER.exception("Unexpected error serving dashboard '%s'", dashboard_raw)
            raise web.HTTPNotFound(text=f"Dashboard API read error: {exc}") from exc


async def async_register_views(hass: HomeAssistant) -> None:
    # Expose only named dashboard images, e.g. /api/edashboard/weather_vienna.
    hass.http.register_view(EDashboardNamedDitheredView())
