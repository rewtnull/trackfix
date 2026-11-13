/*************************************************************************************************************
 *                                                                                                           *
 *  Trackfix v1.0                                                                                            *
 *                                                                                                           *
 *  A small tool to force reinitialization of Synaptics devices. This will hopefully fix frozen TrackPoint   *
 *  on devices such as ThinkPad T14, P14s, X13 series etc. without requiring a reboot.                       *
 *                                                                                                           *
 *  Just run the tool and hopefully your frozen device will be found and reinitialized.                      *
 *                                                                                                           *
 *  MinGW-w64 GCC:                                                                                           *
 *                                                                                                           *
 *  gcc -std=c11 -O2 -Wall -Wextra -Werror trackfix.c -o trackfix.exe -lole32 -loleaut32 -luuid -luser32     *
 *                                                                                                           *
 *  VS x64 Native Tools Command Prompt:                                                                      *
 *                                                                                                           *
 *  cl /nologo /TC /std:c11 /O2 /W4 /WX /Fe:trackfix.exe trackfix.c ole32.lib oleaut32.lib uuid.lib          *
 *                                                                                                           *
 *  Compile for the architecture of the registered Synaptics COM server.                                     *
 *                                                                                                           *
 *  This tool is *FREEWARE*. Do with it as you wish. Or don't. I'm not your mom.                             *
 *                                                                                                           *
 *  (c)2026 Marcus Hoffren                                                                                   *
 *                                                                                                           *
 *  https://github.com/rewtnull                                                                              *
 *                                                                                                           *
 *************************************************************************************************************/

#include <windows.h>
#include <oleauto.h>

#define TRACKPOINT_TYPE         4L
#define PROP_DEVICE_TYPE        0x10000102L
#define PROP_CONNECTION_TYPE    0x10000103L
#define PROP_MODEL              0x10000601L
#define PROP_PNP_ID             0x10000602L
#define PROP_SHORT_NAME         0x10000603L

typedef struct {
    IDispatch *object;
    DISPID initialize;
    DISPID find_device;
    DISPID hardware_broadcast;
} SynapticsApi;

typedef struct {
    IDispatch *object;
    DISPID select;
    DISPID activate;
    DISPID deactivate;
    DISPID get_long;
    DISPID get_string;
} SynapticsDevice;

typedef struct {
    LONG type;
    LONG connection;
    BSTR model;
    BSTR pnp_id;
    BSTR short_name;
} DeviceInfo;

/* Write UTF-16 text to an attached Windows console */
static void console_write(HANDLE output, const WCHAR *text)
{
    DWORD written;
    if (output != INVALID_HANDLE_VALUE && output != NULL && text != NULL) {
        WriteConsoleW(output, text, (DWORD)lstrlenW(text), &written, NULL);
    }
}

/* Report a COM failure with its HRESULT in hexadecimal */
static void console_error(HANDLE output, const WCHAR *message, HRESULT result)
{
    WCHAR code[40];
    console_write(output, message);
    wsprintfW(code, L" (HRESULT 0x%08lX)\r\n", (unsigned long)result);
    console_write(output, code);
}

/* Consume standard input until Enter is pressed; fail on end-of-input or I/O error */
static BOOL wait_for_enter(void)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    char character;
    DWORD received;

    if (input == INVALID_HANDLE_VALUE || input == NULL) return FALSE;

    while (ReadFile(input, &character, 1, &received, NULL) && received == 1) {
        if (character == '\r' || character == '\n') return TRUE;
    }
    return FALSE;
}

/* Instantiate a registered COM automation server by ProgID.
 * On success, the caller owns *object and must release it */
static HRESULT com_create(const WCHAR *name, IDispatch **object)
{
    CLSID clsid;
    HRESULT result;

    *object = NULL;
    result = CLSIDFromProgID(name, &clsid);
    if (FAILED(result)) return result;

    return CoCreateInstance(&clsid, NULL,
                            CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                            &IID_IDispatch, (void **)object);
}

/* Resolve a dispatch method name to its COM member identifier (DISPID) */
static HRESULT com_resolve(IDispatch *object, const WCHAR *name, DISPID *id)
{
    LPOLESTR names[1] = { (LPOLESTR)name };
    return object->lpVtbl->GetIDsOfNames(object, &IID_NULL, names, 1,
                                        LOCALE_USER_DEFAULT, id);
}

/* Invoke a dispatch method with up to three 32-bit integer arguments.
 * COM expects arguments in reverse order. An optional result is initialized
 * for the caller, who must call VariantClear on success */
