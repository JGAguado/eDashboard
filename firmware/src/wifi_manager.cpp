// ============================================================
// WiFi connection manager
// ============================================================

#include "wifi_manager.h"
#include "config.h"
#include "secrets_gen.h"
#include <WiFi.h>

static const char *TAG = "WiFi";

bool wifiConnect() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[%s] Already connected (IP %s)\n", TAG, WiFi.localIP().toString().c_str());
        return true;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    for (int attempt = 1; attempt <= WIFI_RETRY_COUNT; attempt++) {
        Serial.printf("[%s] Connecting to \"%s\" (attempt %d/%d)...\n",
                      TAG, SECRET_WIFI_SSID, attempt, WIFI_RETRY_COUNT);

        WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED &&
               (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[%s] Connected! IP: %s  RSSI: %d dBm\n",
                          TAG, WiFi.localIP().toString().c_str(), WiFi.RSSI());
            return true;
        }

        Serial.printf("[%s] Connection failed (status=%d)\n", TAG, WiFi.status());
        WiFi.disconnect(true);
        delay(1000);
    }

    Serial.printf("[%s] Could not connect after %d attempts\n", TAG, WIFI_RETRY_COUNT);
    return false;
}

void wifiDisconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.printf("[%s] Disconnected & radio off\n", TAG);
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}
