from __future__ import annotations

from pathlib import Path
from typing import Any
import json
import logging

from aiohttp import web

from homeassistant.components.http import HomeAssistantView
from homeassistant.core import HomeAssistant

from .const import DOMAIN

_LOGGER = logging.getLogger(__name__)


def _runtime(hass: HomeAssistant) -> Any:
    runtime = hass.data.get(DOMAIN)
    if runtime is None:
        raise web.HTTPServiceUnavailable(text="eDashboard integration is not loaded")
    return runtime


async def _generate_now(hass: HomeAssistant) -> dict[str, Any]:
    runtime = _runtime(hass)
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


_NO_CACHE_HEADERS = {
    "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0",
    "Pragma": "no-cache",
    "Expires": "0",
}


async def _latest_file_response(runtime: Any, path: Path, not_found_message: str) -> web.FileResponse:
    # Keep reads consistent with the generation transaction to avoid returning
    # mixed versions when a file is requested while a refresh is running.
    async with runtime.generate_lock:
        _ensure_file(path, not_found_message)
        return web.FileResponse(path, headers=_NO_CACHE_HEADERS)


class EDashboardHealthView(HomeAssistantView):
    url = "/api/edashboard/health"
    name = "api:edashboard:health"
    requires_auth = False

    async def get(self, request: web.Request) -> web.Response:
        hass = request.app["hass"]
        runtime = _runtime(hass)
        payload = {
            "status": "ok",
            "last_success": runtime.last_success,
            "last_error": runtime.last_error,
            "output_dir": str(runtime.output_dir),
            "refresh_seconds": runtime.refresh_seconds,
        }
        return web.json_response(payload)


class EDashboardGenerateView(HomeAssistantView):
    url = "/api/edashboard/generate"
    name = "api:edashboard:generate"
    requires_auth = False

    async def post(self, request: web.Request) -> web.Response:
        hass = request.app["hass"]
        metadata = await _generate_now(hass)
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

    async def get(self, request: web.Request) -> web.FileResponse:
        hass = request.app["hass"]
        runtime = _runtime(hass)
        return await _latest_file_response(runtime, runtime.service.rgb_path, "No PNG generated yet")


class EDashboardLatestDitheredView(HomeAssistantView):
    url = "/api/edashboard/latest/dithered.png"
    name = "api:edashboard:latest_dithered"
    requires_auth = False

    async def get(self, request: web.Request) -> web.FileResponse:
        hass = request.app["hass"]
        runtime = _runtime(hass)
        return await _latest_file_response(runtime, runtime.service.dithered_path, "No dithered PNG generated yet")


class EDashboardLatestBinView(HomeAssistantView):
    url = "/api/edashboard/latest/bin"
    name = "api:edashboard:latest_bin"
    requires_auth = False

    async def get(self, request: web.Request) -> web.FileResponse:
        hass = request.app["hass"]
        runtime = _runtime(hass)
        return await _latest_file_response(runtime, runtime.service.binary_path, "No binary payload generated yet")


async def async_register_views(hass: HomeAssistant) -> None:
    hass.http.register_view(EDashboardHealthView())
    hass.http.register_view(EDashboardGenerateView())
    hass.http.register_view(EDashboardMetaView())
    hass.http.register_view(EDashboardLatestPngView())
    hass.http.register_view(EDashboardLatestDitheredView())
    hass.http.register_view(EDashboardLatestBinView())
