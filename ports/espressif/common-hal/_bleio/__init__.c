/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Dan Halbert for Adafruit Industries
 * Copyright (c) 2018 Artur Pacholec
 * Copyright (c) 2016 Glenn Ruben Bakke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>

#include "py/runtime.h"
#include "shared-bindings/_bleio/__init__.h"
#include "shared-bindings/_bleio/Adapter.h"
#include "shared-bindings/_bleio/Characteristic.h"
#include "shared-bindings/_bleio/Connection.h"
#include "shared-bindings/_bleio/Descriptor.h"
#include "shared-bindings/_bleio/Service.h"
#include "shared-bindings/_bleio/UUID.h"
#include "supervisor/shared/bluetooth/bluetooth.h"

#include "common-hal/_bleio/__init__.h"
// #include "common-hal/_bleio/bonding.h"
#include "common-hal/_bleio/ble_events.h"

void bleio_user_reset() {
    // Stop any user scanning or advertising.
    common_hal_bleio_adapter_stop_scan(&common_hal_bleio_adapter_obj);
    common_hal_bleio_adapter_stop_advertising(&common_hal_bleio_adapter_obj);

    ble_event_remove_heap_handlers();

    // Maybe start advertising the BLE workflow.
    supervisor_bluetooth_background();
}

// Turn off BLE on a reset or reload.
void bleio_reset() {
    // Set this explicitly to save data.
    common_hal_bleio_adapter_obj.base.type = &bleio_adapter_type;
    if (!common_hal_bleio_adapter_get_enabled(&common_hal_bleio_adapter_obj)) {
        return;
    }

    supervisor_stop_bluetooth();
    ble_event_reset();
    bleio_adapter_reset(&common_hal_bleio_adapter_obj);
    common_hal_bleio_adapter_set_enabled(&common_hal_bleio_adapter_obj, false);
    supervisor_start_bluetooth();
}

// The singleton _bleio.Adapter object, bound to _bleio.adapter
// It currently only has properties and no state. Inited by bleio_reset
bleio_adapter_obj_t common_hal_bleio_adapter_obj;

void bleio_background(void) {
}

void common_hal_bleio_gc_collect(void) {
    bleio_adapter_gc_collect(&common_hal_bleio_adapter_obj);
}

void check_nimble_error(int rc, const char *file, size_t line) {
    if (rc == NIMBLE_OK) {
        return;
    }
    switch (rc) {
        case BLE_HS_ENOMEM:
            mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("Nimble out of memory"));
            return;
        case BLE_HS_ETIMEOUT:
            mp_raise_msg(&mp_type_TimeoutError, NULL);
            return;
        case BLE_HS_EINVAL:
            mp_raise_ValueError(MP_ERROR_TEXT("Invalid BLE parameter"));
            return;
        case BLE_HS_ENOTCONN:
            mp_raise_ConnectionError(MP_ERROR_TEXT("Not connected"));
            return;
        default:
            #if CIRCUITPY_VERBOSE_BLE
            if (file) {
                mp_raise_bleio_BluetoothError(MP_ERROR_TEXT("Unknown system firmware error at %s:%d: %d"), file, line, rc);
            }
            #else
            (void)file;
            (void)line;
            mp_raise_bleio_BluetoothError(MP_ERROR_TEXT("Unknown system firmware error: %d"), rc);
            #endif

            break;
    }
}

void check_ble_error(int error_code, const char *file, size_t line) {
    // 0 means success. For BLE_HS_* codes, there is no defined "SUCCESS" value.
    if (error_code == 0) {
        return;
    }
    switch (error_code) {
        default:
            #if CIRCUITPY_VERBOSE_BLE
            if (file) {
                mp_raise_bleio_BluetoothError(MP_ERROR_TEXT("Unknown BLE error at %s:%d: %d"), file, line, error_code);
            }
            #else
            (void)file;
            (void)line;
            mp_raise_bleio_BluetoothError(MP_ERROR_TEXT("Unknown BLE error: %d"), error_code);
            #endif

            break;
    }
}

void check_notify(BaseType_t result) {
    if (result == pdTRUE) {
        return;
    }
    mp_raise_msg(&mp_type_TimeoutError, NULL);
}

void common_hal_bleio_check_connected(uint16_t conn_handle) {
    if (conn_handle == BLEIO_HANDLE_INVALID) {
        mp_raise_ConnectionError(MP_ERROR_TEXT("Not connected"));
    }
}
