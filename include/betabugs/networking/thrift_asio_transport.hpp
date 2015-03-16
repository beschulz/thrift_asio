//
// Created by Benjamin Schulz on 15/03/15.
//

#ifndef _THRIFT_ASIO_TRANSPORT_HPP_
#define _THRIFT_ASIO_TRANSPORT_HPP_

#pragma once

#include <thrift/transport/TVirtualTransport.h>
//#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/make_shared.hpp>

namespace betabugs {
namespace networking {

/*!
* thrift transport that employs a boost::asio::socket.
*
* If you want name resolution, it might be easier to use thrift_asio_client_transport
*
* */
class thrift_asio_transport : public apache::thrift::transport::TVirtualTransport<thrift_asio_transport>
{
  public:
	/*!
	* Interface for handling transport events
	* */
	struct event_handlers
	{
		/// Gets invoked when an error occurred while communication over the transport.
		virtual void on_error(const boost::system::error_code& ec)
		{
			(void) ec;
		}

		/// Gets invoked when the transport was successfully connected.
		virtual void on_connected()
		{
		}

		/// Gets invoked when the transport is disconnected.
		virtual void on_disconnected()
		{
		}
	};

	/// a shared_ptr to a tcp socket
	typedef std::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;

	/// creates a thrift_asio_transport from a socket_ptr
	thrift_asio_transport(socket_ptr socket, event_handlers* event_handlers)
		: socket_(socket)
		, event_handlers_(event_handlers)
	{
		assert(event_handlers);
	};


	/// Attempt to read up to the specified number of bytes into the string.
	/*!
	*
	* This does not block.
	*
	* @param buf  Reference to the location to write the data
	* @param len  How many bytes to read
	* @return How many bytes were actually read
	*/
	uint32_t read(uint8_t* buf, uint32_t len)
	{
		auto bytes_to_copy = std::min<size_t>(len, incomming_bytes_.size());

		std::copy(
			incomming_bytes_.begin(),
			incomming_bytes_.begin() + bytes_to_copy,
			buf
		);

		incomming_bytes_.erase(
			incomming_bytes_.begin(),
			incomming_bytes_.begin() + bytes_to_copy
		);

		return bytes_to_copy;
	}

	/// the number of bytes, that have been received on not yet read()
	size_t available_bytes() const
	{
		return incomming_bytes_.size();
	}

	/*uint32_t readAll(uint8_t* buf, uint32_t len)
	{
		return read(buf, len);
	}*/

	/**
	* asynchronously sends len bytes from buf.
	*
	* In case of error, the event_handler::on_error will be invoked.
	*
	* @param buf  The data to write out
	* @param len  number of bytes to read from buf
	*/
	void write(const uint8_t* buf, uint32_t len)
	{
		auto holder = boost::make_shared<std::string>(buf, buf + len);

		boost::asio::async_write(
			*socket_,
			boost::asio::buffer(holder->data(), holder->size()),
			[this, holder](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
			{
				if (ec)
				{
					event_handlers_->on_error(ec);
					this->close();
				}
			}
		);
	}

	/*const uint8_t* borrow(uint8_t* buf, uint32_t* len)
	{
		return nullptr;
	}

	void consume(uint32_t len)
	{
	}*/

	/// return true unless an error occured or the transport was closed
	virtual bool isOpen() override
	{
		return state_ == OPEN;
	}

	/// Checks wether this transport is closed.
	/*!
	* @returns true, if the transport is closed,
	* false if the connection is open
	* or name resolution is in progress.
	*/
	bool isClosed()
	{
		return state_ == CLOSED;
	}

	/// return true, if there is data available to be processed
	virtual bool peek() override
	{
		return isOpen() && !incomming_bytes_.empty();
	}

	/// opens the transport
	virtual void open() override
	{
		event_handlers_->on_connected();
		state_ = OPEN;

		socket_->set_option(boost::asio::ip::tcp::no_delay(true));
		auto receive_buffer = std::make_shared<std::array<char, 1024>>();

		socket_->async_receive(
			boost::asio::buffer(*receive_buffer, 1024),
			0,
			[this, receive_buffer]
				(const boost::system::error_code& ec, std::size_t bytes_transferred)
			{
				this->on_receive(ec, receive_buffer, bytes_transferred);
			}
		);
	}

	/// closes the transport
	virtual void close() override
	{
		if (state_ == OPEN)
		{
			boost::system::error_code ec;
			socket_->cancel(ec);
			if (ec) event_handlers_->on_error(ec);
			socket_->close(ec);
			if (ec) event_handlers_->on_error(ec);
		}
		event_handlers_->on_disconnected();
		state_ = CLOSED;
		incomming_bytes_.clear();
	}


	/**
	* Returns the origin of the transports call. The value depends on the
	* transport used. An IP based transport for example will return the
	* IP address of the client making the request.
	* If the transport doesn't know the origin Unknown is returned.
	*
	* The returned value can be used in a log message for example
	*/
	virtual const std::string getOrigin() override
	{
		return socket_->remote_endpoint().address().to_string() + ":" + std::to_string(socket_->remote_endpoint().port());
	}

  protected:
	/// enum to represent the state-machine of the connection
	enum State
	{
		CLOSED,      ///< the transport is closed
		CONNECTING,  ///< the transport is currently connecting
		RESOLVING,   ///< we're currently trying to resolve host_name:service_name
		OPEN         ///< the transport is open and ready for communication
	};

	State state_ = CLOSED; ///< the state of this transport
	socket_ptr socket_; ///< the underlying socket
	event_handlers* event_handlers_; ///< handles events like on_error, etc.

  private:
	std::string incomming_bytes_;

	void on_receive(
		const boost::system::error_code& ec,
		std::shared_ptr<std::array<char, 1024>> receive_buffer,
		std::size_t bytes_transferred)
	{
		if (ec)
		{
			event_handlers_->on_error(ec);
			this->close();
		}
		else
		{
			incomming_bytes_ += std::string(
				begin(*receive_buffer),
				begin(*receive_buffer) + bytes_transferred);

			socket_->async_receive(
				boost::asio::buffer(*receive_buffer, sizeof(receive_buffer)),
				0,
				[this, receive_buffer](const boost::system::error_code& ec,
					std::size_t bytes_transferred)
				{
					this->on_receive(ec, receive_buffer, bytes_transferred);
				}
			);
		}
	}
};

}
}

#endif //_THRIFT_ASIO_TRANSPORT_HPP_
