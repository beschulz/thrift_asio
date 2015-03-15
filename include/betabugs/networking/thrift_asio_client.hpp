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

namespace betabugs {
namespace networking {

/*!
* Use this class on the client
* */
template<
	typename ClientType,
	typename ProcessorType,
	typename HandlerInterfaceType
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
		, protocol_
			(
				boost::make_shared<apache::thrift::protocol::TBinaryProtocol>(
					boost::make_shared<apache::thrift::transport::TFramedTransport>(
						transport_
					)
				)
			)
		, client_(protocol_)
	{
		protocol_->getTransport()->open();
	}

	/// process incoming traffic
	void update()
	{
		while (transport_->isOpen() && transport_->peek())
		{
			processor_.process(protocol_, nullptr, nullptr);
		}
	}

	/// close the connection and connect to host_name:service_name
	void connect_to(const std::string& host_name, const std::string service_name)
	{
		transport_->connect_to(host_name, service_name);
	}

	/// reconnect in seconds seconds
	void reconnect_in(float seconds)
	{
		auto timer = std::make_shared<boost::asio::deadline_timer>(io_service_);
		timer->expires_from_now(boost::posix_time::seconds(seconds));
		timer->async_wait(
			[timer, this](const boost::system::error_code& ec)
			{
				if (ec) on_error(ec);
				else transport_->open();
			}
		);
	}

  private:
	boost::asio::io_service& io_service_;
	ProcessorType processor_;

	boost::shared_ptr <thrift_asio_client_transport> transport_;
	boost::shared_ptr <apache::thrift::protocol::TProtocol> protocol_;
  protected:
	ClientType client_; ///< the client used to communicate with the server
};

/** \example example_client.cpp
 * This is an example of how to use thrift_asio_client.
 */

}
}

#endif //_THRIFT_ASIO_CLIENT_HPP_
