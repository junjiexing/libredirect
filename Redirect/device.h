#pragma once

#include <ntddk.h>
#include <wdf.h>

NTSTATUS device_init(WDFDRIVER driver, WDFDEVICE& device);
void device_uninit();
