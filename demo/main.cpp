
#include <iostream>
#include "../libredirect/libredirect.h"

void process(connect_t& conn)
{
	if (conn.ip_version == 4 && conn.v4.remote_port == 5000)
	{
		std::cout << "redirect v4." << std::endl;
		//inet_pton要求地址为网络字节序
		auto addr = htonl(conn.v4.remote_address.S_un.S_addr);
		inet_pton(AF_INET, "127.0.0.1", &addr);
		conn.v4.remote_port = 5001;
	}
	else if (conn.ip_version == 6 && conn.v6.remote_port == 5000)
	{
		std::cout << "redirect v6." << std::endl;
		inet_pton(AF_INET6, "::1", &conn.v6.remote_address);
		conn.v6.remote_port = 5001;
		conn.v6.remote_scope_id.Value = 0;
	}
}


int main()
{
	HANDLE engine_handle = nullptr;
	auto status = libredirect_init(LIBREDIRECT_INIT_IPV4 | LIBREDIRECT_INIT_IPV6, &engine_handle);
	if (status != ERROR_SUCCESS)
	{
		std::cout << "libredirect_init failed" << std::endl;
		return 1;
	}

	auto handle = libredirect_open();
	if (handle == INVALID_HANDLE_VALUE)
	{
		std::cout << "libredirect_open file failed" << std::endl;
		return 1;
	}

	for (;;)
	{
		connect_t conn;
		auto ret = libredirect_read_connect(handle, &conn);
		if (!ret)
		{
			std::cout << "read_connect failed" << std::endl;
			continue;
		}

		if (conn.ip_version == 4)
		{
			auto local_addr = conn.v4.local_address;
			auto local_port = conn.v4.local_port;
			auto remote_addr = conn.v4.remote_address;
			auto remote_port = conn.v4.remote_port;
			char local_addr_str[20];
			inet_ntop(AF_INET, &local_addr, local_addr_str, sizeof(local_addr_str));
			char remote_addr_str[20];
			inet_ntop(AF_INET, &remote_addr, remote_addr_str, sizeof(remote_addr_str));
			printf("%s:%d --> %s:%d, PID:%lld\n",
				local_addr_str, local_port, remote_addr_str, remote_port, conn.process_id);
		}
		else
		{
			auto local_addr = conn.v6.local_address;
			auto local_port = conn.v6.local_port;
			auto remote_addr = conn.v6.remote_address;
			auto remote_port = conn.v6.remote_port;
			char local_addr_str[50];
			inet_ntop(AF_INET6, &local_addr, local_addr_str, sizeof(local_addr_str));
			char remote_addr_str[50];
			inet_ntop(AF_INET6, &remote_addr, remote_addr_str, sizeof(remote_addr_str));
			printf("%s:%d --> %s:%d, PID:%lld\n",
				local_addr_str, local_port, remote_addr_str, remote_port, conn.process_id);
		}

		process(conn);

		ret = libredirect_write_connect(handle, &conn);
		if (!ret)
		{
			std::cout << "write_connect failed" << std::endl;
			continue;
		}

	}

	libredirect_uninit(engine_handle);
	return 0;
}

