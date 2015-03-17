//
// Created by Benjamin Schulz on 15/03/15.
//

#include <iostream>

#include <betabugs/networking/thrift_asio_client.hpp>
#include <chat_client.h>
#include <chat_server.h>
#include <boost/asio.hpp>
#include <thread>


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

	chat_client_handler(
		boost::asio::io_service& io_service,
		const std::string& host_name,
		const std::string& service_name
	)
		: thrift_asio_client(io_service, host_name, service_name)
		, stdin_(io_service, ::dup(STDIN_FILENO))
		, input_buffer_(1024)
	{
	}


	virtual void on_set_user_name_failed(const std::string& why) override
	{
		std::cerr << "setting user name failed: " << why << std::endl;
		read_username();
	}

	virtual void on_set_user_name_succeeded() override
	{
		read_message();
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
		this->reconnect_in(boost::posix_time::seconds(1));
	}

	virtual void on_connected() override
	{
		std::clog << "!! connected" << std::endl;
		read_username();
	}

	virtual void on_disconnected() override
	{
		std::clog << "!! disconnected" << std::endl;
	}

  private:
	boost::asio::posix::stream_descriptor stdin_;
	boost::asio::streambuf input_buffer_;

	// read the username from stdin and send it to the server
	void read_username()
	{
		std::cout << "enter user name: " << std::endl;
		// Read a line of input entered by the user.
		boost::asio::async_read_until(stdin_, input_buffer_, '\n',
			[this](const boost::system::error_code& ec, std::size_t length)
			{
				if (!ec && length > 1)
				{
					std::string line;
					std::istream is(&input_buffer_);
					std::getline(is, line);

					if (!line.empty())
						client_.set_user_name(line);
					else
						read_username();
				}
			}
		);
	}

	void read_message()
	{
		std::cout << "broadcast message: " << std::endl;
		// Read a line of input entered by the user.
		boost::asio::async_read_until(stdin_, input_buffer_, '\n',
			[this](const boost::system::error_code& ec, std::size_t)
			{
				if (!ec)
				{
					std::string line;
					std::istream is(&input_buffer_);
					std::getline(is, line);

					if (!line.empty())
						client_.broadcast_message(line);

					// start over
					read_message();
				}
			}
		);
	}
};

/* this is the blocking version of the event loop
*
* this unfortunately causes high cpu load because of
* a (suspected) bug in boost::asio::posix::stream_descriptor
* when used with kevent.
* */
void the_blocking_loop(boost::asio::io_service& io_service, chat_client_handler& handler)
{
	while (true)
	{
		boost::system::error_code ec;
		while (io_service.run_one(ec)) // run_one blocks until there is new data
		{
			if (ec) handler.on_error(ec);
			else handler.update();
		}
	}
}

void the_non_blocking_loop(boost::asio::io_service& io_service, chat_client_handler& handler)
{
	while (true)
	{
		boost::system::error_code ec;
		while (io_service.poll_one(ec))
		{
			if (ec) handler.on_error(ec);
			else handler.update();
		}

		// do some other work. e.g. sleep
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}


int main(int argc, char* argv[])
{
	(void) argc;
	(void) argv;

	std::string host_name = "127.0.0.1";
	std::string service_name = "1528";

	boost::asio::io_service io_service;
	boost::asio::io_service::work work(io_service);

	chat_client_handler handler(io_service, host_name, service_name);

	the_non_blocking_loop(io_service, handler);

	//the_blocking_loop(io_service, handler);

	return 0;
}
