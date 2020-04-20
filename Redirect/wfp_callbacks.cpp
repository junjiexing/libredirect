#include "wfp_callbacks.h"
#include "wfp.h"

LIST_ENTRY read_req_list;
LIST_ENTRY connect_list;
WDFWAITLOCK connect_list_lck;


#define FORMAT_ADDR(x) (x>>24)&0xFF, (x>>16)&0xFF, (x>>8)&0xFF, x&0xFF


void callout_classify(
	const FWPS_INCOMING_VALUES* inFixedValues,
	const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
	void* layerData,
	const void* classifyContext,
	const FWPS_FILTER* filter,
	UINT64 flowContext,
	FWPS_CLASSIFY_OUT* classifyOut)
{
	UNREFERENCED_PARAMETER(layerData);
	UNREFERENCED_PARAMETER(flowContext);

	KdPrint(("|LIBREDIRECT|callout_classify|filter context: %lld", filter->context));
	connect_t conn;
	if (filter->context == 4)
	{
		conn.ip_version = 4;
		conn.v4.local_address = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_LOCAL_ADDRESS].value.uint32;
		conn.v4.remote_address = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_REMOTE_ADDRESS].value.uint32;
		conn.v4.local_port = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_LOCAL_PORT].value.uint16;
		conn.v4.remote_port = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_REMOTE_PORT].value.uint16;
		conn.process_id = inMetaValues->processId;
		KdPrint(("|LIBREDIRECT|callout_classify|IPv4, %d.%d.%d.%d:%hu --> %d.%d.%d.%d:%hu, PID: %lld",
			FORMAT_ADDR(conn.v4.local_address), conn.v4.local_port, FORMAT_ADDR(conn.v4.remote_address), conn.v4.remote_port, conn.process_id));
	}
	else if (filter->context == 6)
	{
		// TODO:
		conn.ip_version = 6;
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}
	else
	{
		KdPrint(("|LIBREDIRECT|callout_classify|invalid filter context"));
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}



	classifyOut->actionType = FWP_ACTION_PERMIT;

	FWPS_CONNECTION_REDIRECT_STATE redirect_state = FWPS_CONNECTION_NOT_REDIRECTED;

	if (FWPS_IS_METADATA_FIELD_PRESENT(inMetaValues, FWPS_METADATA_FIELD_REDIRECT_RECORD_HANDLE))
	{
		VOID* redirect_context = nullptr;
		redirect_state = FwpsQueryConnectionRedirectState(
			inMetaValues->redirectRecords, redirect_handle, &redirect_context);
	}

	switch (redirect_state)
	{
	case FWPS_CONNECTION_NOT_REDIRECTED:
		KdPrint(("|LIBREDIRECT|callout_classify|Ale connection not redirected, goto redirect handler"));
		break;
	case FWPS_CONNECTION_REDIRECTED_BY_OTHER:
		KdPrint(("|LIBREDIRECT|callout_classify|Ale connection redirected by other, permit"));
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	case FWPS_CONNECTION_REDIRECTED_BY_SELF:
		KdPrint(("|LIBREDIRECT|callout_classify|Ale connection not redirect by self, permit"));
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	case FWPS_CONNECTION_PREVIOUSLY_REDIRECTED_BY_SELF:
		KdPrint(("|LIBREDIRECT|callout_classify|Ale connection redirect by self previously, permit"));
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}


	UINT64 classify_handle = 0;
	NTSTATUS status = FwpsAcquireClassifyHandle((void*)classifyContext, 0, &classify_handle);
	if (status != STATUS_SUCCESS)
	{
		KdPrint(("|LIBREDIRECT|callout_classify|FwpsAcquireClassifyHandle error!status=%#x", status));
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}


	FwpsPendClassify(classify_handle, filter->filterId, 0, classifyOut);
	classifyOut->actionType = FWP_ACTION_BLOCK;
	classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;

	conn._priv.classify_handle = classify_handle;
	conn._priv.classify_out = *classifyOut;
	conn._priv.filter_id = filter->filterId;

	WdfWaitLockAcquire(connect_list_lck, nullptr);
	if (!IsListEmpty(&read_req_list))
	{
		WdfWaitLockRelease(connect_list_lck);

		auto entry = RemoveTailList(&read_req_list);
		auto item = CONTAINING_RECORD(entry, read_item_t, list_entry);
		WdfRequestUnmarkCancelable(item->request);
		void* outbuf;
		size_t outlen;
		status = WdfRequestRetrieveOutputBuffer(item->request, sizeof(connect_t), &outbuf, &outlen);
		memcpy(outbuf, &conn, sizeof(connect_t));
		WdfRequestCompleteWithInformation(item->request, status, sizeof(connect_t));
		ExFreePool(item);
		return;
	}

	auto item = reinterpret_cast<conn_item_t*>(ExAllocatePool(PagedPool, sizeof(conn_item_t)));
	item->conn = conn;
	InsertHeadList(&connect_list, &item->list_entry);
	WdfWaitLockRelease(connect_list_lck);
	return;
}



