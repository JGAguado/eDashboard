#include "backend_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

static const char *TAG = "BackendClient";

struct Edb7Header {
    char magic[4];
    uint8_t version;
    uint16_t width;
    uint16_t height;
    uint8_t format;
    uint8_t paletteCount;
    uint16_t reserved;
    uint32_t payloadLen;
};

static uint16_t idxToColor(uint8_t idx) {
    switch (idx) {
        case 0: return COL_WHITE;
        case 1: return COL_BLACK;
        case 2: return COL_GREEN;
        case 3: return COL_BLUE;
        case 4: return COL_RED;
        case 5: return COL_YELLOW;
        case 6: return COL_ORANGE;
        default: return COL_WHITE;
    }
}

static bool readExact(Stream &s, uint8_t *dst, size_t len, uint32_t timeoutMs) {
    size_t got = 0;
    uint32_t t0 = millis();
    while (got < len) {
        int avail = s.available();
        if (avail > 0) {
            int n = s.readBytes((char *)(dst + got), len - got);
            if (n > 0) {
                got += (size_t)n;
                t0 = millis();
            }
        } else {
            if (millis() - t0 > timeoutMs) {
                return false;
            }
            delay(2);
        }
    }
    return true;
}

static bool validateHeader(const Edb7Header &h) {
    if (memcmp(h.magic, "EDB7", 4) != 0) return false;
    if (h.version != 1) return false;
    if (h.format != 1) return false;
    if (h.paletteCount < 7) return false;
    if (h.width != DISPLAY_WIDTH || h.height != DISPLAY_HEIGHT) return false;

    uint32_t expected = (uint32_t)(DISPLAY_WIDTH * DISPLAY_HEIGHT + 1) / 2;
    if (h.payloadLen != expected) return false;
    return true;
}

bool backendRenderFromBinary(Display_t &dsp, const char *url) {
    if (!url || strlen(url) == 0) {
        Serial.printf("[%s] No backend URL configured\n", TAG);
        return false;
    }

    Serial.printf("[%s] GET %s\n", TAG, url);

    WiFiClient client;
    HTTPClient http;
    http.setTimeout(15000);

    if (!http.begin(client, url)) {
        Serial.printf("[%s] HTTP begin failed\n", TAG);
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[%s] HTTP GET failed: %d\n", TAG, code);
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();

    Edb7Header hdr;
    if (!readExact(*stream, (uint8_t *)&hdr, sizeof(hdr), 5000)) {
        Serial.printf("[%s] Failed to read header\n", TAG);
        http.end();
        return false;
    }

    if (!validateHeader(hdr)) {
        Serial.printf("[%s] Invalid EDB7 header (v=%u, %ux%u, fmt=%u, payload=%u)\n",
                      TAG, hdr.version, hdr.width, hdr.height, hdr.format, hdr.payloadLen);
        http.end();
        return false;
    }

    uint8_t *payload = (uint8_t *)ps_malloc(hdr.payloadLen);
    if (!payload) {
        payload = (uint8_t *)malloc(hdr.payloadLen);
    }
    if (!payload) {
        Serial.printf("[%s] Out of memory for payload (%u bytes)\n", TAG, hdr.payloadLen);
        http.end();
        return false;
    }

    bool payloadOk = readExact(*stream, payload, hdr.payloadLen, 12000);
    http.end();
    if (!payloadOk) {
        Serial.printf("[%s] Failed to read payload\n", TAG);
        free(payload);
        return false;
    }

    Serial.printf("[%s] Rendering backend image...\n", TAG);
    dsp.setFullWindow();
    dsp.firstPage();
    do {
        for (int y = 0; y < DISPLAY_HEIGHT; y++) {
            uint32_t rowBaseNib = (uint32_t)y * DISPLAY_WIDTH;
            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                uint32_t pixIndex = rowBaseNib + (uint32_t)x;
                uint32_t byteIndex = pixIndex >> 1;
                uint8_t packed = payload[byteIndex];
                uint8_t idx = (pixIndex & 1U) == 0 ? (packed >> 4) : (packed & 0x0F);
                dsp.drawPixel(x, y, idxToColor(idx));
            }
        }
    } while (dsp.nextPage());

    free(payload);
    Serial.printf("[%s] Backend render complete\n", TAG);
    return true;
}
