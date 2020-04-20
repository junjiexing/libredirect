#include "wfp.h"
#include "wfp_callbacks.h"
#include <stddef.h>
#include "common.h"

HANDLE filter_engine_handle = nullptr;
HANDLE redirect_handle = nullptr;
static UINT32 callout_id;



static NTSTATUS register_callout(DEVICE_OBJECT* wdm_device)
{
	NTSTATUS status = STATUS_SUCCESS;
	FWPS_CALLOUT callout = { 0 };

	if (!filter_engine_handle)
		return STATUS_INVALID_HANDLE;

	// callout的注册必须在内核态，但是可以在用户态添加到引擎中
	callout.calloutKey = FORCEPROXY_CALLOUT_GUID;
	callout.classifyFn = &callout_classify;
	callout.notifyFn = &callout_notify;
	callout.flowDeleteFn = &callout_flow_delete;
	status = FwpsCalloutRegister((void*)wdm_device, &callout, &callout_id);
	if (!NT_SUCCESS(status)) {
		KdPrint(("|LIBREDIRECT|register_callout|Failed to register callout, status 0x%08x", status));
		return status;
	}

	return status;
}

static NTSTATUS unregister_callout()
{
	return FwpsCalloutUnregisterById(callout_id);
}




NTSTATUS wfp_init(PDEVICE_OBJECT dev_obj)
{
	FWPM_SESSION session = { 0 };
	session.flags = FWPM_SESSION_FLAG_DYNAMIC;	// session结束后自动销毁所有callout和filter
	auto status = FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, &session, &filter_engine_handle);
	if (!NT_SUCCESS(status))
	{
		return status;
	}
	status = FwpmTransactionBegin(filter_engine_handle, 0);
	if (!NT_SUCCESS(status))
	{
		return status;
	}

	status = register_callout(dev_obj);
	if (!NT_SUCCESS(status))
	{
		FwpmTransactionAbort(filter_engine_handle);
		return status;
	}

	status = FwpsRedirectHandleCreate(&FORCEPROXY_SUBLAYER_GUID, 0, &redirect_handle);
	if (status != STATUS_SUCCESS)
	{
		unregister_callout();
		FwpmTransactionAbort(filter_engine_handle);
		KdPrint(("|LIBREDIRECT|wfp_init|Failed to create redirect handle, status 0x%08x", status));
		return status;
	}

	status = FwpmTransactionCommit(filter_engine_handle);
	if (!NT_SUCCESS(status))
	{
		unregister_callout();
	}

	WdfWaitLockCreate(WDF_NO_OBJECT_ATTRIBUTES, &connect_list_lck);
	InitializeListHead(&connect_list);
	InitializeListHead(&read_req_list);

	return status;
}

void wfp_uninit()
{
	if (redirect_handle)
	{
		FwpsRedirectHandleDestroy(redirect_handle);
	}

	auto status = unregister_callout();
	if (!NT_SUCCESS(status))
		KdPrint(("|LIBREDIRECT|wfp_uninit|Failed to unregister callout, status: 0x%08x", status));

	if (filter_engine_handle)
	{
		FwpmEngineClose(filter_engine_handle);
	}
}