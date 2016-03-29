//
// Created by Benjamin Schulz on 15/03/15.
//

#ifndef _THRIFT_ASIO_TRANSPORT_HPP_
#define _THRIFT_ASIO_TRANSPORT_HPP_

#pragma once

#include <thrift/transport/TVirtualTransport.h>
//#include <boost/asio.hpp>
#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/make_shared.hpp>
#include <deque>

namespace betabugs {
namespace networking {

/*!
* thrift transport that employs a boost::asio::socket.
*
* If you want name resolution, it might be easier to use thrift_asio_client_transport
*
* */
class thrift_asio_transport
    : public apache::thrift::transport::TVirtualTransport<thrift_asio_transport>
    , public boost::enable_shared_from_this<thrift_asio_transport>
{
	static constexpr size_t BUFFER_SIZE = 1024;

  public:
	/*!
	* Interface for handling transport events
	* */
	struct event_handlers
	{
        virtual ~event_handlers(){}

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
    typedef std::weak_ptr<boost::asio::ip::tcp::socket> socket_weak_ptr;

    /// creates a thrift_asio_transport from a socket_ptr
	thrift_asio_transport(socket_ptr socket, event_handlers* event_handlers)
		: socket_(socket)
		, event_handlers_(event_handlers)
	{
		assert(event_handlers);
	};

    virtual ~thrift_asio_transport()
    {
    }


	/// Attempt to read up to the specified number of bytes into the string.
	/*!
	*
	* This does not block if data is available. But it does block,
	* if there is not enough data. If you don't want to block, use
	* available_bytes() to check if there's enough data.
	*
	* @param buf  Reference to the location to write the data
	* @param len  How many bytes to read
	* @return How many bytes were actually read
	*/
	uint32_t read(uint8_t* buf, uint32_t len)
	{
		while (available_bytes() < len)
		{
			socket_->get_io_service().run_one();
		}

		auto bytes_to_copy = std::min<size_t>(len, incomming_bytes_.size());

		std::copy_n(incomming_bytes_.begin(), bytes_to_copy, buf);

		incomming_bytes_.erase(
			incomming_bytes_.begin(),
			incomming_bytes_.begin() + std::string::difference_type(bytes_to_copy)
		);

		return uint32_t(bytes_to_copy);
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
		outbound_messages_.push_back({buf, buf+len});
		if (outbound_messages_.size() == 1)
		{
			async_write_one();
		}// the other case is handled in the completion handler in async_write_one
	}

	void async_write_one()
	{
        auto self = shared_from_this();

		// consolidate outbound messages
		std::string msg;
		for(const auto& x : outbound_messages_)
		{
			msg += x;
		}
		outbound_messages_ = {msg};

		boost::asio::async_write(
			*socket_,
			boost::asio::buffer(outbound_messages_.front().data(), outbound_messages_.front().size()),
			[this, self](const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
			{
                if (ec)
                {
                    event_handlers_->on_error(ec);
                    this->close();
                }
                else
                {
                    // we've sent it, remove from queue
                    outbound_messages_.pop_front();

                    if (!outbound_messages_.empty())
                    {
                        async_write_one();
                    }
                }
			}
		);
	}

	/// return true unless an error occured or the transport was closed
	virtual bool isOpen() override
	{
		return socket_ && socket_->is_open();
	}

	/// Checks wether this transport is closed.
	/*!
	* @returns true, if the transport is closed,
	* false if the connection is open
	* or name resolution is in progress.
	*/
	bool isClosed()
	{
		return !socket_ || !socket_->is_open();
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

		socket_->set_option(boost::asio::ip::tcp::no_delay(true));
		auto receive_buffer = std::make_shared<std::array<char, BUFFER_SIZE>>();

		socket_->async_receive(
			boost::asio::buffer(*receive_buffer, receive_buffer->size()),
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
		if (isOpen())
		{
			boost::system::error_code ec;
			socket_->cancel(ec);
			if (ec) event_handlers_->on_error(ec);
			socket_->close(ec);
			if (ec) event_handlers_->on_error(ec);
            //socket_.reset();
		}
		event_handlers_->on_disconnected();
		incomming_bytes_.clear();
		outbound_messages_.clear();
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
	socket_ptr socket_; ///< the underlying socket
	event_handlers* event_handlers_; ///< handles events like on_error, etc.

  private:
	std::deque<uint8_t> incomming_bytes_;
	std::list<std::string> outbound_messages_;

	void on_receive(
		const boost::system::error_code& ec,
		std::shared_ptr<std::array<char, BUFFER_SIZE>> receive_buffer,
		std::size_t bytes_transferred)
	{
		if (ec)
		{
			event_handlers_->on_error(ec);
			this->close();
		}
		else
		{
			incomming_bytes_.insert(
				incomming_bytes_.end(),
				begin(*receive_buffer),
				begin(*receive_buffer) + bytes_transferred
			);

			//std::clog << "got " << bytes_transferred << " bytes, avail=" << available_bytes() << std::endl;

			socket_->async_receive(
				boost::asio::buffer(*receive_buffer, receive_buffer->size()),
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
