#include "monitor_abi.h"
#include <strsafe.h>
#include <cwchar>

static constexpr WCHAR PORT_NAME[] = L"RP425-Bluetooth";
static constexpr WCHAR MONITOR_NAME[] = L"RP425 Bluetooth Native";
static constexpr DWORD MAX_JOB = 32 * 1024 * 1024;
static constexpr DWORD MONITOR_MAGIC = 0x52504D32;
static constexpr DWORD PORT_MAGIC = 0x52505032;

struct Monitor {
    DWORD magic;
    HANDLE gate;
};
struct Port {
    DWORD magic;
    Monitor* monitor;
    HANDLE serial;
    HANDLE printer;
    DWORD job;
    DWORD total;
    DWORD error;
    BOOL active;
    BOOL held;
};

static BOOL fail(DWORD error) { SetLastError(error); return FALSE; }
static BOOL valid(Monitor* monitor) { return monitor && monitor->magic == MONITOR_MAGIC; }
static BOOL valid(Port* port) { return port && port->magic == PORT_MAGIC; }

static void disconnect(Port* port) {
    if (port->serial != INVALID_HANDLE_VALUE) {
        CloseHandle(port->serial);
        port->serial = INVALID_HANDLE_VALUE;
    }
    if (port->printer) { ClosePrinter(port->printer); port->printer = nullptr; }
    if (port->held) { ReleaseSemaphore(port->monitor->gate, 1, nullptr); port->held = FALSE; }
    port->active = FALSE;
}

static BOOL serial_name(LPCWSTR name) {
    if (!name || wcsncmp(name, L"COM", 3) || !name[3]) return FALSE;
    for (const WCHAR* p = name + 3; *p; ++p) if (*p < L'0' || *p > L'9') return FALSE;
    return TRUE;
}

static BOOL device_name(LPCWSTR name) {
    static constexpr WCHAR PREFIX[] = L"\\Device\\BthModem";
    const size_t length = _countof(PREFIX) - 1;
    if (wcsncmp(name, PREFIX, length) || !name[length]) return FALSE;
    for (const WCHAR* p = name + length; *p; ++p) if (*p < L'0' || *p > L'9') return FALSE;
    return TRUE;
}

static BOOL bluetooth_address(LPCWSTR address) {
    if (!address || wcslen(address) != 12) return FALSE;
    for (const WCHAR* p = address; *p; ++p)
        if (!((*p >= L'0' && *p <= L'9') || (*p >= L'A' && *p <= L'F'))) return FALSE;
    return TRUE;
}

static DWORD find_device(WCHAR* output, size_t capacity) {
    WCHAR address[13] = {}; DWORD address_bytes = sizeof(address);
    LSTATUS configuration = RegGetValueW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\RP425 Bluetooth Native",
        L"BluetoothAddress", RRF_RT_REG_SZ, nullptr, address, &address_bytes);
    if (configuration != ERROR_SUCCESS) return configuration;
    if (!bluetooth_address(address)) return ERROR_INVALID_DATA;
    HKEY bluetooth = nullptr;
    LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Enum\\BTHENUM", 0, KEY_READ, &bluetooth);
    if (status != ERROR_SUCCESS) return static_cast<DWORD>(status);
    WCHAR port[32] = {};
    for (DWORD i = 0; !port[0]; ++i) {
        WCHAR service[512]; DWORD length = _countof(service);
        status = RegEnumKeyExW(bluetooth, i, service, &length, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS) { RegCloseKey(bluetooth); return status; }
        if (_wcsnicmp(service, L"{00001101-0000-1000-8000-00805F9B34FB}", 38)) continue;
        HKEY devices = nullptr;
        if (RegOpenKeyExW(bluetooth, service, 0, KEY_READ, &devices) != ERROR_SUCCESS) continue;
        for (DWORD j = 0; !port[0]; ++j) {
            WCHAR instance[512]; length = _countof(instance);
            status = RegEnumKeyExW(devices, j, instance, &length, nullptr, nullptr, nullptr, nullptr);
            if (status == ERROR_NO_MORE_ITEMS) break;
            if (status != ERROR_SUCCESS) break;
            if (!wcsstr(instance, address)) continue;
            WCHAR subkey[600];
            if (FAILED(StringCchPrintfW(subkey, _countof(subkey), L"%s\\Device Parameters", instance))) continue;
            DWORD bytes = sizeof(port);
            status = RegGetValueW(devices, subkey, L"PortName", RRF_RT_REG_SZ, nullptr, port, &bytes);
            if (status != ERROR_SUCCESS || !serial_name(port)) port[0] = 0;
        }
        RegCloseKey(devices);
    }
    RegCloseKey(bluetooth);
    if (!port[0]) return ERROR_DEVICE_NOT_CONNECTED;
    HKEY serial = nullptr;
    status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &serial);
    if (status != ERROR_SUCCESS) return status;
    DWORD result = ERROR_DEVICE_NOT_CONNECTED;
    for (DWORD i = 0;; ++i) {
        WCHAR name[512], value[32];
        DWORD name_size = _countof(name), value_size = sizeof(value), type = 0;
        status = RegEnumValueW(serial, i, name, &name_size, nullptr, &type,
                              reinterpret_cast<BYTE*>(value), &value_size);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status == ERROR_MORE_DATA) continue;
        if (status != ERROR_SUCCESS) { result = status; break; }
        if (type != REG_SZ || value_size < sizeof(WCHAR) || value_size > sizeof(value)) continue;
        value[_countof(value) - 1] = 0;
        if (_wcsicmp(value, port) || !device_name(name)) continue;
        result = SUCCEEDED(StringCchPrintfW(output, capacity, L"\\\\?\\GLOBALROOT%s", name))
            ? ERROR_SUCCESS : ERROR_INSUFFICIENT_BUFFER;
        break;
    }
    RegCloseKey(serial);
    return result;
}

