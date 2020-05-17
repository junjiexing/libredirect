#include "wfp_callbacks.h"
#include "wfp.h"

LIST_ENTRY read_req_list;
LIST_ENTRY connect_list;
WDFWAITLOCK connect_list_lck;


#define FORMAT_ADDR4(x) x.S_un.S_un_b.s_b4, x.S_un.S_un_b.s_b3, x.S_un.S_un_b.s_b2, x.S_un.S_un_b.s_b1

#define FORMAT_ADDR6(x)  RtlUshortByteSwap(x.u.Word[0]), \
	RtlUshortByteSwap(x.u.Word[1]), \
	RtlUshortByteSwap(x.u.Word[2]), \
	RtlUshortByteSwap(x.u.Word[3]), \
	RtlUshortByteSwap(x.u.Word[4]), \
	RtlUshortByteSwap(x.u.Word[5]), \
	RtlUshortByteSwap(x.u.Word[6]), \
	RtlUshortByteSwap(x.u.Word[7])


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

	KdPrint(("|LIBREDIRECT|callout_classify|layerId: %d", inFixedValues->layerId));
	connect_t conn;
	conn.process_id = inMetaValues->processId;
	conn.local_redirect_pid = 0xFFFF;
	if (inFixedValues->layerId == FWPS_LAYER_ALE_CONNECT_REDIRECT_V4)
	{
		conn.ip_version = 4;
		// FWP_UINT32
		conn.v4.local_address = *reinterpret_cast<IN_ADDR*>(&inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_LOCAL_ADDRESS].value.uint32);
		// FWP_UINT32
		conn.v4.remote_address = *reinterpret_cast<IN_ADDR*>(&inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_REMOTE_ADDRESS].value.uint32);
		// FWP_UINT16
		conn.v4.local_port = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_LOCAL_PORT].value.uint16;
		// FWP_UINT16
		conn.v4.remote_port = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V4_IP_REMOTE_PORT].value.uint16;
		KdPrint(("|LIBREDIRECT|callout_classify|IPv4, %d.%d.%d.%d:%hu --> %d.%d.%d.%d:%hu, PID: %lld",
			FORMAT_ADDR4(conn.v4.local_address), conn.v4.local_port, FORMAT_ADDR4(conn.v4.remote_address), conn.v4.remote_port, conn.process_id));
	}
	else if (inFixedValues->layerId == FWPS_LAYER_ALE_CONNECT_REDIRECT_V6)
	{
		conn.ip_version = 6;
		// FWP_BYTE_ARRAY16_TYPE
		conn.v6.local_address = *reinterpret_cast<IN6_ADDR*>(inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_IP_LOCAL_ADDRESS].value.byteArray16);
		// FWP_BYTE_ARRAY16_TYPE
		conn.v6.remote_address = *reinterpret_cast<IN6_ADDR*>(inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_IP_REMOTE_ADDRESS].value.byteArray16);
		// FWP_UINT16
		conn.v6.local_port = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_IP_LOCAL_PORT].value.uint16;
		// FWP_UINT16
		conn.v6.remote_port = inFixedValues->incomingValue[FWPS_FIELD_ALE_CONNECT_REDIRECT_V6_IP_REMOTE_PORT].value.uint16;
		conn.v6.remote_scope_id = inMetaValues->remoteScopeId;
		KdPrint(("|LIBREDIRECT|callout_classify|IPv6, %x:%x:%x:%x:%x:%x:%x::%x:%hu --> %x:%x:%x:%x:%x:%x:%x::%x:%hu, PID: %lld",
			FORMAT_ADDR6(conn.v6.local_address), conn.v6.local_port, FORMAT_ADDR6(conn.v6.remote_address), conn.v6.remote_port, conn.process_id));
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
		auto entry = RemoveTailList(&read_req_list);
		WdfWaitLockRelease(connect_list_lck);

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
		auto remote_addr = reinterpret_cast<SOCKADDR_IN*>(&connect_request->remoteAddressAndPort);
		auto local_addr = reinterpret_cast<SOCKADDR_IN*>(&connect_request->localAddressAndPort);
