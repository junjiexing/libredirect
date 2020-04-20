#include "device.h"
#include "io_callbacks.h"



NTSTATUS device_init(WDFDRIVER driver, WDFDEVICE& device)
{
	DECLARE_CONST_UNICODE_STRING(device_name, L"\\Device\\libredirect");
	DECLARE_CONST_UNICODE_STRING(dos_device_name, L"\\??\\libredirect");
	//DECLARE_CONST_UNICODE_STRING(sddl, L"D:P(A;;GA;;;WD)");

	NTSTATUS status;

	auto device_init = WdfControlDeviceInitAllocate(driver, &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
	if (!device_init)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		KdPrint(("|LIBREDIRECT|device_init|failed to allocate WDF control device: %d", status));
		return status;
	}

	WdfDeviceInitSetDeviceType(device_init, FILE_DEVICE_NETWORK);
	WdfDeviceInitSetIoType(device_init, WdfDeviceIoDirect);
	status = WdfDeviceInitAssignName(device_init, &device_name);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("|LIBREDIRECT|device_init|failed to create WDF device name:%d", status));
		WdfDeviceInitFree(device_init);
		return status;
	}

	WDF_FILEOBJECT_CONFIG file_config;
	WDF_FILEOBJECT_CONFIG_INIT(&file_config, file_create, file_close, file_cleanup);
	WDF_OBJECT_ATTRIBUTES obj_attrs;
	WDF_OBJECT_ATTRIBUTES_INIT(&obj_attrs);
	obj_attrs.ExecutionLevel = WdfExecutionLevelPassive;
	obj_attrs.SynchronizationScope = WdfSynchronizationScopeNone;
	WdfDeviceInitSetFileObjectConfig(device_init, &file_config, &obj_attrs);
	WDF_OBJECT_ATTRIBUTES_INIT(&obj_attrs);

	status = WdfDeviceCreate(&device_init, &obj_attrs, &device);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("|LIBREDIRECT|device_init|failed to create WDF control device: %d", status));
		WdfDeviceInitFree(device_init);
		return status;
	}

	WDF_IO_QUEUE_CONFIG queue_config;
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue_config, WdfIoQueueDispatchParallel);
	queue_config.EvtIoRead = NULL;
	queue_config.EvtIoWrite = NULL;
	queue_config.EvtIoDeviceControl = device_ioctl;
	WDF_OBJECT_ATTRIBUTES_INIT(&obj_attrs);
	obj_attrs.ExecutionLevel = WdfExecutionLevelPassive;
	obj_attrs.SynchronizationScope = WdfSynchronizationScopeNone;
	WDFQUEUE queue;
	status = WdfIoQueueCreate(device, &queue_config, &obj_attrs, &queue);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("|LIBREDIRECT|device_init|failed to create default WDF queue: %d", status));
		WdfDeviceInitFree(device_init);
		return status;
	}
	status = WdfDeviceCreateSymbolicLink(device, &dos_device_name);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("|LIBREDIRECT|device_init|failed to create device symbolic link", status));
		WdfDeviceInitFree(device_init);
		return status;
	}
	WdfControlFinishInitializing(device);

	return status;
}

void device_uninit()
{
	UNICODE_STRING dos_device_name = { 0 };
	RtlInitUnicodeString(&dos_device_name, L"\\??\\libredirect");
	IoDeleteSymbolicLink(&dos_device_name);
}
