from __future__ import annotations

from datetime import datetime, timedelta
from functools import lru_cache
from pathlib import Path
from typing import Any, Literal
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError
import re

from PIL import Image, ImageChops, ImageDraw, ImageFont

from .config import AppConfig


ASSETS_DIR = Path(__file__).resolve().parents[1] / "assets"
ICONS_DIR = ASSETS_DIR / "icons"
FONTS_DIR = ASSETS_DIR / "fonts"

Alignment = Literal[
    "top-left", "top-right", "center-left", "center-right",
    "bottom-left", "bottom-right", "center"
]
Positioning = Literal["absolute", "relative"]
Size = tuple[int, int]

WEEKDAY_EN = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]


def _font(path: Path, size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    try:
        return ImageFont.truetype(str(path), size=size)
    except Exception:
        return ImageFont.load_default()


def _pick_font(name: str, fallback_dir: Path, size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    bundled = FONTS_DIR / name
    if bundled.exists():
        return _font(bundled, size)
    return _font(fallback_dir / "OpenSans-Regular.ttf", size)


def _safe_zoneinfo(name: str) -> ZoneInfo:
    try:
        return ZoneInfo(name)
    except ZoneInfoNotFoundError:
        return ZoneInfo("UTC")


def _temp_with_unit(celsius: float, unit: str) -> float:
    if unit == "F":
        return celsius * 9.0 / 5.0 + 32.0
    return celsius


def _wind_with_unit(kmh: float, unit: str) -> tuple[str, str]:
    if unit == "mph":
        return f"{kmh * 0.621371:.1f}", "mph"
    if unit == "m/s":
        return f"{kmh / 3.6:.1f}", "m/s"
    if unit == "knots":
        return f"{kmh * 0.539957:.1f}", "kt"
    return f"{kmh:.1f}", "km/h"


def _aqi_scale(aqi: float | None) -> str:
    if aqi is None:
        return "n/a"
    if aqi < 20:
        return "Very Good"
    if aqi < 40:
        return "Good"
    if aqi < 60:
        return "Neutral"
    if aqi < 80:
        return "Bad"
    return "Very Bad"


def _aqi_visual(aqi: float | None) -> tuple[str, tuple[int, int, int], str]:
    label = _aqi_scale(aqi)
    if label in ("Very Good", "Good"):
        return "aqi-good.png", (96, 150, 82), label
    if label == "Neutral":
        return "aqi-neutral.png", (216, 141, 73), label
    return "aqi-bad.png", (196, 80, 80), label


def _uv_scale(uv: float | None) -> str:
    if uv is None:
        return "n/a"
    if uv < 3:
        return "Low"
    if uv < 6:
        return "Moderate"
    if uv < 8:
        return "High"
    if uv < 11:
        return "Very High"
    return "Extreme"


def _uv_visual(uv: float | None) -> tuple[tuple[int, int, int], str]:
    label = _uv_scale(uv)
    if label == "Low":
        return (96, 150, 82), label
    if label == "Moderate":
        return (216, 141, 73), label
    if label == "High":
        return (235, 125, 40), label
    if label == "Very High":
        return (196, 80, 80), label
    if label == "Extreme":
        return (147, 92, 168), label
    return (120, 120, 120), label


def _fmt_time_24h(dt: datetime | None) -> str:
    if not dt:
        return "--:--"
    return dt.strftime("%H:%M")


def _posix_tz_utc_offset_hours(tz_string: str) -> float:
    # POSIX TZ has inverted sign: "CET-1" means UTC+1.
    m = re.search(r"([+-]?\d+(?:\.\d+)?)", tz_string)
    if not m:
        return 0.0
    try:
        return -float(m.group(1))
    except ValueError:
        return 0.0


def _get_local_now(cfg: AppConfig, weather: dict[str, Any]) -> datetime:
    # Always use configured timezone as source of truth for "current" clock time.
    try:
        return datetime.now(ZoneInfo(cfg.timezone)).replace(tzinfo=None)
    except ZoneInfoNotFoundError:
        # Fallback for POSIX-style TZ strings not available in zoneinfo DB.
        offset_h = _posix_tz_utc_offset_hours(cfg.timezone)
        return datetime.utcnow() + timedelta(hours=offset_h)


def _weather_icon_name(code: int, is_day: bool) -> str:
    day = "d" if is_day else "n"
    if code == 0:
        return f"01{day}.png"
    if code == 1:
        return f"022{day}.png"
    if code == 2:
        return f"02{day}.png"
    if code == 3:
        return "04d.png"
    if code in (45, 48):
        return "50d.png"
    if code in (51, 61, 80):
        return "51d.png"
    if code in (53, 63, 81):
        return "53d.png"
    if code in (55, 65, 82):
        return "09d.png"
    if code in (56, 66):
        return "56d.png"
    if code in (57, 67):
        return "57d.png"
    if code in (71, 85):
        return "71d.png"
    if code == 73:
        return "73d.png"
    if code in (75, 86):
        return "13d.png"
    if code == 77:
        return "77d.png"
    if code in (95, 96, 99):
        return "11d.png"
    return f"01{day}.png"


@lru_cache(maxsize=256)
def _load_icon_rgba(filename: str) -> Image.Image | None:
    p = ICONS_DIR / filename
    if not p.exists():
        return None
    try:
        return Image.open(p).convert("RGBA")
    except Exception:
        return None


def _resolve_anchor(x: int, y: int, size: Size, alignment: Alignment) -> tuple[int, int]:
    w, h = size
    if alignment == "top-left":
        return x, y
    if alignment == "top-right":
        return x - w, y
    if alignment == "center-left":
        return x, y - h // 2
    if alignment == "center-right":
        return x - w, y - h // 2
    if alignment == "bottom-left":
        return x, y - h
    if alignment == "bottom-right":
        return x - w, y - h
    return x - w // 2, y - h // 2


def _place_xy(x: int, y: int, size: Size, position: Positioning,
              origin: tuple[int, int], alignment: Alignment) -> tuple[int, int]:
    ax = x + origin[0] if position == "relative" else x
    ay = y + origin[1] if position == "relative" else y
    return _resolve_anchor(ax, ay, size, alignment)


def _text_size(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.ImageFont) -> Size:
    b = draw.textbbox((0, 0), text, font=font)
    return int(b[2] - b[0]), int(b[3] - b[1])


def _draw_icon(canvas: Image.Image, filename: str, x: int, y: int, size: Size,
               position: Positioning, alignment: Alignment,
               origin: tuple[int, int] = (0, 0), rotate_clockwise_deg: float = 0.0,
               tint: tuple[int, int, int] | None = None) -> None:
    icon = _load_icon_rgba(filename)
    if icon is None:
        return
    w, h = size
    base = icon.resize((w, h), Image.Resampling.LANCZOS)
    if rotate_clockwise_deg != 0.0:
        base = base.rotate(-rotate_clockwise_deg, resample=Image.Resampling.BICUBIC, expand=True)
        size = (base.width, base.height)
    if tint is not None:
        alpha = base.getchannel("A")
        colorized = Image.new("RGBA", base.size, (*tint, 0))
        colorized.putalpha(alpha)
        base = colorized
    px, py = _place_xy(x, y, size, position, origin, alignment)
    canvas.alpha_composite(base, (px, py))


def _draw_text(draw: ImageDraw.ImageDraw, text: str, font: ImageFont.ImageFont,
               fill: tuple[int, int, int], x: int, y: int,
               position: Positioning, alignment: Alignment,
               origin: tuple[int, int] = (0, 0)) -> tuple[int, int]:
    size = _text_size(draw, text, font)
    px, py = _place_xy(x, y, size, position, origin, alignment)
    draw.text((px, py), text, font=font, fill=fill)
    return px, py


def _smooth_points(points: list[tuple[int, int]], steps: int = 10) -> list[tuple[int, int]]:
    if len(points) < 3:
        return points
    out: list[tuple[int, int]] = []
    for i in range(len(points) - 1):
        p0 = points[max(0, i - 1)]
        p1 = points[i]
        p2 = points[i + 1]
        p3 = points[min(len(points) - 1, i + 2)]
        for s in range(steps):
            t = s / float(steps)
            t2 = t * t
            t3 = t2 * t
            x = 0.5 * (
                (2 * p1[0])
                + (-p0[0] + p2[0]) * t
                + (2 * p0[0] - 5 * p1[0] + 4 * p2[0] - p3[0]) * t2
                + (-p0[0] + 3 * p1[0] - 3 * p2[0] + p3[0]) * t3
            )
            y = 0.5 * (
                (2 * p1[1])
                + (-p0[1] + p2[1]) * t
                + (2 * p0[1] - 5 * p1[1] + 4 * p2[1] - p3[1]) * t2
                + (-p0[1] + 3 * p1[1] - 3 * p2[1] + p3[1]) * t3
            )
            out.append((int(x), int(y)))
    out.append(points[-1])
    return out


# ---- Widgets (all use x, y, size, position, alignment) ----
def draw_datetime_widget(draw: ImageDraw.ImageDraw, now_local: datetime,
                         x: int, y: int, size: Size,
                         position: Positioning, alignment: Alignment,
                         origin: tuple[int, int], font: ImageFont.ImageFont) -> None:
    _draw_text(draw, now_local.strftime("%A, %B %d"), font, (32, 32, 32), x, y, position, alignment, origin)


def draw_location_widget(draw: ImageDraw.ImageDraw, location_name: str,
                         x: int, y: int, size: Size,
                         position: Positioning, alignment: Alignment,
                         origin: tuple[int, int], font: ImageFont.ImageFont) -> None:
    _draw_text(draw, location_name, font, (32, 32, 32), x, y, position, alignment, origin)


def draw_header_widget(draw: ImageDraw.ImageDraw, now_local: datetime, location_name: str,
                       x: int, y: int, size: Size,
                       position: Positioning, alignment: Alignment,
                       origin: tuple[int, int],
                       location_font: ImageFont.ImageFont,
                       date_font: ImageFont.ImageFont) -> None:
    w, h = size
    px, py = _place_xy(x, y, size, position, origin, alignment)
    center_x = px + w // 2

    date_text = now_local.strftime("%A, %B %d")
    location_size = _text_size(draw, location_name, location_font)
    date_size = _text_size(draw, date_text, date_font)
    line_gap = 2
    block_h = location_size[1] + line_gap + date_size[1]
    top_y = py + max(0, (h - block_h) // 2)

    draw_location_widget(
        draw,
        location_name,
        center_x,
        top_y + location_size[1] // 2,
        (w, location_size[1]),
        "absolute",
        "center",
        (0, 0),
        location_font,
    )
    draw_datetime_widget(
        draw,
        now_local,
        center_x,
        top_y + location_size[1] + line_gap + date_size[1] // 2,
        (w, date_size[1]),
        "absolute",
        "center",
        (0, 0),
        date_font,
    )


def draw_update_time_widget(canvas: Image.Image, draw: ImageDraw.ImageDraw, now_local: datetime,
                            x: int, y: int, size: Size,
                            position: Positioning, alignment: Alignment,
                            origin: tuple[int, int], time_font: ImageFont.ImageFont) -> None:
    time_text = now_local.strftime("%H:%M")
    tw, th = _text_size(draw, time_text, time_font)
    icon_side = size[1]
    gap = 6
    total_w = icon_side + gap + tw
    container_pos = _place_xy(x, y, (total_w, size[1]), position, origin, alignment)
    icon_cx = container_pos[0] + icon_side // 2
    icon_cy = container_pos[1] + 6 + size[1] // 2
    _draw_icon(canvas, "refresh.png", icon_cx, icon_cy, (icon_side, icon_side), "absolute", "center")

    text_x = container_pos[0] + icon_side + gap
    text_y = container_pos[1] + max(0, (size[1] - th) // 2)
    draw.text((text_x, text_y), time_text, font=time_font, fill=(88, 88, 88))


def draw_weather_icon_widget(canvas: Image.Image, icon_name: str,
                             x: int, y: int, size: Size,
                             position: Positioning, alignment: Alignment,
                             origin: tuple[int, int]) -> None:
    _draw_icon(canvas, icon_name, x, y, size, position, alignment, origin)


def draw_current_temperature_widget(draw: ImageDraw.ImageDraw, value: float, unit: str,
                                    x: int, y: int, size: Size,
                                    position: Positioning, alignment: Alignment,
                                    origin: tuple[int, int],
                                    value_font: ImageFont.ImageFont, unit_font: ImageFont.ImageFont) -> None:
    value_txt = f"{value:.0f}"
    vw, vh = _text_size(draw, value_txt, value_font)
    uw, uh = _text_size(draw, "°" + unit, unit_font)
    group_w = vw + 4 + uw
    group_h = max(vh, uh)
    gx, gy = _place_xy(x, y, (group_w, group_h), position, origin, alignment)
    draw.text((gx, gy), value_txt, font=value_font, fill=(25, 25, 25))
    draw.text((gx + vw + 4, gy + 10), "°" + unit, font=unit_font, fill=(25, 25, 25))


def draw_minmax_temperature_widget(draw: ImageDraw.ImageDraw, tmin: float, tmax: float,
                                   x: int, y: int, size: Size,
                                   position: Positioning, alignment: Alignment,
                                   origin: tuple[int, int], font: ImageFont.ImageFont) -> None:
    _draw_text(draw, f"{tmin:.0f}° / {tmax:.0f}°", font, (90, 90, 90), x, y, position, alignment, origin)


def draw_metric_widget(canvas: Image.Image, draw: ImageDraw.ImageDraw,
                       icon: str, title: str, value: str, unit_desc: str,
                       x: int, y: int, size: Size,
                       position: Positioning, alignment: Alignment,
                       origin: tuple[int, int],
                       title_font: ImageFont.ImageFont,
                       value_font: ImageFont.ImageFont,
                       unit_font: ImageFont.ImageFont,
                       rotate_clockwise_deg: float = 0.0,
                       icon_tint: tuple[int, int, int] | None = None,
                       value_fill: tuple[int, int, int] = (20, 20, 20),
                       unit_fill: tuple[int, int, int] = (46, 46, 46)) -> None:
    w, h = size
    px, py = _place_xy(x, y, size, position, origin, alignment)
    icon_side = int(h * 0.7)
    icon_y = py + icon_side // 2
    _draw_icon(canvas, icon, px + icon_side // 2, icon_y + icon_side // 2, (icon_side, icon_side), "absolute", "center", rotate_clockwise_deg=rotate_clockwise_deg, tint=icon_tint)

    text_x = px + icon_side + 8
    draw.text((text_x, py), title, font=title_font, fill=(78, 78, 78))
    draw.text((text_x, py + int(h * 0.45)), value, font=value_font, fill=value_fill)
    if unit_desc:
        vx, _ = _text_size(draw, value, value_font)
        draw.text((text_x + vx + 4, py + int(h * 0.56)), unit_desc, font=unit_font, fill=unit_fill)


def draw_metrics_widget(canvas: Image.Image, draw: ImageDraw.ImageDraw,
                        metrics: list[dict[str, Any]],
                        x: int, y: int, size: Size,
                        position: Positioning, alignment: Alignment,
                        origin: tuple[int, int],
                        title_font: ImageFont.ImageFont,
                        value_font: ImageFont.ImageFont,
                        unit_font: ImageFont.ImageFont) -> None:
    w, h = size    
    v_gap = 8
    px, py = _place_xy(x, y, size, position, origin, alignment)
    cols = 2
    rows = 3
    cw = w // cols
    rh = (h - v_gap * (rows - 1)) // rows
    for i, m in enumerate(metrics[:6]):
        col = i // rows
        row = i % rows
        mx = px + col * cw
        my = py + row * rh + row * v_gap
        draw_metric_widget(
            canvas,
            draw,
            m["icon"],
            m["title"],
            m["value"],
            m.get("unit", ""),
            mx,
            my,
            (cw, rh),
            "absolute",
            "top-left",
            (0, 0),
            title_font,
            value_font,
            unit_font,
            rotate_clockwise_deg=float(m.get("rotate", 0.0)),
            icon_tint=m.get("icon_tint"),
            value_fill=tuple(m.get("value_color", (20, 20, 20))),
            unit_fill=tuple(m.get("unit_color", (46, 46, 46))),
        )


def draw_trend_widget(canvas: Image.Image, draw: ImageDraw.ImageDraw,
                      temps: list[float], pops: list[float], hours: list[str],
                      x: int, y: int, size: Size,
                      position: Positioning, alignment: Alignment,
                      origin: tuple[int, int],
                      font: ImageFont.ImageFont) -> None:
    w, h = size
    px, py = _place_xy(x, y, size, position, origin, alignment)
    axis_pad = 35
    plot_left = px + axis_pad
    plot_right = px + w - axis_pad
    plot_top = py + 4
    baseline_y = py + h - 22
    draw.line((plot_left, baseline_y, plot_right, baseline_y), fill=(190, 190, 190), width=3)

    series_len = min(12, len(temps), len(pops), len(hours))
    if series_len >= 2:
        temps_series = temps[:series_len]
        pops_series = pops[:series_len]
        tmin = min(temps_series)
        tmax = max(temps_series)
        pmin = min(pops_series)
        pmax = max(pops_series)
        tspan = max(1.0, tmax - tmin)
        pspan = max(1.0, pmax - pmin)
        bar_h = baseline_y - plot_top

        # Left axis (temperature) and right axis (rain probability), both full-range scaled.
        draw.text((px + 1, plot_top - 2), f"{tmax:.0f}º", font=font, fill=(120, 120, 120))
        draw.text((px + 1, baseline_y - 12), f"{tmin:.0f}º", font=font, fill=(120, 120, 120))
        draw.text((plot_right + 15, plot_top - 2), f"{pmax:.0f}%", font=font, fill=(120, 120, 120))
        draw.text((plot_right + 15, baseline_y - 12), f"{pmin:.0f}%", font=font, fill=(120, 120, 120))

        span_px = plot_right - plot_left
        x_points: list[int] = []
        for i in range(series_len):
            x_points.append(plot_left + int(round(i * span_px / max(1, series_len - 1))))

        rain_overlay = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        rain_draw = ImageDraw.Draw(rain_overlay)
        for i, p in enumerate(pops_series):
            center_x = x_points[i]
            if i == 0:
                x0 = plot_left
            else:
                x0 = (x_points[i - 1] + center_x) // 2

            if i == series_len - 1:
                x1 = plot_right
            else:
                x1 = (center_x + x_points[i + 1]) // 2

            if x1 <= x0:
                x1 = x0 + 1

            by = baseline_y - int(((p - pmin) / pspan) * bar_h)
            bar_span = max(1, baseline_y - by)
            for gy in range(by, baseline_y + 1):
                t = (gy - by) / bar_span
                alpha = int(170 * (1.0 - t))
                rain_draw.line((x0, gy, x1, gy), fill=(84, 154, 230, alpha), width=1)

        canvas.alpha_composite(rain_overlay)

        points: list[tuple[int, int]] = []
        for i, t in enumerate(temps_series):
            tx = x_points[i]
            ty = plot_top + int((tmax - t) * bar_h / tspan)
            points.append((tx, ty))

        smooth = _smooth_points(points, steps=10)
        trend_color = (225, 177, 104)

        # Fill area under the trend line with a vertical alpha gradient:
        # near the line -> trend color, near x-axis -> transparent.
        if len(smooth) >= 2:
            poly = list(smooth) + [(smooth[-1][0], baseline_y), (smooth[0][0], baseline_y)]
            mask = Image.new("L", canvas.size, 0)
            ImageDraw.Draw(mask).polygon(poly, fill=255)

            grad = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
            gdraw = ImageDraw.Draw(grad)
            y_top = max(plot_top, min(p[1] for p in smooth))
            y_bot = baseline_y
            span = max(1, y_bot - y_top)
            for gy in range(y_top, y_bot + 1):
                t = (gy - y_top) / span
                alpha = int(170 * (1.0 - t))
                gdraw.line((plot_left, gy, plot_right, gy), fill=(*trend_color, alpha), width=1)

            alpha = grad.split()[3]
            alpha = ImageChops.multiply(alpha, mask)
            grad.putalpha(alpha)
            canvas.alpha_composite(grad)

        draw.line(smooth, fill=trend_color, width=5)

    # Next 12h labels in 1h steps.
    tick_indices = list(range(0, series_len, 1))
    for idx in tick_indices:
        label = hours[idx]
        hx = plot_left + int(round(idx * (plot_right - plot_left) / max(1, series_len - 1)))
        hw, hh = _text_size(draw, label, font)
        draw.text((hx - hw // 2, baseline_y + 4), label, font=font, fill=(122, 122, 122))


def draw_day_forecast_widget(canvas: Image.Image, draw: ImageDraw.ImageDraw,
                             day_label: str, icon_name: str, tmin: float, tmax: float,
                             x: int, y: int, size: Size,
                             position: Positioning, alignment: Alignment,
                             origin: tuple[int, int],
                             day_font: ImageFont.ImageFont,
                             temp_font: ImageFont.ImageFont) -> None:
    w, h = size
    px, py = _place_xy(x, y, size, position, origin, alignment)
    draw.rounded_rectangle((px, py, px + w, py + h), radius=8, outline=(177, 177, 177), width=2)
    _draw_text(draw, day_label, day_font, (56, 56, 56), px + w // 2, py + 12, "absolute", "center")
    _draw_icon(canvas, icon_name, px + w // 2, py +  h // 2, (48, 48), "absolute", "center")
    _draw_text(draw, f"{tmin:.0f}° / {tmax:.0f}°", temp_font, (90, 90, 90), px + w // 2, py + 105, "absolute", "center")


def draw_daily_forecast_widget(canvas: Image.Image, draw: ImageDraw.ImageDraw,
                               days: list[dict[str, Any]],
                               x: int, y: int, size: Size,
                               position: Positioning, alignment: Alignment,
                               origin: tuple[int, int],
                               day_font: ImageFont.ImageFont,
                               temp_font: ImageFont.ImageFont) -> None:
    w, h = size
    px, py = _place_xy(x, y, size, position, origin, alignment)
    gap = 7
    card_w = (w - gap * 6) // 7
    for i in range(7):
        d = days[i] if i < len(days) else {
            "day": WEEKDAY_EN[i % 7],
            "icon": "01d.png",
            "min": 0.0,
            "max": 0.0,
        }
        draw_day_forecast_widget(
            canvas,
            draw,
            d["day"],
            d["icon"],
            float(d["min"]),
            float(d["max"]),
            px + i * (card_w + gap),
            py,
            (card_w, h),
            "absolute",
            "top-left",
            (0, 0),
            day_font,
            temp_font,
        )


def render_dashboard(
    cfg: AppConfig,
    weather: dict[str, Any],
    aqi: dict[str, Any],
    location_name: str | None = None,
) -> Image.Image:
    img = Image.new("RGBA", (cfg.width, cfg.height), (255, 255, 255, 255))
    draw = ImageDraw.Draw(img)

    # Fonts
    f_location = _pick_font("Jost-SemiBold.ttf", cfg.fonts_dir, 33)
    f_date = _pick_font("Jost.ttf", cfg.fonts_dir, 31)
    f_update = _pick_font("Jost.ttf", cfg.fonts_dir, 20)
    f_temp = _pick_font("Jost-SemiBold.ttf", cfg.fonts_dir, 84)
    f_unit = _pick_font("Jost-SemiBold.ttf", cfg.fonts_dir, 40)
    f_minmax = _pick_font("Jost.ttf", cfg.fonts_dir, 30)
    f_metric_title = _pick_font("Jost.ttf", cfg.fonts_dir, 18)
    f_metric_value = _pick_font("Jost-SemiBold.ttf", cfg.fonts_dir, 28)
    f_metric_unit = _pick_font("Jost.ttf", cfg.fonts_dir, 21)
    f_chart = _pick_font("Jost.ttf", cfg.fonts_dir, 16)
    f_forecast_day = _pick_font("Jost-SemiBold.ttf", cfg.fonts_dir, 22)
    f_forecast_temp = _pick_font("Jost.ttf", cfg.fonts_dir, 20)

    tz = _safe_zoneinfo(cfg.timezone)
    now_local = _get_local_now(cfg, weather)

    current = weather.get("current", {})
    daily = weather.get("daily", {})
    hourly = weather.get("hourly", {})

    cur_temp_c = float(current.get("temperature_2m", 0.0))
    cur_temp = _temp_with_unit(cur_temp_c, cfg.temp_unit)
    weather_code = int(current.get("weather_code", 0))
    is_day = int(current.get("is_day", 1)) == 1

    humidity = float(current.get("relative_humidity_2m", 0.0))
    pressure = float(current.get("surface_pressure", 0.0))
    wind_speed_kmh = float(current.get("wind_speed_10m", 0.0))
    wind_dir_deg = float(current.get("wind_direction_10m", 0.0))
    uv_index = current.get("uv_index")

    sunrise_dt = None
    sunset_dt = None
    if daily.get("sunrise"):
        try:
            sunrise_dt = datetime.fromisoformat(daily["sunrise"][0]).astimezone(tz).replace(tzinfo=None)
        except Exception:
            sunrise_dt = None
    if daily.get("sunset"):
        try:
            sunset_dt = datetime.fromisoformat(daily["sunset"][0]).astimezone(tz).replace(tzinfo=None)
        except Exception:
            sunset_dt = None

    aqi_values = aqi.get("hourly", {}).get("european_aqi", [])
    aqi_now = float(aqi_values[0]) if isinstance(aqi_values, list) and aqi_values else None

    # Top widgets

    header_location = (location_name or "Home").strip() or "Home"
    draw_header_widget(
        draw,
        now_local,
        header_location,
        cfg.width // 2,
        35,
        (cfg.width - 220, 56),
        "absolute",
        "center",
        (0, 0),
        f_location,
        f_date,
    )

    draw_update_time_widget(img, draw, now_local, 16, 13, (85, 22), "absolute", "top-left", (0, 0), f_update)

    # Current weather group
    cw_origin = (24, 90)
    cw_size = (370, 174)
    wx_icon = _weather_icon_name(weather_code, is_day=is_day)
    draw_weather_icon_widget(img, wx_icon, 0, 0, (174, 174), "relative", "top-left", cw_origin)
    draw_current_temperature_widget(draw, cur_temp, cfg.temp_unit, 190, 0, (190, 96), "relative", "top-left", cw_origin, f_temp, f_unit)

    tmin0 = _temp_with_unit(float(daily.get("temperature_2m_min", [cur_temp_c])[0]), cfg.temp_unit)
    tmax0 = _temp_with_unit(float(daily.get("temperature_2m_max", [cur_temp_c])[0]), cfg.temp_unit)
    draw_minmax_temperature_widget(draw, tmin0, tmax0, 195, 120, (160, 32), "relative", "bottom-left", cw_origin, f_minmax)

    # Metrics group
    right_x0 = 404
    metrics_origin = (right_x0, 80)
    metrics_size = (cfg.width - 10 - right_x0, 174)

    next_solar_label = "Sunrise"
    next_solar_time = _fmt_time_24h(sunrise_dt)
    if sunrise_dt and sunset_dt:
        if now_local < sunrise_dt:
            next_solar_label, next_solar_time = "Sunrise", _fmt_time_24h(sunrise_dt)
        elif now_local < sunset_dt:
            next_solar_label, next_solar_time = "Sunset", _fmt_time_24h(sunset_dt)
        else:
            sr = daily.get("sunrise", [])
            if len(sr) > 1:
                try:
                    next_solar_time = _fmt_time_24h(datetime.fromisoformat(sr[1]).astimezone(tz).replace(tzinfo=None))
                except Exception:
                    next_solar_time = _fmt_time_24h(sunrise_dt)
            next_solar_label = "Sunrise"

    wind_val, wind_unit = _wind_with_unit(wind_speed_kmh, cfg.wind_unit)
    aqi_icon, aqi_color, aqi_label = _aqi_visual(aqi_now)
    uv_color, uv_label = _uv_visual(float(uv_index) if uv_index is not None else None)
    metrics = [
        {"icon": "sunrise.png" if next_solar_label == "Sunrise" else "sunset.png", "title": next_solar_label, "value": next_solar_time, "unit": ""},
        {"icon": "arrow.png", "title": "Wind", "value": wind_val, "unit": wind_unit, "rotate": wind_dir_deg},
        {"icon": "humidity.png", "title": "Humidity", "value": f"{humidity:.0f}", "unit": "%"},
        {"icon": "pressure.png", "title": "Pressure", "value": f"{pressure:.0f}", "unit": "hPa"},
        {
            "icon": "uvi.png",
            "title": "UV Index",
            "value": f"{float(uv_index):.1f}" if uv_index is not None else "--",
            "unit": uv_label if uv_index is not None else "",
            # "icon_tint": uv_color,
        },
        {
            "icon": aqi_icon,
            "title": "Air Quality",
            "value": f"{aqi_now:.0f}" if aqi_now is not None else "n/a",
            "unit": aqi_label if aqi_now is not None else "",
            "icon_tint": aqi_color,
        },
    ]
    draw_metrics_widget(
        img,
        draw,
        metrics,
        0,
        0,
        metrics_size,
        "relative",
        "top-left",
        metrics_origin,
        f_metric_title,
        f_metric_value,
        f_metric_unit,
    )


    # 12h trend widget
    trend_origin = (20, 270)
    trend_size = (cfg.width - 60, 62)
    start_hour = now_local.replace(minute=0, second=0, microsecond=0)
    start_idx = 0
    hourly_times = hourly.get("time", [])
    for i, ts in enumerate(hourly_times):
        try:
            dt = datetime.fromisoformat(ts)
        except Exception:
            continue
        if dt >= start_hour:
            start_idx = i
            break

    window_end = start_idx + 12
    hours: list[str] = []
    for ts in hourly_times[start_idx:window_end]:
        try:
            dt = datetime.fromisoformat(ts)
            hours.append(dt.strftime("%H"))
        except Exception:
            hours.append("")
    temps = [float(v) for v in hourly.get("temperature_2m", [])[start_idx:window_end] if v is not None]
    pops = [float(v) for v in hourly.get("precipitation_probability", [])[start_idx:window_end] if v is not None]
    draw_trend_widget(img, draw, temps, pops, hours, 0, 0, trend_size, "relative", "top-left", trend_origin, f_chart)


    # Daily forecast widget (next 7 days, excluding today)
    day_times = daily.get("time", [])
    day_codes = daily.get("weather_code", [])
    day_max = daily.get("temperature_2m_max", [])
    day_min = daily.get("temperature_2m_min", [])

    base_next_day = now_local.date() + timedelta(days=1)
    if len(day_times) > 1:
        try:
            base_next_day = datetime.fromisoformat(day_times[1]).date()
        except Exception:
            pass

    days: list[dict[str, Any]] = []
    for i in range(7):
        src_i = i + 1
        if src_i < len(day_times):
            try:
                d = datetime.fromisoformat(day_times[src_i])
                day_label = WEEKDAY_EN[d.weekday()]
            except Exception:
                day_label = WEEKDAY_EN[(base_next_day + timedelta(days=i)).weekday()]
        else:
            day_label = WEEKDAY_EN[(base_next_day + timedelta(days=i)).weekday()]

        code = int(day_codes[src_i]) if src_i < len(day_codes) else weather_code
        tmin = _temp_with_unit(float(day_min[src_i]), cfg.temp_unit) if src_i < len(day_min) else cur_temp - 3
        tmax = _temp_with_unit(float(day_max[src_i]), cfg.temp_unit) if src_i < len(day_max) else cur_temp
        days.append({
            "day": day_label,
            "icon": _weather_icon_name(code, True),
            "min": tmin,
            "max": tmax,
        })

    draw_daily_forecast_widget(
        img,
        draw,
        days,
        10,
        346,
        (cfg.width - 20, 130),
        "absolute",
        "top-left",
        (0, 0),
        f_forecast_day,
        f_forecast_temp,
    )

    return img.convert("RGB")
