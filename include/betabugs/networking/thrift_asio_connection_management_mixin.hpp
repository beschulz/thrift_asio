//
// Created by Benjamin Schulz on 16/03/15.
//

#ifndef _THRIFT_ASIO_THRIFT_ASIO_CONNECTION_MANAGEMENT_MIXIN_HPP_
#define _THRIFT_ASIO_THRIFT_ASIO_CONNECTION_MANAGEMENT_MIXIN_HPP_

namespace betabugs{
namespace networking{

/**
* This class provides management of connected clients.
* This is the easiest way to get access to all connected clients.
*
* inherit your server side handler from this class to get access to
* the connected clients and the current client
*
* @tparam ClientType the client to create out of the output_protocol. If you want other session data,
* you can wrap the auto-generted client into your own custom class that takes a
* boost::shared_ptr<apache::thrift::protocol::TProtocol> as the first and only argument
*
* to respond to the current client:
* @code
* current_client_->on_fancy_result_computed(42);
* @endcode
*
* to broadcast a message to all clients:
*
* @code
* for(auto& client : clients_)
* {
* 	client.second->on_fancy_result_computed(42);
* }
* @endcode
*
*
* */
template <typename ClientType>
class thrift_asio_connection_management_mixin
{
  public:
	/// creates a ClientType objects and inserts it into clients_
	virtual void on_client_connected(boost::shared_ptr<apache::thrift::protocol::TProtocol> output_protocol)
	{
		assert( clients_.find(output_protocol) == clients_.end() );

		current_client_ = std::make_shared<ClientType>(output_protocol);

		clients_.insert(
			std::make_pair(
				output_protocol,
				current_client_
			)
		);
	}

	/// erases the client associated with output_protocol from clients_
	virtual void on_client_disconnected(const boost::shared_ptr<apache::thrift::protocol::TProtocol>& output_protocol, const boost::system::error_code& ec)
	{
		(void)ec;
		assert( clients_.find(output_protocol) != clients_.end() );
		clients_.erase(output_protocol);
		assert( clients_.find(output_protocol) == clients_.end() );
	}

	/// sets client associated with output_protocol as the current_client_
	virtual void before_process(boost::shared_ptr<apache::thrift::protocol::TProtocol> output_protocol)
	{
		auto pos = clients_.find(output_protocol);
		assert(pos != clients_.end());
		current_client_ = pos->second;
	}

	/// sets the current_client_ to zero
	virtual void after_process()
	{
		current_client_.reset();
	}

    virtual ~thrift_asio_connection_management_mixin(){}
  protected:
	/// used as key_type in the client_map
	typedef boost::shared_ptr<apache::thrift::protocol::TProtocol> protocol_ptr;

	/// used as mapped_type in the client_map
	typedef std::shared_ptr<ClientType> client_ptr;

	/// maps protocol instances to clients
	typedef std::map<protocol_ptr, client_ptr> client_map;

	/// All connected clients.
	client_map clients_;

	/// Only valid while a request is processed.
	client_ptr current_client_;
};

}
}

#endif //_THRIFT_ASIO_THRIFT_ASIO_CONNECTION_MANAGEMENT_MIXIN_HPP_
