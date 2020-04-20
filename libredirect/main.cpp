#define INITGUID
#include <WinSock2.h>
#include <windows.h>
#include <fwpmu.h>
#include <fwpstypes.h>
#include <iostream>

#include <common.h>


void process(connect_t& conn)
{
	if (conn.ip_version == 4)
	{
		if (conn.v4.remote_port == 5000)
		{
			std::cout << "redirect." << std::endl;
			// 注意这里是小端
			auto remote_addr = reinterpret_cast<IN_ADDR*>(&conn.v4.remote_address);
			remote_addr->S_un.S_un_b.s_b4 = 127;
			remote_addr->S_un.S_un_b.s_b3 = 0;
			remote_addr->S_un.S_un_b.s_b2 = 0;
			remote_addr->S_un.S_un_b.s_b1 = 1;
		}
	}
	else
	{
		// TODO: IPv6
	}
}


int main()
{
	FWPM_SESSION session = { 0 };
	session.flags = FWPM_SESSION_FLAG_DYNAMIC;	// session结束后自动销毁所有callout和filter
	HANDLE engine_handle;
	auto status = FwpmEngineOpen(nullptr, RPC_C_AUTHN_WINNT, nullptr, &session, &engine_handle);
	if (status != ERROR_SUCCESS)
	{
		std::cout << "FwpmEngineOpen failed" << std::endl;
		return status;
	}

	status = FwpmTransactionBegin(engine_handle, 0);
	if (status != ERROR_SUCCESS)
	{
		std::cout << "FwpmTransactionBegin failed" << std::endl;
		return status;
	}



	FWPM_CALLOUT callout_v4 = { 0 };
	FWPM_DISPLAY_DATA display_data = { 0 };
	wchar_t callout_display_name_v4[] = L"LibredirectCalloutV4";
	wchar_t callout_display_desc_v4[] = L"IPv4 callout for Libredirect";
	display_data.name = callout_display_name_v4;
	display_data.description = callout_display_desc_v4;

	callout_v4.calloutKey = LIBREDIRECT_CALLOUT_GUID_V4;
	callout_v4.displayData = display_data;
	callout_v4.applicableLayer = FWPM_LAYER_ALE_CONNECT_REDIRECT_V4;
	callout_v4.flags = 0;
	status = FwpmCalloutAdd(engine_handle, &callout_v4, nullptr, nullptr);
	if (status != ERROR_SUCCESS)
	{
		std::cout << "FwpmCalloutAdd v4 failed" << std::endl;;
		return status;
	}

	FWPM_CALLOUT callout_v6 = { 0 };
	FWPM_DISPLAY_DATA display_data_v6 = { 0 };
	wchar_t callout_display_name_v6[] = L"LibredirectCalloutV6";
	wchar_t callout_display_desc_v6[] = L"IPv6 callout for Libredirect";
	display_data_v6.name = callout_display_name_v6;
	display_data_v6.description = callout_display_desc_v6;

	callout_v6.calloutKey = LIBREDIRECT_CALLOUT_GUID_V6;
	callout_v6.displayData = display_data_v6;
	callout_v6.applicableLayer = FWPM_LAYER_ALE_CONNECT_REDIRECT_V6;
	callout_v6.flags = 0;
	status = FwpmCalloutAdd(engine_handle, &callout_v6, nullptr, nullptr);
	if (status != ERROR_SUCCESS)
	{
		std::cout << "FwpmCalloutAdd v6 failed" << std::endl;;
		return status;
	}



	FWPM_SUBLAYER sublayer = { 0 };
	sublayer.subLayerKey = LIBREDIRECT_SUBLAYER_GUID;
	wchar_t sublayer_display_name[] = L"LibredirectSublayer";;
	sublayer.displayData.name = sublayer_display_name;
	wchar_t sublayer_display_desc[] = L"Sublayer for Libredirect";
	sublayer.displayData.description = sublayer_display_desc;
	sublayer.flags = 0;
	sublayer.weight = 0x0f;
	status = FwpmSubLayerAdd(engine_handle, &sublayer, nullptr);
	if (status != ERROR_SUCCESS)
	{
		std::cout << "FwpmSubLayerAdd failed" << std::endl;;
		return status;
	}

	FWPM_FILTER filter_v4 = { 0 };
	UINT64 filter_id_v4 = 0;
	wchar_t filter_display_name_v4[] = L"LibredirectFilterV4";
	filter_v4.displayData.name = filter_display_name_v4;
	wchar_t filter_display_desc_v4[] = L"IPv4 filter for Libredirect";
	filter_v4.displayData.description = filter_display_desc_v4;
	filter_v4.action.type = FWP_ACTION_CALLOUT_TERMINATING;
	filter_v4.subLayerKey = LIBREDIRECT_SUBLAYER_GUID;
	filter_v4.weight.type = FWP_UINT8;
	filter_v4.weight.uint8 = 0xf;
	filter_v4.numFilterConditions = 0;
	filter_v4.layerKey = FWPM_LAYER_ALE_CONNECT_REDIRECT_V4;
	filter_v4.action.calloutKey = LIBREDIRECT_CALLOUT_GUID_V4;
	status = FwpmFilterAdd(engine_handle, &filter_v4, nullptr, &filter_id_v4);
	if (status != ERROR_SUCCESS)
	{
		std::cout << "Add IPv4 filter failed:" << status << std::endl;;
		return status;
	}


	FWPM_FILTER filter_v6 = { 0 };
	UINT64 filter_id_v6 = 0;
	wchar_t filter_display_name_v6[] = L"LibredirectFilterV6";
	filter_v6.displayData.name = filter_display_name_v6;
	wchar_t filter_display_desc_v6[] = L"IPv6 filter for Libredirect";
	filter_v6.displayData.description = filter_display_desc_v6;
	filter_v6.action.type = FWP_ACTION_CALLOUT_TERMINATING;
	filter_v6.subLayerKey = LIBREDIRECT_SUBLAYER_GUID;
	filter_v6.weight.type = FWP_UINT8;
	filter_v6.weight.uint8 = 0xf;
	filter_v6.numFilterConditions = 0;
	filter_v6.layerKey = FWPM_LAYER_ALE_CONNECT_REDIRECT_V6;
	filter_v6.action.calloutKey = LIBREDIRECT_CALLOUT_GUID_V6;
	status = FwpmFilterAdd(engine_handle, &filter_v6, nullptr, &filter_id_v6);
	if (status != ERROR_SUCCESS)
	{
		std::cout << "Add IPv6 filter failed:" << status << std::endl;;
		return status;
	}

	status = FwpmTransactionCommit(engine_handle);
	if (status != ERROR_SUCCESS)
	{
		std::cout << "FwpmTransactionCommit failed" << std::endl;;
		return status;
	}

	//getchar();
	//关闭引擎的handle会自动删除filter，转发引擎也会停止工作
	//FwpmEngineClose(engine_handle);

	std::cout << "success" << std::endl;

	auto handle = CreateFileA("\\\\.\\libredirect",
		GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, INVALID_HANDLE_VALUE);
	if (handle == INVALID_HANDLE_VALUE)
	{
		std::cout << "create file failed" << std::endl;
		return 1;
	}

	for (;;)
	{
		connect_t conn;
		DWORD recv_num;
		auto ret = DeviceIoControl(handle, IOCTL_GET_CONN, nullptr, 0, &conn, sizeof(conn), &recv_num, nullptr);
		if (!ret)
		{
			std::cout << "recv failed" << std::endl;
			continue;
		}

		if (conn.ip_version == 4)
		{
			UINT32 local_addr = conn.v4.local_address;
			UINT16 local_port = conn.v4.local_port;
			UINT32 remote_addr = conn.v4.remote_address;
			UINT16 remote_port = conn.v4.remote_port;
			printf("%d.%d.%d.%d:%d --> %d.%d.%d.%d:%d, PID:%lld\n",
				FORMAT_ADDR(local_addr), local_port,
				FORMAT_ADDR(remote_addr), remote_port, conn.process_id);
		}
		else
		{
			// TODO: IPv6
		}

		process(conn);

		ret = DeviceIoControl(handle, IOCTL_SET_CONN, &conn, sizeof(conn), nullptr, 0, &recv_num, nullptr);
		if (!ret)
		{
			std::cout << "send failed" << std::endl;
			continue;
		}

	}
	return 0;
}