static BOOL WINAPI enum_ports(HANDLE handle, LPWSTR, DWORD level, LPBYTE buffer,
                             DWORD capacity, LPDWORD needed, LPDWORD returned) {
    if (!valid(static_cast<Monitor*>(handle)) || !needed || !returned) return fail(ERROR_INVALID_PARAMETER);
    *returned = 0;
    if (level != 1 && level != 2) { *needed = 0; return fail(ERROR_INVALID_LEVEL); }
    DWORD name_bytes = sizeof(PORT_NAME), monitor_bytes = sizeof(MONITOR_NAME);
    DWORD structure = level == 1 ? sizeof(PORT_INFO_1W) : sizeof(PORT_INFO_2W);
    *needed = structure + name_bytes + (level == 2 ? monitor_bytes + sizeof(L"RP425 Bluetooth serial") : 0);
    if (!buffer || capacity < *needed) return fail(ERROR_INSUFFICIENT_BUFFER);
    ZeroMemory(buffer, *needed);
    auto name = reinterpret_cast<LPWSTR>(buffer + structure);
    CopyMemory(name, PORT_NAME, name_bytes);
    if (level == 1) reinterpret_cast<PORT_INFO_1W*>(buffer)->pName = name;
    else {
        auto info = reinterpret_cast<PORT_INFO_2W*>(buffer);
        info->pPortName = name;
        info->pMonitorName = reinterpret_cast<LPWSTR>(buffer + structure + name_bytes);
        CopyMemory(info->pMonitorName, MONITOR_NAME, monitor_bytes);
        info->pDescription = reinterpret_cast<LPWSTR>(buffer + structure + name_bytes + monitor_bytes);
        CopyMemory(info->pDescription, L"RP425 Bluetooth serial", sizeof(L"RP425 Bluetooth serial"));
        info->fPortType = PORT_TYPE_WRITE;
    }
    *returned = 1;
    return TRUE;
}

static BOOL WINAPI open_port(HANDLE handle, LPWSTR name, PHANDLE output) {
    auto monitor = static_cast<Monitor*>(handle);
    if (!valid(monitor) || !output || !name || wcscmp(name, PORT_NAME)) return fail(ERROR_INVALID_PARAMETER);
    *output = nullptr;
    auto port = static_cast<Port*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Port)));
    if (!port) return fail(ERROR_NOT_ENOUGH_MEMORY);
    port->magic = PORT_MAGIC; port->monitor = monitor; port->serial = INVALID_HANDLE_VALUE;
    *output = port;
    return TRUE;
}

static BOOL WINAPI start_doc(HANDLE handle, LPWSTR printer_name, DWORD job, DWORD level, LPBYTE info) {
    auto port = static_cast<Port*>(handle);
    if (!valid(port) || !printer_name || !info || level != 1) return fail(ERROR_INVALID_PARAMETER);
    if (port->active) return fail(ERROR_BUSY);
    DWORD wait = WaitForSingleObject(port->monitor->gate, 0);
    if (wait != WAIT_OBJECT_0) return fail(wait == WAIT_TIMEOUT ? ERROR_BUSY : GetLastError());
    port->held = TRUE;
    WCHAR path[600];
    DWORD error = find_device(path, _countof(path));
    if (error != ERROR_SUCCESS) { disconnect(port); return fail(error); }
    port->serial = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    if (port->serial == INVALID_HANDLE_VALUE) { error = GetLastError(); disconnect(port); return fail(error); }
    COMMTIMEOUTS timeouts = {}; timeouts.WriteTotalTimeoutConstant = 5000;
    if (!SetCommTimeouts(port->serial, &timeouts)) { error = GetLastError(); disconnect(port); return fail(error); }
    if (!OpenPrinterW(printer_name, &port->printer, nullptr)) { error = GetLastError(); disconnect(port); return fail(error); }
    port->job = job; port->total = 0; port->error = ERROR_SUCCESS; port->active = TRUE;
    return TRUE;
}

