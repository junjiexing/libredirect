#include <ntifs.h>
#include "io_callbacks.h"
#include "wfp_callbacks.h"
#include "common.h"


VOID file_create(IN WDFDEVICE device, IN WDFREQUEST request, IN WDFFILEOBJECT object)
{
	UNREFERENCED_PARAMETER(device);
	UNREFERENCED_PARAMETER(object);
	KdPrint(("|LIBREDIRECT|file_create|"));

	WdfRequestComplete(request, STATUS_SUCCESS);
}

VOID file_close(IN WDFFILEOBJECT object)
{
	KdPrint(("|LIBREDIRECT|file_close|"));
	UNREFERENCED_PARAMETER(object);
}

VOID file_cleanup(IN WDFFILEOBJECT object)
{
	KdPrint(("|LIBREDIRECT|file_cleanup|"));
	UNREFERENCED_PARAMETER(object);
}

static void request_cancel(WDFREQUEST request)
{
	KdPrint(("|LIBREDIRECT|request_cancel|"));
	WdfWaitLockAcquire(connect_list_lck, nullptr);
	for (auto p = read_req_list.Flink; p != &read_req_list; p = p->Flink)
	{
		auto req = CONTAINING_RECORD(p, read_item_t, list_entry);
		if (req->request == request)
		{
			KdPrint(("|LIBREDIRECT|request_cancel|found"));
			RemoveEntryList(p);
			ExFreePool(req);
			break;
		}
	}
	WdfWaitLockRelease(connect_list_lck);

	WdfRequestComplete(request,STATUS_CANCELLED);
}


VOID device_ioctl(IN WDFQUEUE queue, IN WDFREQUEST request,
	IN size_t out_length, IN size_t in_length, IN ULONG code)
{
	UNREFERENCED_PARAMETER(queue);
	//UNREFERENCED_PARAMETER(request);
	UNREFERENCED_PARAMETER(out_length);
	UNREFERENCED_PARAMETER(in_length);
	//UNREFERENCED_PARAMETER(code);


	if (code == IOCTL_GET_CONN)
	{
		KdPrint(("|LIBREDIRECT|device_ioctl|IOCTL_GET_CONN"));
		WdfWaitLockAcquire(connect_list_lck, nullptr);
		if (!IsListEmpty(&connect_list))
		{
			WdfWaitLockRelease(connect_list_lck);

			auto entry = RemoveTailList(&connect_list);
			auto item = CONTAINING_RECORD(entry, conn_item_t, list_entry);
			void* outbuf;
			size_t outlen;
			auto status = WdfRequestRetrieveOutputBuffer(request, sizeof(connect_t), &outbuf, &outlen);
			memcpy(outbuf, &item->conn, sizeof(connect_t));
			ExFreePool(item);
			WdfRequestCompleteWithInformation(request, status, sizeof(connect_t));
			return;
		}
		auto req_item = reinterpret_cast<read_item_t*>(ExAllocatePool(PagedPool, sizeof(read_item_t)));
		req_item->request = request;
		WdfRequestMarkCancelableEx(request, request_cancel);
		InsertHeadList(&read_req_list, &req_item->list_entry);
		WdfWaitLockRelease(connect_list_lck);
		return;
	}
	else if (code == IOCTL_SET_CONN)
	{
		KdPrint(("|LIBREDIRECT|device_ioctl|IOCTL_SET_CONN"));
		void* inbuf;
		size_t inlen;
		auto status = WdfRequestRetrieveInputBuffer(request, sizeof(connect_t), &inbuf, &inlen);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("|LIBREDIRECT|device_ioctl|get input buffer error: %d", status));
			WdfRequestComplete(request, status);
			return;
		}
		if (inlen != sizeof(connect_t))
		{
			KdPrint(("|LIBREDIRECT|device_ioctl|get input buffer len error: %d", inlen));
			WdfRequestComplete(request, STATUS_INVALID_PARAMETER);
			return;
		}

		do_redirect(*reinterpret_cast<connect_t*>(inbuf));
		WdfRequestComplete(request, status);
		return;
	}
	else
	{
		KdPrint(("|LIBREDIRECT|device_ioctl|UNKNOWN code"));
		WdfRequestComplete(request, STATUS_UNSUCCESSFUL);
	}
}


