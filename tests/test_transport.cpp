//
// Created by Benjamin Schulz on 16/03/15.
//

#ifndef BOOST_TEST_MODULE
#	define BOOST_TEST_MODULE test_transport
#	define BOOST_TEST_DYN_LINK

#	include <boost/test/unit_test.hpp>

#endif /* BOOST_TEST_MODULE */

#include <iostream>
#include <thrift/protocol/TBinaryProtocol.h>

#include <returning_service.h>
#include <betabugs/networking/thrift_asio_transport.hpp>
#include <betabugs/networking/thrift_asio_server.hpp>
#include <betabugs/networking/thrift_asio_client_transport.hpp>
#include <faac.h>
#include <thread>
#include <future>

class returning_service_handler : public test::returning_serviceIf
								  , public betabugs::networking::thrift_asio_transport::event_handlers
{
  public:
	virtual int32_t add(const int32_t a, const int32_t b) override
	{
		return a + b;
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
	}

	void on_client_disconnected(const boost::shared_ptr<apache::thrift::protocol::TProtocol>& output_protocol, const boost::system::error_code& ec)
	{
		(void) output_protocol;
		std::clog << "client disconnected, reason: " << ec.message() << std::endl;
	}

	void before_process(boost::shared_ptr<apache::thrift::protocol::TBinaryProtocol> output_protocol)
	{
		(void) output_protocol;
	}

	void after_process()
	{
	}
};

struct test_event_handlers : public betabugs::networking::thrift_asio_transport::event_handlers
{

	virtual void on_error(const boost::system::error_code& ec) override
	{
		std::clog << "client error: " << ec.message() << std::endl;
	}

	virtual void on_connected() override
	{
		std::clog << "client connected: " << std::endl;
	}

	virtual void on_disconnected() override
	{
		std::clog << "client disconnected: " << std::endl;
	}
};

BOOST_AUTO_TEST_SUITE(test_transport)

BOOST_AUTO_TEST_CASE(test_transport)
{
	auto handler = boost::make_shared<returning_service_handler>();
	auto processor = test::returning_serviceProcessor
		(
			handler
		);

	betabugs::networking::thrift_asio_server<returning_service_handler> server;

	boost::asio::io_service io_service;
	boost::asio::io_service::work work(io_service);

	const unsigned short port = 1338;

	server.serve(io_service, processor, handler, port);

	auto client_thread = std::async([&]()
	{
		boost::asio::io_service io_service;
		boost::asio::io_service::work work(io_service);

		// create the client
		test_event_handlers event_handlers;
		auto t1 = boost::make_shared<betabugs::networking::thrift_asio_client_transport>(
			io_service,
			"127.0.0.1",
			std::to_string(port),
			&event_handlers
		);
		auto t2 = boost::make_shared<apache::thrift::transport::TFramedTransport>(t1);
		auto client_protocol = boost::make_shared<apache::thrift::protocol::TBinaryProtocol>(t2);

		test::returning_serviceClient client(client_protocol);

		t2->open();

		while (true)
		{
			while (io_service.poll_one());
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			if (t2->isOpen())
			{
				int32_t result = client.add(20, 22);
				std::clog << "got result: " << result << std::endl;
				BOOST_CHECK_EQUAL(result, 20 + 22);
				return;
			}
		}
	});

	int num_iterations = 5000/100;
	while (num_iterations--)
	{
		auto status = client_thread.wait_for(std::chrono::milliseconds(100));

		if(status == std::future_status::ready)
			break;
		else
			io_service.poll_one();
	}
	BOOST_CHECK_GT(num_iterations, 0);
}

BOOST_AUTO_TEST_SUITE_END()
