#pragma once

#include "icons.h"

// Fetches EDB7-v1 binary image from URL and renders it to the display.
// Returns true on success.
bool backendRenderFromBinary(Display_t &dsp, const char *url);