static HRESULT com_invoke(IDispatch *object, DISPID id, const LONG *values,
                          UINT count, VARIANT *result)
{
    VARIANTARG arguments[3];
    DISPPARAMS params = { count != 0 ? arguments : NULL, NULL, count, 0 };
    HRESULT status;
    UINT i;

    if (count > 3 || (count != 0 && values == NULL)) return E_INVALIDARG;

    for (i = 0; i < count; ++i) {
        VariantInit(&arguments[i]);
        arguments[i].vt = VT_I4;
        arguments[i].lVal = values[count - i - 1];
    }
    if (result != NULL) VariantInit(result);

    status = object->lpVtbl->Invoke(object, id, &IID_NULL, LOCALE_USER_DEFAULT,
                                    DISPATCH_METHOD, &params, result, NULL, NULL);
    if (FAILED(status) && result != NULL) VariantClear(result);

    return status;
}

/* Invoke a COM method and convert its returned value to a 32-bit integer */
static HRESULT com_long(IDispatch *object, DISPID id, const LONG *arguments,
                        UINT count, LONG *value)
{
    VARIANT result;
    VARIANT converted;
    HRESULT status = com_invoke(object, id, arguments, count, &result);

    if (FAILED(status)) return status;

    VariantInit(&converted);
    status = VariantChangeType(&converted, &result, 0, VT_I4);
    if (SUCCEEDED(status)) *value = converted.lVal;

    VariantClear(&converted);
    VariantClear(&result);
    return status;
}

/* Read a string-valued device property through COM automation.
 * On success, the caller owns the returned BSTR and must free it */
static HRESULT com_string(IDispatch *object, DISPID id, LONG property, BSTR *value)
{
    VARIANT result;
    VARIANT converted;
    HRESULT status;

    *value = NULL;
    status = com_invoke(object, id, &property, 1, &result);
    if (FAILED(status)) return status;

    VariantInit(&converted);
    status = VariantChangeType(&converted, &result, 0, VT_BSTR);
    if (SUCCEEDED(status)) {
        *value = converted.bstrVal;
        converted.vt = VT_EMPTY;
    }
    VariantClear(&converted);
    VariantClear(&result);
    return status;
}

/* Create the Synaptics controller and cache the methods needed for discovery
 * and hardware reset. The caller owns api->object, even on partial failure */
static HRESULT synaptics_open_api(SynapticsApi *api)
{
    HRESULT result = com_create(L"SynCtrl.SynAPICtrl", &api->object);

    if (SUCCEEDED(result)) result = com_resolve(api->object, L"Initialize", &api->initialize);
    if (SUCCEEDED(result)) result = com_resolve(api->object, L"FindDevice", &api->find_device);
    if (SUCCEEDED(result)) result = com_resolve(api->object, L"HardwareBroadcast", &api->hardware_broadcast);
    return result;
}

/* Create a Synaptics device controller and cache its property-access methods.
 * The caller owns device->object, even on partial failure */
static HRESULT synaptics_open_device(SynapticsDevice *device)
{
    HRESULT result = com_create(L"SynCtrl.SynDeviceCtrl", &device->object);

    if (SUCCEEDED(result)) result = com_resolve(device->object, L"Select", &device->select);
    if (SUCCEEDED(result)) result = com_resolve(device->object, L"Activate", &device->activate);
    if (SUCCEEDED(result)) result = com_resolve(device->object, L"Deactivate", &device->deactivate);
    if (SUCCEEDED(result)) result = com_resolve(device->object, L"GetLongProperty", &device->get_long);
    if (SUCCEEDED(result)) result = com_resolve(device->object, L"GetStringProperty", &device->get_string);
    return result;
}

/* Release all COM-allocated strings in a device information record */
static void device_info_free(DeviceInfo *info)
{
    SysFreeString(info->model);
    SysFreeString(info->pnp_id);
    SysFreeString(info->short_name);
    ZeroMemory(info, sizeof(*info));
}

/* Select and activate a discovered device, collect its identifying properties,
 * then deactivate it. The caller must free the returned strings, including
 * when a property read fails partway through */
static HRESULT synaptics_device_info(const SynapticsDevice *device,
                                     LONG handle, DeviceInfo *info)
{
    HRESULT result;
    HRESULT deactivate_result;
    LONG property;

    ZeroMemory(info, sizeof(*info));
    result = com_invoke(device->object, device->select, &handle, 1, NULL);
    if (FAILED(result)) return result;

    result = com_invoke(device->object, device->activate, NULL, 0, NULL);
    if (FAILED(result)) return result;

    property = PROP_DEVICE_TYPE;
    result = com_long(device->object, device->get_long, &property, 1, &info->type);
    if (SUCCEEDED(result)) {
        property = PROP_CONNECTION_TYPE;
        result = com_long(device->object, device->get_long, &property, 1, &info->connection);
    }
    if (SUCCEEDED(result)) result = com_string(device->object, device->get_string, PROP_MODEL, &info->model);
    if (SUCCEEDED(result)) result = com_string(device->object, device->get_string, PROP_PNP_ID, &info->pnp_id);
    if (SUCCEEDED(result)) result = com_string(device->object, device->get_string, PROP_SHORT_NAME, &info->short_name);

    deactivate_result = com_invoke(device->object, device->deactivate, NULL, 0, NULL);
    return FAILED(result) ? result : deactivate_result;
}

