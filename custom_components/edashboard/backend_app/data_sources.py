from __future__ import annotations

from datetime import datetime, timezone
from typing import Any
import re

import requests


FORECAST_URL = (
    "https://api.open-meteo.com/v1/forecast"
    "?latitude={lat}&longitude={lon}"
    "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
    "surface_pressure,precipitation,wind_speed_10m,wind_direction_10m,weather_code,is_day,uv_index"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset"
    "&hourly=temperature_2m,precipitation_probability,weather_code,uv_index"
    "&forecast_days=4&timezone=auto"
)

AQI_URL = (
    "https://air-quality-api.open-meteo.com/v1/air-quality"
    "?latitude={lat}&longitude={lon}&hourly=european_aqi&timezone=auto"
)


def _get_json(url: str) -> dict[str, Any]:
    response = requests.get(url, timeout=30)
    response.raise_for_status()
    payload = response.json()
    if not isinstance(payload, dict):
        raise RuntimeError("Unexpected API response payload")
    return payload


def fetch_weather(lat: float, lon: float) -> dict[str, Any]:
    return _get_json(FORECAST_URL.format(lat=lat, lon=lon))


def fetch_aqi(lat: float, lon: float) -> dict[str, Any]:
    return _get_json(AQI_URL.format(lat=lat, lon=lon))


def _parse_ics_datetime(value: str) -> datetime | None:
    raw = value.strip()
    if not raw:
        return None

    # All-day events.
    if re.fullmatch(r"\d{8}", raw):
        dt = datetime.strptime(raw, "%Y%m%d")
        return dt.replace(tzinfo=timezone.utc)

    if raw.endswith("Z"):
        raw = raw[:-1]
        dt = datetime.strptime(raw, "%Y%m%dT%H%M%S")
        return dt.replace(tzinfo=timezone.utc)

    if re.fullmatch(r"\d{8}T\d{6}", raw):
        dt = datetime.strptime(raw, "%Y%m%dT%H%M%S")
        return dt.replace(tzinfo=timezone.utc)

    return None


def _unfold_ics_lines(text: str) -> list[str]:
    lines = text.replace("\r\n", "\n").split("\n")
    unfolded: list[str] = []
    for line in lines:
        if line.startswith((" ", "\t")) and unfolded:
            unfolded[-1] += line[1:]
        else:
            unfolded.append(line)
    return unfolded


def fetch_next_calendar_event(ical_url: str | None) -> dict[str, str] | None:
    if not ical_url:
        return None

    try:
        response = requests.get(ical_url, timeout=30)
        response.raise_for_status()
        lines = _unfold_ics_lines(response.text)
    except Exception:
        return None

    now = datetime.now(timezone.utc)
    in_event = False
    current_start: datetime | None = None
    current_summary = ""
    next_event: tuple[datetime, str] | None = None

    for line in lines:
        if line == "BEGIN:VEVENT":
            in_event = True
            current_start = None
            current_summary = ""
            continue

        if line == "END:VEVENT":
            if current_start and current_start >= now:
                if next_event is None or current_start < next_event[0]:
                    next_event = (current_start, current_summary or "Untitled")
            in_event = False
            continue

        if not in_event:
            continue

        if line.startswith("DTSTART"):
            _, value = line.split(":", 1)
            current_start = _parse_ics_datetime(value)
        elif line.startswith("SUMMARY"):
            _, value = line.split(":", 1)
            current_summary = value.strip()

    if not next_event:
        return None

    dt, title = next_event
    return {
        "title": title,
        "iso": dt.isoformat(),
    }
