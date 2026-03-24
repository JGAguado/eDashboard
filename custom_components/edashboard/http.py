from __future__ import annotations

from pathlib import Path
import logging
import re

from aiohttp import web

from homeassistant.components.http import HomeAssistantView
from homeassistant.core import HomeAssistant

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


def _iter_location_candidates(raw_value: str) -> list[str]:
    base = _sanitize_dashboard_name(raw_value)
    out: list[str] = []
    for item in [raw_value.strip(), raw_value.strip().lower(), base, f"weather_{base}"]:
        name = _sanitize_dashboard_name(item)
        if name and name not in out:
            out.append(name)
    return out


def _discover_generated_paths(base_output: Path, names: list[str]) -> list[Path]:
    paths: list[Path] = []

    # Direct deterministic candidates first.
    for name in names:
        paths.append(base_output / name / "latest_epd.png")

    # Single-dashboard fallback layout.
    paths.append(base_output / "latest_epd.png")

    # Directory scan fallback for near matches.
    if base_output.exists() and base_output.is_dir():
        try:
            children = [p for p in base_output.iterdir() if p.is_dir()]
        except OSError:
            children = []

        for child in children:
            child_name = _sanitize_dashboard_name(child.name)
            if not child_name:
                continue
            if child_name in names:
                paths.append(child / "latest_epd.png")
                continue

            for name in names:
                if child_name.endswith(f"_{name}") or name in child_name:
                    paths.append(child / "latest_epd.png")
                    break

    # Deduplicate while preserving order.
    unique: list[Path] = []
    seen: set[str] = set()
    for p in paths:
        key = str(p)
        if key in seen:
            continue
        seen.add(key)
        unique.append(p)
    return unique


_NO_CACHE_HEADERS = {
    "Cache-Control": "no-store, no-cache, must-revalidate, max-age=0",
    "Pragma": "no-cache",
    "Expires": "0",
}


class EDashboardLatestDashboardView(HomeAssistantView):
    url = "/api/edashboard/latest"
    name = "api:edashboard:latest_dashboard"
    requires_auth = False

    async def get(self, request: web.Request) -> web.Response:
        try:
            hass = _request_hass(request)
            dashboard_raw = (request.query.get("dashboard") or "").strip()
            if not dashboard_raw:
                raise web.HTTPBadRequest(text="Query parameter 'dashboard' is required")

            base_output = Path(hass.config.path("www", "edashboard", "output")).resolve()
            unique_names = _iter_location_candidates(dashboard_raw)

            path_candidates = _discover_generated_paths(base_output, unique_names)

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
    hass.http.register_view(EDashboardLatestDashboardView())