/* Print one device and restore the console's original text attributes */
static void console_device(HANDLE output, const DeviceInfo *info,
                           WORD attributes, BOOL colored)
{
    WCHAR prefix[64];
    WORD foreground = info->type == TRACKPOINT_TYPE
                    ? FOREGROUND_GREEN | FOREGROUND_INTENSITY
                    : FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;

    if (colored) {
        SetConsoleTextAttribute(output, (WORD)((attributes & 0xFFF0u) | foreground));
    }
    wsprintfW(prefix, L"[%ld:%ld, ", info->connection, info->type);
    console_write(output, prefix);
    console_write(output, info->pnp_id != NULL ? info->pnp_id : L"");
    console_write(output, L", ");
    console_write(output, info->short_name != NULL ? info->short_name : L"");
    console_write(output, L"]: ");
    console_write(output, info->model != NULL ? info->model : L"");
    console_write(output, L"\r\n");
    if (colored) SetConsoleTextAttribute(output, attributes);
}

/* Enumerate Synaptics devices, print each entry, and count TrackPoints.
 * FindDevice returns -1 when enumeration is complete */
static HRESULT synaptics_list_devices(const SynapticsApi *api,
                                      const SynapticsDevice *device,
                                      HANDLE output, WORD attributes, BOOL colored,
                                      LONG *device_count, LONG *trackpoint_count)
{
    LONG handle = -1;
    HRESULT result;

    *device_count = 0;
    *trackpoint_count = 0;
    for (;;) {
        LONG query[3] = { 0, 0, handle };
        DeviceInfo info = { 0 };

        result = com_long(api->object, api->find_device, query, 3, &handle);
        if (FAILED(result) || handle == -1) return result;

        result = synaptics_device_info(device, handle, &info);
        if (SUCCEEDED(result)) {
            ++*device_count;
            if (info.type == TRACKPOINT_TYPE) ++*trackpoint_count;

            console_device(output, &info, attributes, colored);
        }
        device_info_free(&info);
        if (FAILED(result)) return result;
    }
}

/* Initialize COM, list the available devices, and send HardwareBroadcast(0)
 * when a TrackPoint was found. Always release COM objects and restore the
 * original console attributes before exiting */
int main(void)
{
    SynapticsApi api = { 0 };
    SynapticsDevice device = { 0 };
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO screen;
    BOOL colored = output != NULL && output != INVALID_HANDLE_VALUE &&
                   GetConsoleScreenBufferInfo(output, &screen);
    WORD attributes = colored ? screen.wAttributes : 0;
    LONG device_count = 0;
    LONG trackpoint_count = 0;
    WCHAR summary[128];
    LONG reset = 0;
    HRESULT result;
    int exit_code = 1;

    result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(result)) {
        console_error(output, L"COM initialization failed", result);
        return exit_code;
    }

    result = synaptics_open_api(&api);
    if (SUCCEEDED(result)) result = synaptics_open_device(&device);
    if (SUCCEEDED(result)) result = com_invoke(api.object, api.initialize, NULL, 0, NULL);
    if (FAILED(result)) {
        console_error(output, L"Synaptics COM initialization failed", result);
        goto cleanup;
    }
    console_write(output, L"Trackfix v1.0 (c)2026 Marcus Hoffren\r\n");
    console_write(output, L"Searching for Synaptics devices...\r\n");
    result = synaptics_list_devices(&api, &device, output, attributes, colored,
                                   &device_count, &trackpoint_count);
    if (FAILED(result)) {
        console_error(output, L"Device enumeration failed", result);
        goto cleanup;
    }

    wsprintfW(summary, L"\r\nDevices found: %ld; TrackPoints found: %ld\r\n",
              device_count, trackpoint_count);
    console_write(output, summary);
    if (trackpoint_count == 0) {
        console_write(output, L"No TrackPoint detected\r\n");
        goto cleanup;
    }

    console_write(output, L"Press Enter to reset TrackPoint hardware. CTRL+C to cancel.\r\n");
    if (!wait_for_enter()) {
        console_write(output, L"Cancelled\r\n");
        goto cleanup;
    } else {
        console_write(output, L"Sending reset request\r\n");
    }
    result = com_invoke(api.object, api.hardware_broadcast, &reset, 1, NULL);
    if (FAILED(result)) {
        console_error(output, L"TrackPoint reset request failed", result);
        goto cleanup;
    }
    console_write(output, L"Done!\r\n");
    exit_code = 0;

cleanup:
    if (colored) SetConsoleTextAttribute(output, attributes);
    if (device.object != NULL) device.object->lpVtbl->Release(device.object);
    if (api.object != NULL) api.object->lpVtbl->Release(api.object);
    CoUninitialize();
    return exit_code;
}
