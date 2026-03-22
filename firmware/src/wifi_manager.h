#pragma once
// ============================================================
// WiFi connection manager
// ============================================================

#include <Arduino.h>

/// Initialise WiFi in station mode and connect to the configured AP.
/// Returns true on success.
bool wifiConnect();

/// Disconnect and power-down the WiFi radio to save power.
void wifiDisconnect();

/// Returns true if currently connected.
bool wifiIsConnected();
