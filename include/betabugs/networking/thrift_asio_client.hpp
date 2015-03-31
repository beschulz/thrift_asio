//
// Created by Benjamin Schulz on 15/03/15.
//

#ifndef _THRIFT_ASIO_CLIENT_HPP_
#define _THRIFT_ASIO_CLIENT_HPP_

#pragma once

#include <boost/smart_ptr/enable_shared_from_raw.hpp>
#include "./thrift_asio_client_transport.hpp"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TZlibTransport.h>

namespace betabugs {
namespace networking {

/// An asynchronous bidirectional thrift client
/*!
* Use this class as the base class for the implementation of your client side handler.
*
* @tparam ClientType type of the auto-generated client. i.e. MyAwesomeServerClient
* @tparam ProcessorType an auto-generated TProcessor, that works with HandlerInterfaceType. i.e. MyAwesomeClientProcessor
* @tparam HandlerInterfaceType auto-generated interface of the handler you're implementing. i.e. MyAwesomeClientIf
* @tparam use_compression whether to wrap the transport into a TZlibTransport or not
* */
template<
	typename ClientType,
	typename ProcessorType,
	typename HandlerInterfaceType,
	bool use_compression=false
>
class thrift_asio_client
	: public HandlerInterfaceType
	  , public boost::enable_shared_from_raw
	  , public thrift_asio_client_transport::event_handlers
{
  public:
	/// creates a thrift_asio_client and tries to connect to host_name:service_name
	thrift_asio_client(
		boost::asio::io_service& io_service,
		const std::string& host_name,
		const std::string& service_name
	)
		: io_service_(io_service)
		, processor_(boost::shared_from_raw(this))
		, transport_(boost::make_shared<thrift_asio_client_transport>(
			io_service, host_name, service_name, this
		))
		, input_protocol_ (make_input_protocol(transport_))
		, output_protocol_(make_output_protocol(transport_))
		, client_(input_protocol_, output_protocol_)
	{
		input_protocol_->getTransport()->open();
	}

	/// process incoming traffic
	void update()
	{
		while (transport_->isOpen() && transport_->peek())
		{
			processor_.process(input_protocol_, nullptr, nullptr);
		}
	}

	/// process at most one incoming RPC call
	void update_one()
	{
		if (transport_->isOpen() && transport_->peek())
		{
			processor_.process(input_protocol_, nullptr, nullptr);
		}
	}

	/// close the connection and connect to host_name:service_name
	void connect_to(const std::string& host_name, const std::string service_name)
	{
		input_protocol_  = make_input_protocol(transport_);
		output_protocol_ = make_output_protocol(transport_);
		client_ = ClientType(input_protocol_, output_protocol_);

		transport_->connect_to(host_name, service_name);
	}

	/// reconnect in seconds seconds
	void reconnect_in(const boost::posix_time::time_duration& duration)
	{
		input_protocol_  = make_input_protocol(transport_);
		output_protocol_ = make_output_protocol(transport_);
		client_ = ClientType(input_protocol_, output_protocol_);

		reconnect_timer = std::make_shared<boost::asio::deadline_timer>(io_service_);
		reconnect_timer->expires_from_now(duration);
		reconnect_timer->async_wait(
			[this](const boost::system::error_code& ec)
			{
				if (ec) on_error(ec);
				else transport_->open();
			}
		);
	}

  private:
	boost::asio::io_service& io_service_;
	ProcessorType processor_;

	boost::shared_ptr<thrift_asio_client_transport> transport_;
	boost::shared_ptr<apache::thrift::protocol::TProtocol> input_protocol_;
	boost::shared_ptr<apache::thrift::protocol::TProtocol> output_protocol_;

	std::shared_ptr<boost::asio::deadline_timer> reconnect_timer;

	static boost::shared_ptr<apache::thrift::protocol::TProtocol>
	make_input_protocol(boost::shared_ptr<thrift_asio_client_transport> transport)
	{
		boost::shared_ptr<apache::thrift::transport::TTransport> t2
			= boost::make_shared<apache::thrift::transport::TFramedTransport>(transport);
		if (use_compression)
			t2 = boost::make_shared<apache::thrift::transport::TZlibTransport>(t2);
		return boost::make_shared<apache::thrift::protocol::TBinaryProtocol>(t2);
	}

	static boost::shared_ptr<apache::thrift::protocol::TProtocol>
	make_output_protocol(boost::shared_ptr<thrift_asio_client_transport> transport)
	{
		boost::shared_ptr<apache::thrift::transport::TTransport> t2
			= boost::make_shared<apache::thrift::transport::TFramedTransport>(transport);
		return boost::make_shared<apache::thrift::protocol::TBinaryProtocol>(t2);
	}

  protected:
	ClientType client_; ///< the client used to communicate with the server
};

/** \example example_client.cpp
 * This is an example of how to use thrift_asio_client.
 */

}
}

#endif //_THRIFT_ASIO_CLIENT_HPP_
