//
// Created by Benjamin Schulz on 15/03/15.
//

#ifndef _THRIFT_ASIO_SERVER_HPP_
#define _THRIFT_ASIO_SERVER_HPP_

#pragma once

#include <boost/smart_ptr.hpp>
#include <boost/asio.hpp>
#include <thrift/TProcessor.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <iostream>
#include "./thrift_asio_transport.hpp"

namespace betabugs{
namespace networking{

/*!
* Use this class on the server
* */
template <typename HandlerType>
class thrift_asio_server
{
	typedef boost::shared_ptr<HandlerType> Handler_ptr;

	// forward typedefs to minimize pollution
	typedef apache::thrift::TProcessor TProcessor;
	typedef apache::thrift::transport::TMemoryBuffer TMemoryBuffer;
	typedef apache::thrift::protocol::TBinaryProtocol TBinaryProtocol;

  public:
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
	* */
	static void serve(
		boost::asio::io_service& io_service,
		TProcessor& processor,
		Handler_ptr handler,
		short port
	)
	{
		using boost::asio::ip::tcp;

		auto acceptor = std::make_shared<tcp::acceptor>(
			io_service,
			boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port),
			true
		);

		start_accept(io_service, acceptor, processor, handler);
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
				}

				// Note: this will accept new connections without any bounds
				start_accept(io_service, acceptor, processor, handler);
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

		auto transport = std::make_shared<thrift_asio_transport>(socket, handler.get());
		handler->on_client_connected(transport);

		read_frame_size(io_service, socket, processor, handler);
	}

	// read the size of a frame. Clients are expected to use the framed protocol
	static void read_frame_size(
		boost::asio::io_service& io_service,
		std::shared_ptr<boost::asio::ip::tcp::socket> socket,
		TProcessor& processor,
		Handler_ptr handler
	)
	{
		auto frame_size = std::make_shared<uint32_t>(0);
		boost::asio::async_read(
			*socket, boost::asio::buffer(frame_size.get(), sizeof(uint32_t)),
			[&io_service, socket, &processor, handler, frame_size]
				(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
			{
				if(ec)
				{
					std::clog << ec.message() << std::endl;
				}
				else
				{
					*frame_size = ntohl(*frame_size);
					std::clog << "frame_size=" << *frame_size << std::endl;

					read_frame_data(io_service, socket, processor, handler, *frame_size);
				}
			}
		);
	}

	// read the data of the frame
	static void read_frame_data(
		boost::asio::io_service& io_service,
		std::shared_ptr<boost::asio::ip::tcp::socket> socket,
		TProcessor& processor,
		Handler_ptr handler,
		uint32_t frame_size
	)
	{
		auto frame_bytes = std::make_shared<std::vector<uint8_t>>(frame_size);
		boost::asio::async_read(
			*socket, boost::asio::buffer(frame_bytes->data(), frame_size),
			[&io_service, socket, &processor, handler, frame_bytes]
				(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
			{
				if(ec)
				{
					std::clog << ec.message() << std::endl;
				}
				else
				{
					std::clog << "got frame bytes: " << frame_bytes->size() << std::endl;

					auto input_transport = boost::make_shared<TMemoryBuffer>(frame_bytes->data(), frame_bytes->size());
					auto input_protocol = boost::make_shared<TBinaryProtocol>(input_transport);

					auto output_transport = boost::make_shared<TMemoryBuffer>();
					auto output_protocol = boost::make_shared<TBinaryProtocol>(output_transport);

					void* connection_context = nullptr;

					handler->before_process(output_protocol);
					processor.process(input_protocol, output_protocol, connection_context);
					handler->after_process();

					std::clog << "response bytes: " << output_transport->available_read() << std::endl;

					// is there a response to write back to the client?
					if( output_transport->available_read() )
					{
						write_frame_size(
							io_service,
							socket,
							processor,
							handler,
							std::make_shared<std::string>(output_transport->getBufferAsString())
						);
					}
					else // nope, start reading the next frame
					{
						read_frame_size(io_service, socket, processor, handler);
					}
				}
			}
		);
	}

	// write the number of bytes in the response frame to follow
	static void write_frame_size(
		boost::asio::io_service& io_service,
		std::shared_ptr<boost::asio::ip::tcp::socket> socket,
		TProcessor& processor,
		Handler_ptr handler,
		std::shared_ptr<std::string> response
	)
	{
		auto frame_size = std::make_shared<uint32_t>(htonl(response->size()));

		boost::asio::async_write(
			*socket,
			boost::asio::buffer( frame_size.get(), sizeof(uint32_t) ),
			[&io_service, socket, &processor, handler, response, frame_size]
				(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
			{
				if(ec)
				{
					std::clog << "write_frame_size: " << ec.message() << std::endl;
				}
				else
				{
					write_frame_data(io_service, socket, processor, handler, response);
				}
			}
		);
	}

	// write the gist of the frame
	static void write_frame_data(
		boost::asio::io_service& io_service,
		std::shared_ptr<boost::asio::ip::tcp::socket> socket,
		TProcessor& processor,
		Handler_ptr handler,
		std::shared_ptr<std::string> response
	)
	{
		boost::asio::async_write(
			*socket,
			boost::asio::buffer( response->data(), response->size() ),
			[&io_service, socket, &processor, handler, response]
				(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
			{
				if(ec)
				{
					std::clog << "write_frame_data: " << ec.message() << std::endl;
				}
				else
				{
					read_frame_size(io_service, socket, processor, handler);
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
