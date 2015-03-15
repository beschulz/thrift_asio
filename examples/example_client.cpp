//
// Created by Benjamin Schulz on 15/03/15.
//

#include <iostream>

#include <betabugs/networking/thrift_asio_client.hpp>
#include <chat_client.h>
#include <chat_server.h>

class chat_client_handler : public betabugs::networking::thrift_asio_client<
	example::chat::chat_serverClient,
	example::chat::chat_clientProcessor,
	example::chat::chat_clientIf
>
{
  public:
	using betabugs::networking::thrift_asio_client<
		example::chat::chat_serverClient,
		example::chat::chat_clientProcessor,
		example::chat::chat_clientIf
	>::thrift_asio_client;


	virtual void on_set_user_name_failed(const std::string& why) override
	{
		std::cerr << "setting user name failed: " << why << std::endl;
	}

	virtual void on_message(const std::string& from_user, const std::string& message) override
	{
		std::cout << from_user << ": " << message << std::endl;
	}

	virtual void on_send_message_failed(const std::string& why) override
	{
		std::cerr << "sending message failed: " << why << std::endl;
	}

	// called by the transport
	virtual void on_error(const boost::system::error_code& ec) override
	{
		std::clog << "!! error: " << ec.message() << std::endl;
		this->reconnect_in(1.0f);
	}

	virtual void on_connected() override
	{
		std::clog << "!! connected" << std::endl;
		client_.set_user_name("I am the Source of foo!");
	}

	virtual void on_disconnected() override
	{
		std::clog << "!! disconnected" << std::endl;
	}
};

int main(int argc, char* argv[])
{
	(void) argc;
	(void) argv;

	std::string host_name = "127.0.0.1";
	std::string service_name = "1528";

	{
		boost::asio::io_service io_service;
		boost::asio::io_service::work work(io_service);

		chat_client_handler handler(io_service, host_name, service_name);

		while (true)
		{
			boost::system::error_code ec;
			while(io_service.poll_one(ec))
			{
				if (ec) handler.on_error(ec);
			}

			handler.update();

			sleep(1);
			std::clog << "loop" << std::endl;
		}
	}

	return 0;
}
