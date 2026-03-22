from __future__ import annotations

from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from .config import ConfigError, load_config
from .service import DashboardService


try:
    app_config = load_config()
except ConfigError as exc:
    raise RuntimeError(f"Configuration error: {exc}") from exc

service = DashboardService(app_config, app_config.google_ical_url)

app = FastAPI(title="eDashboard Image Backend", version="0.1.0")
app.mount("/output", StaticFiles(directory=str(app_config.output_dir)), name="output")


@app.on_event("startup")
def startup_event() -> None:
    service.start()


@app.on_event("shutdown")
def shutdown_event() -> None:
    service.stop()


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.post("/generate")
def generate_now() -> dict:
    return service.generate_once()


@app.get("/latest/meta")
def latest_meta() -> dict:
    if not service.metadata_path.exists():
        raise HTTPException(status_code=404, detail="No generation yet")
    import json
    return json.loads(service.metadata_path.read_text(encoding="utf-8"))


@app.get("/latest/png")
def latest_png() -> FileResponse:
    if not service.rgb_path.exists():
        raise HTTPException(status_code=404, detail="No image yet")
    return FileResponse(path=service.rgb_path, media_type="image/png", filename="latest_rgb.png")


@app.get("/latest/dithered.png")
def latest_dithered() -> FileResponse:
    if not service.dithered_path.exists():
        raise HTTPException(status_code=404, detail="No dithered image yet")
    return FileResponse(path=service.dithered_path, media_type="image/png", filename="latest_epd.png")


@app.get("/latest/bin")
def latest_bin() -> FileResponse:
    if not service.binary_path.exists():
        raise HTTPException(status_code=404, detail="No binary yet")
    return FileResponse(path=service.binary_path, media_type="application/octet-stream", filename="latest_epd.bin")
