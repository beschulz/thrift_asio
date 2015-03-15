//
// Created by Benjamin Schulz on 15/03/15.
//

#include <betabugs/networking/thrift_asio_server.hpp>
#include <chat_server.h>
#include <chat_client.h>

class chat_server_handler
	: public example::chat::chat_serverIf
	, public betabugs::networking::thrift_asio_transport::event_handlers
{

  public:
	virtual void set_user_name(const std::string& name) override
	{

	}

	virtual void send_message(const std::string& to_user, const std::string& message) override
	{

	}

	virtual void broadcast_message(const std::string& message) override
	{

	}

	// functions called by thrift_asio_transport
	virtual void on_error(const boost::system::error_code& ec)
	{
		std::clog << "thrift_asio_transport::on_error" << ec.message() << std::endl;
	}

	virtual void on_connected()
	{
		std::clog << "thrift_asio_transport::on_connected" << std::endl;
	}

	virtual void on_disconnected()
	{
		std::clog << "thrift_asio_transport::on_disconnected" << std::endl;
	}

	// functions called by thrift_asio_server
	void on_client_connected(std::shared_ptr<betabugs::networking::thrift_asio_transport> transport)
	{

	}

	void before_process(boost::shared_ptr<apache::thrift::protocol::TBinaryProtocol>)
	{

	}

	void after_process()
	{

	}

  private:
	//example::chat::chat_clientClient
};

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

	while (true)
	{
		while (io_service.poll_one());
		sleep(1);
		std::clog << "loop" << std::endl;
	}

	return 0;
}
