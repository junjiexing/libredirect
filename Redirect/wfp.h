#pragma once

#include <ntddk.h>

extern HANDLE filter_engine_handle;
extern HANDLE redirect_handle;


NTSTATUS wfp_init(PDEVICE_OBJECT dev_obj);
void wfp_uninit();