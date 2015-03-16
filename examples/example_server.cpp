//
// Created by Benjamin Schulz on 15/03/15.
//

#include <betabugs/networking/thrift_asio_server.hpp>
#include <betabugs/networking/thrift_asio_connection_management_mixin.hpp>
#include <chat_server.h>
#include <chat_client.h>
#include <thread> // for sleep

/**
* a chat-session holds the user name and a client
* */
struct session
{
	session(const boost::shared_ptr<apache::thrift::protocol::TProtocol>& output_protocol)
		: client(output_protocol)
	{

	}

	std::string user_name;
	example::chat::chat_clientClient client;
};


class chat_server_handler
	: public example::chat::chat_serverIf
	  , public betabugs::networking::thrift_asio_transport::event_handlers
	  , public betabugs::networking::thrift_asio_connection_management_mixin<session>
{

  public:
	virtual void set_user_name(const std::string& name) override
	{
		for (auto& p : clients_)
		{
			if (p.second->user_name == name)
			{
				p.second->client.on_set_user_name_failed("username already taken");
				return;
			}
		}

		assert(current_client_);
		current_client_->user_name = name;
		current_client_->client.on_set_user_name_succeeded();
	}

	virtual void broadcast_message(const std::string& message) override
	{
		assert(current_client_);
		for (auto& s : clients_)
		{
			if (s.second != current_client_)
				s.second->client.on_message(current_client_->user_name, message);
		}
	}
};


/// This is the simple version that blocks.
void the_blocking_version(boost::asio::io_service& io_service)
{
	io_service.run();
}


/*! this is why we went through all this efford. You're in control of the event lopp. huray!!!
 * this version is more suitable for realtime applications
 * */
void the_non_blocking_loop(boost::asio::io_service& io_service)
{
	while (true)
	{
		while (io_service.poll_one());

		// do some other work, e.g. sleep
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}


int main(int argc, char* argv[])
{
	(void) argc;
	(void) argv;

	auto handler = boost::make_shared<chat_server_handler>();
	auto processor = example::chat::chat_serverProcessor
		(
			handler
		);

	betabugs::networking::thrift_asio_server<chat_server_handler> server;

	boost::asio::io_service io_service;
	boost::asio::io_service::work work(io_service);

	server.serve(io_service, processor, handler, 1528);

	the_blocking_version(io_service);

	/// this version is more suitable for realtime applications
	//the_non_blocking_loop(io_service);

	return 0;
}