NTSTATUS NTAPI callout_notify(
	FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	const GUID* filterKey,
	FWPS_FILTER* filter)
{
	UNREFERENCED_PARAMETER(filterKey);
	UNREFERENCED_PARAMETER(filter);

	NTSTATUS status = STATUS_SUCCESS;
	switch (notifyType) {
	case FWPS_CALLOUT_NOTIFY_ADD_FILTER:
		KdPrint(("|LIBREDIRECT|callout_notify|A new filter has registered, filterId: %lld", filter->filterId));
		break;
	case FWPS_CALLOUT_NOTIFY_DELETE_FILTER:
		// 删除一个filter时，将链表中与这个filter相关的连接请求都直接放行
		KdPrint(("|LIBREDIRECT|callout_notify|A filter has just been deleted, filterId: %lld", filter->filterId));
		WdfWaitLockAcquire(connect_list_lck, nullptr);
		for (auto p = connect_list.Flink; p != &connect_list; p = p->Flink)
		{
			auto item = CONTAINING_RECORD(p, conn_item_t, list_entry);
			if (item->conn._priv.filter_id == filter->filterId)
			{
				KdPrint(("|LIBREDIRECT|callout_notify|remove"));
				RemoveEntryList(p);
				do_redirect(item->conn);
				ExFreePool(item);
			}
		}
		WdfWaitLockRelease(connect_list_lck);
		break;
	}
	return status;
}


void NTAPI callout_flow_delete(UINT16 layerId, UINT32 calloutId, UINT64 flowContext)
{
	UNREFERENCED_PARAMETER(layerId);
	UNREFERENCED_PARAMETER(calloutId);
	UNREFERENCED_PARAMETER(flowContext);
}


void do_redirect(connect_t& conn)
{
	VOID* writeable_layer_data = NULL;
	auto status = FwpsAcquireWritableLayerDataPointer(conn._priv.classify_handle, conn._priv.filter_id, 0, &writeable_layer_data, &conn._priv.classify_out);
	if (status != STATUS_SUCCESS)
	{
		KdPrint(("|LIBREDIRECT|callout_classify|FwpsAcquireWritableLayerDataPointer error!status=%#x", status));
		FwpsReleaseClassifyHandle(conn._priv.classify_handle);
		conn._priv.classify_out.actionType = FWP_ACTION_PERMIT;
		FwpsCompleteClassify(conn._priv.classify_handle, 0, &conn._priv.classify_out);
		return;
	}

	auto connect_request = reinterpret_cast<FWPS_CONNECT_REQUEST*>(writeable_layer_data);
	connect_request->localRedirectHandle = redirect_handle;


	if (conn.ip_version == 4)
	{
		SOCKADDR_IN* remote_addr = reinterpret_cast<SOCKADDR_IN*>(&connect_request->remoteAddressAndPort);
		SOCKADDR_IN* local_addr = reinterpret_cast<SOCKADDR_IN*>(&connect_request->localAddressAndPort);
#if DBG
		UINT32 local_ip = local_addr->sin_addr.S_un.S_addr;
		UINT16 local_port = local_addr->sin_port;
		UINT32 remote_ip = remote_addr->sin_addr.S_un.S_addr;
		UINT16 remote_port = remote_addr->sin_port;

		UINT32 mod_local_ip = conn.v4.local_address;
		UINT16 mod_local_port = conn.v4.local_port;
		UINT32 mod_remote_ip = conn.v4.remote_address;
		UINT16 mod_remote_port = conn.v4.remote_port;


		DbgPrint("|LIBREDIRECT|callout_notify|origin: %d.%d.%d.%d:%hu --> %d.%d.%d.%d:%hu;modified: %d.%d.%d.%d:%hu --> %d.%d.%d.%d:%hu",
			FORMAT_ADDR(RtlUlongByteSwap(local_ip)), RtlUshortByteSwap(local_port),
			FORMAT_ADDR(RtlUlongByteSwap(remote_ip)), RtlUshortByteSwap(remote_port),
			FORMAT_ADDR(mod_local_ip), mod_local_port, FORMAT_ADDR(mod_remote_ip), mod_remote_port);
#endif

		remote_addr->sin_addr.S_un.S_addr = RtlUlongByteSwap(conn.v4.remote_address);
		remote_addr->sin_port = RtlUshortByteSwap(conn.v4.remote_port);
		local_addr->sin_addr.S_un.S_addr = RtlUlongByteSwap(conn.v4.local_address);
		local_addr->sin_port = RtlUshortByteSwap(conn.v4.local_port);
	}
	else
	{
		//TODO: IPv6
	}



	connect_request->localRedirectHandle = redirect_handle;
	connect_request->localRedirectTargetPID = 0xFFFF;

	FwpsApplyModifiedLayerData(conn._priv.classify_handle, writeable_layer_data, FWPS_CLASSIFY_FLAG_REAUTHORIZE_IF_MODIFIED_BY_OTHERS);
	conn._priv.classify_out.actionType = FWP_ACTION_PERMIT;
	conn._priv.classify_out.rights |= FWPS_RIGHT_ACTION_WRITE;
	FwpsCompleteClassify(conn._priv.classify_handle, 0, &conn._priv.classify_out);
	FwpsReleaseClassifyHandle(conn._priv.classify_handle);
}