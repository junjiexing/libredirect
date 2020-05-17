
#include <iostream>
#include <libredirect.h>
#include <asio.hpp>
#include <asio/windows/overlapped_ptr.hpp>

using asio::ip::tcp;

class session
	: public std::enable_shared_from_this<session>
{
public:
	session(tcp::socket socket)
		: socket_(std::move(socket))
	{
		std::cout << "new conn" << std::endl;
		//socket_.lowest_layer()
	}

	void start()
	{
		do_read();
	}

private:
	void do_read()
	{
		auto self(shared_from_this());
		socket_.async_read_some(asio::buffer(data_, max_length),
			[this, self](std::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				do_write(length);
			}
		});
	}

	void do_write(std::size_t length)
	{
		auto self(shared_from_this());
		asio::async_write(socket_, asio::buffer(data_, length),
			[this, self](std::error_code ec, std::size_t /*length*/)
		{
			if (!ec)
			{
				do_read();
			}
		});
	}

	tcp::socket socket_;
	enum { max_length = 1024 };
	char data_[max_length];
};

class server
{
public:
	server(asio::io_context& io_context, short port)
		: acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
	{
		do_accept();
	}

private:
	void do_accept()
	{
		acceptor_.async_accept([this](std::error_code ec, tcp::socket socket)
		{
			if (!ec)
			{
				std::make_shared<session>(std::move(socket))->start();
			}

			do_accept();
		});
	}

	tcp::acceptor acceptor_;
};


int main()
{
	std::thread thd([] {
		asio::io_context io_context;

		server s(io_context, 5001);
		io_context.run();
	});


	HANDLE engine_handle = nullptr;
	auto status = libredirect_init(LIBREDIRECT_INIT_IPV4, &engine_handle);
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
		if (conn.ip_version == 4 && conn.v4.remote_port == 5000)
		{
			// 转到本地的asio echo server上
			inet_pton(AF_INET, "127.0.0.1", &conn.v4.remote_address);
			conn.v4.remote_port = 5001;
			// inet_pton的结果为网络字节序，需要转换
			conn.v4.remote_address.S_un.S_addr = ntohl(conn.v4.remote_address.S_un.S_addr);
		}

		ret = libredirect_write_connect(handle, &conn);
		if (!ret)
		{
			std::cout << "write_connect failed" << std::endl;
			continue;
		}
	}


	thd.join();
	libredirect_uninit(engine_handle);
	return 0;
}