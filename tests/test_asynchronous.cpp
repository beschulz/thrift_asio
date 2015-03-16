//
// Created by Benjamin Schulz on 16/03/15.
//

#ifndef BOOST_TEST_MODULE
#	define BOOST_TEST_MODULE test_asynchrounous
#	define BOOST_TEST_DYN_LINK

#	include <boost/test/unit_test.hpp>

#endif /* BOOST_TEST_MODULE */

#include <iostream>
#include <thrift/protocol/TBinaryProtocol.h>

#include <asynchronous_server.h>
#include <asynchronous_client.h>
#include <betabugs/networking/thrift_asio_transport.hpp>
#include <betabugs/networking/thrift_asio_server.hpp>
#include <betabugs/networking/thrift_asio_client_transport.hpp>
#include <betabugs/networking/thrift_asio_client.hpp>
#include <thread>
#include <future>


class asynchronous_server_handler : public test::asynchronous_serverIf
									, public betabugs::networking::thrift_asio_transport::event_handlers
{
  public:
	virtual void add(const int32_t a, const int32_t b) override
	{
		assert(current_client_);
		current_client_->on_added(a + b);
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
	void on_client_connected(boost::shared_ptr<apache::thrift::protocol::TProtocol> output_protocol)
	{
		(void) output_protocol;
		std::clog << "server: client connected" << std::endl;
		clients_.insert(
			std::make_pair(
				output_protocol,
				std::make_shared<test::asynchronous_clientClient>(output_protocol)
			)
		);
	}

	void on_client_disconnected(const boost::shared_ptr<apache::thrift::protocol::TProtocol>& output_protocol, const boost::system::error_code& ec)
	{
		std::clog << "client disconnected, reason: " << ec.message() << std::endl;
		clients_.erase(output_protocol);
	}

	void before_process(boost::shared_ptr<apache::thrift::protocol::TProtocol> output_protocol)
	{
		auto pos = clients_.find(output_protocol);
		assert(pos != clients_.end());
		current_client_ = pos->second;
	}

	void after_process()
	{
		current_client_.reset();
	}

  private:
	typedef boost::shared_ptr<apache::thrift::protocol::TProtocol> protocol_ptr;
	typedef std::shared_ptr<test::asynchronous_clientClient> client_ptr;
	typedef std::map<protocol_ptr, client_ptr> Clients;
	Clients clients_;

	client_ptr current_client_;
};


class asynchronous_client_handler : public betabugs::networking::thrift_asio_client<
	test::asynchronous_serverClient,
	test::asynchronous_clientProcessor,
	test::asynchronous_clientIf
>
{
  public:

	/// use constructor of base class
	using betabugs::networking::thrift_asio_client<
		test::asynchronous_serverClient,
		test::asynchronous_clientProcessor,
		test::asynchronous_clientIf
	>::thrift_asio_client;

	/// implementation of test::asynchronous_clientIf
	int32_t last_result = 0;
	virtual void on_added(const int32_t result) override
	{
		last_result = result;
	}

	virtual void on_connected() override
	{
		client_.add(20, 22);
	}
};


BOOST_AUTO_TEST_SUITE(test_asynchrounous)

BOOST_AUTO_TEST_CASE(test_asynchrounous_basic)
{
	const unsigned short port = 1338;


	// create the server
	auto handler = boost::make_shared<asynchronous_server_handler>();
	auto processor = test::asynchronous_serverProcessor{handler};

	betabugs::networking::thrift_asio_server<asynchronous_server_handler> server;

	boost::asio::io_service io_service;
	boost::asio::io_service::work work(io_service);

	server.serve(io_service, processor, handler, port);


	// create the client
	asynchronous_client_handler client_handler(io_service, "127.0.0.1", std::to_string(port));


	int num_iterations = 5000 / 100;
	while (num_iterations--)
	{
		while (io_service.poll_one())
		{
			client_handler.update();
		}

		if(client_handler.last_result != 0)
		{
			BOOST_CHECK_EQUAL(client_handler.last_result, 42);
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	BOOST_CHECK_GT(num_iterations, 0);
}

BOOST_AUTO_TEST_SUITE_END()
