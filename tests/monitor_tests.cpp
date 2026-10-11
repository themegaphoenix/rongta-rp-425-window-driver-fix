#include "monitor_abi.h"
#include <strsafe.h>
#include <cwchar>
#include <cassert>
#include <cstdio>
#include <vector>

static std::vector<BYTE> received;
static bool short_writes = false, zero_write = false, fail_write = false;
static bool job_marked = false;
static BOOL WINAPI test_write(HANDLE, LPCVOID data, DWORD count, LPDWORD written, LPOVERLAPPED) {
    if (fail_write) { *written = 0; SetLastError(ERROR_DEVICE_NOT_CONNECTED); return FALSE; }
    *written = zero_write ? 0 : (short_writes ? min(count, 17UL) : count);
    auto bytes = static_cast<const BYTE*>(data);
    received.insert(received.end(), bytes, bytes + *written);
    return TRUE;
}
static void WINAPI test_sleep(DWORD) {}
static BOOL WINAPI test_job(HANDLE, DWORD, DWORD, LPBYTE, DWORD) { job_marked = true; return TRUE; }
static BOOL WINAPI test_close_printer(HANDLE) { return TRUE; }
#define WriteFile test_write
#define Sleep test_sleep
#define SetJobW test_job
#define ClosePrinter test_close_printer
#include "../src/rp425_monitor.cpp"
#undef WriteFile
#undef Sleep
#undef SetJobW
#undef ClosePrinter

int main() {
    HANDLE monitor = nullptr;
    MONITORINIT init = {}; init.cbSize = sizeof(init); init.bLocal = TRUE;
    auto api = InitializePrintMonitor2(&init, &monitor);
    assert(api && monitor && api->cbSize == 184);
    DWORD needed = 0, count = 999;
    assert(!api->pfnEnumPorts(monitor, nullptr, 2, nullptr, 0, &needed, &count));
    assert(GetLastError() == ERROR_INSUFFICIENT_BUFFER && needed && !count);
    std::vector<BYTE> buffer(needed);
    assert(api->pfnEnumPorts(monitor, nullptr, 2, buffer.data(), needed, &needed, &count));
    auto info = reinterpret_cast<PORT_INFO_2W*>(buffer.data());
    assert(count == 1 && !wcscmp(info->pPortName, PORT_NAME));
    assert(!wcscmp(info->pMonitorName, MONITOR_NAME));
    HANDLE handle = nullptr;
    assert(!api->pfnOpenPort(monitor, const_cast<LPWSTR>(L"unexpected port"), &handle));
    assert(api->pfnOpenPort(monitor, const_cast<LPWSTR>(PORT_NAME), &handle));
    auto port = static_cast<Port*>(handle);
    assert(serial_name(L"COM21") && !serial_name(L"COM21\\other") && !serial_name(L"COM"));
    assert(bluetooth_address(L"001122AABBCC"));
    assert(!bluetooth_address(L"001122AABBC") && !bluetooth_address(L"001122AABBCZ"));
    assert(!bluetooth_address(L"00:11:22:AA:BB:CC"));
    assert(device_name(L"\\Device\\BthModem9") && !device_name(L"\\Device\\Harddisk0"));
    std::vector<BYTE> label(777);
    for (size_t i = 0; i < label.size(); ++i) label[i] = static_cast<BYTE>(i % 251);
    DWORD written = 999;
    assert(!api->pfnWritePort(handle, label.data(), static_cast<DWORD>(label.size()), &written) && written == 0);
    port->active = TRUE;
    short_writes = true;
    assert(api->pfnWritePort(handle, label.data(), static_cast<DWORD>(label.size()), &written));
    assert(written == label.size() && received == label && port->total == label.size());
    short_writes = false; zero_write = true;
    assert(!api->pfnWritePort(handle, label.data(), 1, &written));
    assert(GetLastError() == ERROR_TIMEOUT && written == 0);
    zero_write = false;
    assert(!api->pfnWritePort(handle, label.data(), 1, &written));
    assert(!api->pfnEndDocPort(handle) && !job_marked && !port->active);
    port->active = TRUE; port->error = 0; port->total = 0; fail_write = true;
    assert(!api->pfnWritePort(handle, label.data(), 1, &written));
    assert(GetLastError() == ERROR_DEVICE_NOT_CONNECTED && written == 0);
    assert(!api->pfnEndDocPort(handle) && !job_marked);
    fail_write = false; port->active = TRUE; port->error = 0; port->total = MAX_JOB;
    assert(!api->pfnWritePort(handle, label.data(), 1, &written));
    assert(GetLastError() == ERROR_FILE_TOO_LARGE);
    assert(!api->pfnEndDocPort(handle));
    port->active = TRUE; port->error = 0; port->total = 0;
    assert(api->pfnWritePort(handle, label.data(), 1, &written));
    assert(api->pfnEndDocPort(handle) && job_marked && !port->active);
    assert(api->pfnClosePort(handle));
    api->pfnShutdown(monitor);
    std::puts("Native monitor tests passed: ABI, enumeration, validation, partial writes, failures and job completion.");
    return 0;
}
