//
// Created by Benjamin Schulz on 15/03/15.
//

#ifndef _THRIFT_ASIO_SERVER_HPP_
#define _THRIFT_ASIO_SERVER_HPP_

#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <thrift/TProcessor.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TZlibTransport.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <iostream>
#include "./thrift_asio_transport.hpp"

namespace betabugs{
namespace networking{

/*!
* Use this class on the server
*
* \tparam HandlerType the type of the implementation of a handler.
* \tparam use_compression whether to wrap the transport into a TZlibTransport or not
*
* \section HandlerType HandlerType
*   First of all the HandlerType must work with the auto-generated processor you're using.
*   it must also be inherited from thrift_asio_transport::event_handlers to be able to receive
*   transport errors.
*   lastly it must implement the following member functions:
*
*   @code
*   void on_client_connected(boost::shared_ptr<apache::thrift::protocol::TProtocol> output_protocol);
*   void on_client_disconnected(const boost::shared_ptr<apache::thrift::protocol::TProtocol>& output_protocol, const boost::system::error_code& ec);
*   void before_process(boost::shared_ptr<apache::thrift::protocol::TProtocol> output_protocol);
*   void after_process();
*   @endcode
*
*   or you can simply inherit your server side handler from thrift_asio_connection_management_mixin
* */
template <typename HandlerType, bool use_compression=false>
class thrift_asio_server
{
	typedef boost::shared_ptr<HandlerType> Handler_ptr;

	// forward typedefs to minimize pollution
	typedef apache::thrift::TProcessor TProcessor;
	typedef apache::thrift::transport::TMemoryBuffer TMemoryBuffer;
	typedef apache::thrift::transport::TZlibTransport TZlibTransport;
	typedef apache::thrift::transport::TFramedTransport TFramedTransport;
	typedef apache::thrift::protocol::TBinaryProtocol TBinaryProtocol;

  public:
	typedef std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_ptr;

	/*!
	* call this to start listening for incoming connections.
	* This call is non blocking. To actually service the clients,
	* make sure, that you run/poll the io_service, i.e.
	*
	* @code
	* // in a "realtime" context (game/audio-processor/etc.)
	* void call_me_once_per_frame()
	* {
	*   // drain outstanding handlers
	*   while (io_service.poll_one());
	* }
	* @endcode
	*
	* The nice thing about this is, that you RCP-calls will be made from
	* the thread that is calling call_me_once_per_frame(). This means, you'll
	* need no (or a lot less) locking. This is much more suitable for a "realtime"
	* environment.
	*
	* @returns acceptor_ptr, so that you can stop listening
	*
	* */
	static acceptor_ptr serve(
		boost::asio::io_service& io_service,
		TProcessor& processor,
		Handler_ptr handler,
		unsigned short port
	)
	{
		using boost::asio::ip::tcp;

		auto acceptor = std::make_shared<tcp::acceptor>(
			io_service,
			boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port),
			true
		);

		start_accept(io_service, acceptor, processor, handler);
		return acceptor;
	}

  private:
	static void start_accept(
		boost::asio::io_service& io_service,
		std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor,
		TProcessor& processor,
		Handler_ptr handler
	)
	{
		using boost::asio::ip::tcp;

		auto socket = std::make_shared<tcp::socket>(io_service);
		acceptor->async_accept(
			*socket,
			[&io_service, acceptor, socket, &processor, handler]
				(boost::system::error_code ec)
			{
				if (ec)
				{
					std::clog << ec.message() << std::endl;
				}
				else
				{
					std::clog << "client connected" << std::endl;
					on_accept(io_service, socket, processor, handler);

					// Note: this will accept new connections without any bounds
					start_accept(io_service, acceptor, processor, handler);
				}
			}
		);
	}

	// called when a new client connection was established (accepted)
	static void on_accept(
		boost::asio::io_service& io_service,
		std::shared_ptr<boost::asio::ip::tcp::socket> socket,
		TProcessor& processor,
		Handler_ptr handler
	)
	{
		using boost::make_shared;

		// construct the output_protocol and call the handler
		auto t1 = boost::make_shared<thrift_asio_transport>(socket, handler.get());
		boost::shared_ptr<apache::thrift::transport::TTransport> t2
			= boost::make_shared<TFramedTransport>(t1);
		if (use_compression)
			t2 = boost::make_shared<TZlibTransport>(t2, 128, 1024, 128, 1024, 9);
		auto output_protocol = boost::make_shared<TBinaryProtocol>(t2);
		handler->on_client_connected(output_protocol);

		read_frame_size(io_service, socket, output_protocol, processor, handler);
	}

	// read the size of a frame. Clients are expected to use the framed protocol
	static void read_frame_size(
		boost::asio::io_service& io_service,
		std::shared_ptr<boost::asio::ip::tcp::socket> socket,
		boost::shared_ptr<TBinaryProtocol> output_protocol,
		TProcessor& processor,
		Handler_ptr handler
	)
	{
		auto frame_size = std::make_shared<uint32_t>(0);
		boost::asio::async_read(
			*socket, boost::asio::buffer(frame_size.get(), sizeof(uint32_t)),
			[&io_service, socket, output_protocol, &processor, handler, frame_size]
				(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
			{
				if(ec)
				{
					std::clog << ec.message() << std::endl;
					handler->on_client_disconnected(output_protocol, ec);
				}
				else
				{
					*frame_size = ntohl(*frame_size);
					read_frame_data(io_service, socket, output_protocol, processor, handler, *frame_size);
				}
			}
		);
	}

	// read the data of the frame
	static void read_frame_data(
		boost::asio::io_service& io_service,
		std::shared_ptr<boost::asio::ip::tcp::socket> socket,
		boost::shared_ptr<TBinaryProtocol> output_protocol,
		TProcessor& processor,
		Handler_ptr handler,
		uint32_t frame_size
	)
	{
		auto frame_bytes = std::make_shared<std::vector<uint8_t>>(frame_size);
		boost::asio::async_read(
			*socket, boost::asio::buffer(frame_bytes->data(), frame_size),
			[&io_service, socket, output_protocol, &processor, handler, frame_bytes]
				(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
			{
				if(ec)
				{
					std::clog << ec.message() << std::endl;
					handler->on_client_disconnected(output_protocol, ec);
				}
				else
				{
					boost::shared_ptr<apache::thrift::transport::TTransport> input_transport
						= boost::make_shared<TMemoryBuffer>(frame_bytes->data(), frame_bytes->size());
					//if(use_compression)
					//	input_transport = boost::make_shared<TZlibTransport>(input_transport);
					auto input_protocol = boost::make_shared<TBinaryProtocol>(input_transport);

					void* connection_context = nullptr;

					handler->before_process(output_protocol);
					processor.process(input_protocol, output_protocol, connection_context);
					handler->after_process();

					// read the next frame
					read_frame_size(io_service, socket, output_protocol, processor, handler);
				}
			}
		);
	}
};

/** \example example_server.cpp
 * This is an example of how to use thrift_asio_server.
 */

}
}


#endif //_THRIFT_ASIO_SERVER_HPP_
