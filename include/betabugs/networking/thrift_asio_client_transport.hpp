//
// Created by Benjamin Schulz on 15/03/15.
//

#ifndef _THRIFT_ASIO_THRIFT_ASIO_CLIENT_TRANSPORT_HPP_
#define _THRIFT_ASIO_THRIFT_ASIO_CLIENT_TRANSPORT_HPP_

#pragma once

#include "./thrift_asio_transport.hpp"
#include <boost/asio/connect.hpp>

namespace betabugs {
namespace networking {

/*!
* In contrast to thrift_asio_transport, this class does name resolution and
* connects to the endpoint.
* */
class thrift_asio_client_transport : public thrift_asio_transport
{
  public:
	/// creates a thrift_asio_client_transport and tries to connect to host_name:service_name
	thrift_asio_client_transport(
		boost::asio::io_service& io_service, ///< io_service to use
		const std::string& host_name,        ///< name of the host to connect to
		const std::string& service_name,     ///< i.e. port
		event_handlers* event_handlers       ///< the event handlers to use
	) : thrift_asio_transport(std::make_shared<boost::asio::ip::tcp::socket>(io_service), event_handlers)
		, host_name_(host_name)
		, service_name_(service_name)
		, resolver_(io_service)
	{
	}

	/**
	* tries to resolve host_name:server_name and opens the transport for communication.
	*
	* In contrast to apache::thrift::TTransport, this does not block and the isOpen method
	* will return false until the connection is established.
	*/
	virtual void open() override
	{
		using boost::asio::ip::tcp;
		resolver_.cancel();
		resolver_.async_resolve
			(
				{host_name_, service_name_},
			[this]
				(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator iterator)
			{
				if (ec)
				{
					event_handlers_->on_error(ec);
					close();
				}
				else
				{
					boost::asio::async_connect
						(
							*socket_,
							iterator,
							[this]
								(boost::system::error_code ec, tcp::resolver::iterator)
							{
								if (ec)
								{
									event_handlers_->on_error(ec);
									this->close();
								}
								else
								{
									thrift_asio_transport::open();
								}
							}
						);
				}
			}
		);
	}


	/// close the current connection and connect to host_name::service_name
	void connect_to(const std::string& host_name, const std::string& service_name)
	{
		close();
		this->host_name_ = host_name;
		this->service_name_ = service_name;
		open();
	}

  private:
	std::string host_name_;
	std::string service_name_;
	boost::asio::ip::tcp::resolver resolver_;
};

}
}

#endif //_THRIFT_ASIO_THRIFT_ASIO_CLIENT_TRANSPORT_HPP_