#if DBG
		IN_ADDR local_ip;
		local_ip.S_un.S_addr = RtlUlongByteSwap(local_addr->sin_addr.S_un.S_addr);
		auto local_port = RtlUshortByteSwap(local_addr->sin_port);
		IN_ADDR remote_ip;
		remote_ip.S_un.S_addr = RtlUlongByteSwap(remote_addr->sin_addr.S_un.S_addr);
		auto remote_port = RtlUshortByteSwap(remote_addr->sin_port);

		auto mod_local_ip = conn.v4.local_address;
		auto mod_local_port = conn.v4.local_port;
		auto mod_remote_ip = conn.v4.remote_address;
		auto mod_remote_port = conn.v4.remote_port;


		DbgPrint("|LIBREDIRECT|do_redirect|IPv4 origin: %d.%d.%d.%d:%hu --> %d.%d.%d.%d:%hu; modified: %d.%d.%d.%d:%hu --> %d.%d.%d.%d:%hu",
			FORMAT_ADDR4(local_ip), local_port,
			FORMAT_ADDR4(remote_ip), remote_port,
			FORMAT_ADDR4(mod_local_ip), mod_local_port,
			FORMAT_ADDR4(mod_remote_ip), mod_remote_port);
#endif

		remote_addr->sin_addr.S_un.S_addr = RtlUlongByteSwap(conn.v4.remote_address.S_un.S_addr);
		remote_addr->sin_port = RtlUshortByteSwap(conn.v4.remote_port);
		local_addr->sin_addr.S_un.S_addr = RtlUlongByteSwap(conn.v4.local_address.S_un.S_addr);
		local_addr->sin_port = RtlUshortByteSwap(conn.v4.local_port);
	}
	else
	{
		auto remote_addr = reinterpret_cast<SOCKADDR_IN6*>(&connect_request->remoteAddressAndPort);
		auto local_addr = reinterpret_cast<SOCKADDR_IN6*>(&connect_request->localAddressAndPort);
#if DBG
		auto local_ip = local_addr->sin6_addr;
		auto local_port = local_addr->sin6_port;
		auto local_scope = local_addr->sin6_scope_id;
		auto remote_ip = remote_addr->sin6_addr;
		auto remote_port = remote_addr->sin6_port;
		auto remote_scope = local_addr->sin6_scope_id;

		auto mod_local_ip = conn.v6.local_address;
		auto mod_local_port = conn.v6.local_port;
		auto mod_remote_ip = conn.v6.remote_address;
		auto mod_remote_port = conn.v6.remote_port;
		auto mod_remote_scope = conn.v6.remote_scope_id.Value;

		DbgPrint("|LIBREDIRECT|callout_classify|IPv6 origin: %x:%x:%x:%x:%x:%x:%x::%x%%%d:%hu --> %x:%x:%x:%x:%x:%x:%x::%x%%%d:%hu; "
			"modified: %x:%x:%x:%x:%x:%x:%x::%x%%%d:%hu --> %x:%x:%x:%x:%x:%x:%x::%x%%%d:%hu",
			FORMAT_ADDR6(local_ip), local_scope, RtlUshortByteSwap(local_port),
			FORMAT_ADDR6(remote_ip), remote_scope, RtlUshortByteSwap(remote_port),
			FORMAT_ADDR6(mod_local_ip), local_scope, mod_local_port,
			FORMAT_ADDR6(mod_remote_ip), mod_remote_scope, mod_remote_port);

#endif
		remote_addr->sin6_addr = conn.v6.remote_address;
		remote_addr->sin6_port = RtlUshortByteSwap(conn.v6.remote_port);
		remote_addr->sin6_scope_id = conn.v6.remote_scope_id.Value;
		local_addr->sin6_addr = conn.v6.local_address;
		local_addr->sin6_port = RtlUshortByteSwap(conn.v6.local_port);
	}



	connect_request->localRedirectHandle = redirect_handle;
	connect_request->localRedirectTargetPID = conn.local_redirect_pid;

	FwpsApplyModifiedLayerData(conn._priv.classify_handle, writeable_layer_data, FWPS_CLASSIFY_FLAG_REAUTHORIZE_IF_MODIFIED_BY_OTHERS);
	conn._priv.classify_out.actionType = FWP_ACTION_PERMIT;
	conn._priv.classify_out.rights |= FWPS_RIGHT_ACTION_WRITE;
	FwpsCompleteClassify(conn._priv.classify_handle, 0, &conn._priv.classify_out);
	FwpsReleaseClassifyHandle(conn._priv.classify_handle);
}