static BOOL WINAPI write_port(HANDLE handle, LPBYTE data, DWORD length, LPDWORD written) {
    auto port = static_cast<Port*>(handle);
    if (!written) return fail(ERROR_INVALID_PARAMETER);
    *written = 0;
    if (!valid(port) || !port->active || (!data && length)) return fail(ERROR_INVALID_PARAMETER);
    if (port->error) return fail(port->error);
    if (length > MAX_JOB - port->total) { port->error = ERROR_FILE_TOO_LARGE; return fail(port->error); }
    while (*written < length) {
        DWORD amount = min(256UL, length - *written), sent = 0;
        BOOL result = WriteFile(port->serial, data + *written, amount, &sent, nullptr);
        if (sent > amount) { port->error = ERROR_INVALID_DATA; return fail(port->error); }
        *written += sent; port->total += sent;
        if (!result || !sent) {
            port->error = result ? ERROR_TIMEOUT : GetLastError();
            if (!port->error) port->error = ERROR_WRITE_FAULT;
            return fail(port->error);
        }
        Sleep(30);
    }
    return TRUE;
}

static BOOL WINAPI read_port(HANDLE, LPBYTE, DWORD, LPDWORD read) {
    if (read) *read = 0;
    return fail(ERROR_NOT_SUPPORTED);
}

static BOOL WINAPI end_doc(HANDLE handle) {
    auto port = static_cast<Port*>(handle);
    if (!valid(port)) return fail(ERROR_INVALID_HANDLE);
    if (!port->active) return TRUE;
    DWORD error = port->error;
    if (!error) {
        // Matches the drain interval verified with two successive physical labels.
        Sleep(2000);
        if (!SetJobW(port->printer, port->job, 0, nullptr, JOB_CONTROL_SENT_TO_PRINTER)) error = GetLastError();
    }
    disconnect(port);
    return error ? fail(error) : TRUE;
}

static BOOL WINAPI close_port(HANDLE handle) {
    auto port = static_cast<Port*>(handle);
    if (!valid(port)) return fail(ERROR_INVALID_HANDLE);
    disconnect(port); port->magic = 0;
    HeapFree(GetProcessHeap(), 0, port);
    return TRUE;
}

struct Xcv { DWORD access; };
static BOOL WINAPI xcv_open(HANDLE monitor, LPCWSTR, ACCESS_MASK access, PHANDLE output) {
    if (!valid(static_cast<Monitor*>(monitor)) || !output) return fail(ERROR_INVALID_PARAMETER);
    auto xcv = static_cast<Xcv*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Xcv)));
    if (!xcv) return fail(ERROR_NOT_ENOUGH_MEMORY);
    xcv->access = access; *output = xcv; return TRUE;
}
static DWORD WINAPI xcv_data(HANDLE handle, LPCWSTR, PBYTE, DWORD, PBYTE, DWORD, PDWORD needed) {
    if (!handle || !needed) return ERROR_INVALID_PARAMETER;
    *needed = 0;
    return ERROR_NOT_SUPPORTED;
}
static BOOL WINAPI xcv_close(HANDLE handle) {
    if (!handle) return fail(ERROR_INVALID_HANDLE);
    return HeapFree(GetProcessHeap(), 0, handle);
}
static void WINAPI shutdown_monitor(HANDLE handle) {
    auto monitor = static_cast<Monitor*>(handle);
    if (!valid(monitor)) return;
    CloseHandle(monitor->gate); monitor->magic = 0;
    HeapFree(GetProcessHeap(), 0, monitor);
}
static DWORD WINAPI bidi(HANDLE, DWORD, LPCWSTR, PBIDI_REQUEST_CONTAINER, PBIDI_RESPONSE_CONTAINER* response) {
    if (response) *response = nullptr;
    return ERROR_NOT_SUPPORTED;
}

static RpMonitor2 callbacks = {
    sizeof(RpMonitor2), enum_ports, open_port, nullptr, start_doc, write_port, read_port,
    end_doc, close_port, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    xcv_open, xcv_data, xcv_close, shutdown_monitor, bidi, nullptr, nullptr, nullptr
};

extern "C" LPMONITOR2 WINAPI InitializePrintMonitor2(PMONITORINIT init, PHANDLE output) {
    if (!init || init->cbSize < sizeof(MONITORINIT) || !output) { fail(ERROR_INVALID_PARAMETER); return nullptr; }
    *output = nullptr;
    auto monitor = static_cast<Monitor*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(Monitor)));
    if (!monitor) { fail(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
    monitor->gate = CreateSemaphoreW(nullptr, 1, 1, nullptr);
    if (!monitor->gate) {
        DWORD error = GetLastError(); HeapFree(GetProcessHeap(), 0, monitor); fail(error); return nullptr;
    }
    monitor->magic = MONITOR_MAGIC; *output = monitor;
    return &callbacks;
}
