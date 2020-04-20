
#define NDIS61 1				// Need to declare this to compile WFP stuff on Win7, I'm not sure why

#include "Ntifs.h"
#include <ntddk.h>				// Windows Driver Development Kit
#include <wdf.h>				// Windows Driver Foundation

#include "device.h"
#include "wfp.h"


VOID libredirect_unload(IN WDFDRIVER driver)
{
	UNREFERENCED_PARAMETER(driver);
	KdPrint(("|LIBREDIRECT|file_cleanup|libredirect_unload"));
	//PDRIVER_OBJECT drv_obj = WdfDriverWdmGetDriverObject(driver);
	wfp_uninit();
	device_uninit();
}


extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT driver_obj, IN PUNICODE_STRING reg_path)
{
	KdPrint(("|LIBREDIRECT|file_cleanup|DriverEntry"));
	// Configure ourself as a non-PnP driver:
	WDF_DRIVER_CONFIG config;
	WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
	config.DriverInitFlags |= WdfDriverInitNonPnpDriver;
	config.EvtDriverUnload = libredirect_unload;
	WDFDRIVER driver;
	auto status = WdfDriverCreate(driver_obj, reg_path, WDF_NO_OBJECT_ATTRIBUTES, &config, &driver);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("|LIBREDIRECT|file_cleanup|failed to create WDF driver: %d", status));
		return status;
	}

	WDFDEVICE device;
	status = device_init(driver, device);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	auto dev_obj = WdfDeviceWdmGetDeviceObject(device);
	status = wfp_init(dev_obj);

	return status;
}