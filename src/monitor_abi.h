#pragma once
#include <cstddef>
#include <windows.h>
#include <winspool.h>
#include <winsplp.h>

using RpMonitor2 = MONITOR2;
static_assert(sizeof(void*) == 8, "Build the monitor for Windows x64");
static_assert(offsetof(MONITOR2, pfnWritePort) == 40, "MONITOR2 ABI mismatch");
static_assert(sizeof(RpMonitor2) == 184, "MONITOR2 ABI mismatch");
