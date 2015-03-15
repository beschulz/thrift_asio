//
// Created by Benjamin Schulz on 15/03/15.
//

#include <betabugs/networking/thrift_asio_server.hpp>
#include <chat_server.h>
#include <chat_client.h>

class chat_server_handler
	: public example::chat::chat_serverIf
	  , public betabugs::networking::thrift_asio_transport::event_handlers
{

  public:
	virtual void set_user_name(const std::string& name) override
	{
		for (auto& p : sessions_)
		{
			if (p.second->user_name == name)
			{
				p.second->client.on_set_user_name_failed("username already taken");
				return;
			}
		}

		assert(current_session_);
		current_session_->user_name = name;
		current_session_->client.on_set_user_name_succeeded();
	}

	virtual void send_message(const std::string& to_user, const std::string& message) override
	{
		assert(current_session_);
		for (auto& s : sessions_)
		{
			if( s.second->user_name == to_user )
			{
				s.second->client.on_message(current_session_->user_name, message);
				return;
			}
		}
		current_session_->client.on_send_message_failed("no such user " + to_user);
	}

	virtual void broadcast_message(const std::string& message) override
	{
		assert(current_session_);
		for (auto& s : sessions_)
		{
			if (s.second != current_session_)
				s.second->client.on_message(current_session_->user_name, message);
		}
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
		sessions_.insert(
			std::make_pair(
				output_protocol,
				std::make_shared<Session>(output_protocol, "no-name")
			)
		);
	}

	void before_process(boost::shared_ptr<apache::thrift::protocol::TBinaryProtocol> output_protocol)
	{
		Sessions::iterator pos = sessions_.find(output_protocol);
		assert(pos != sessions_.end());
		current_session_ = pos->second;
	}

	void after_process()
	{
		current_session_.reset();
	}

  private:
	typedef boost::shared_ptr<apache::thrift::protocol::TProtocol> protocol_ptr;

	struct Session
	{
		Session(const example::chat::chat_clientClient& client, const std::string& user_name)
			: client(client), user_name(user_name)
		{
		}

		example::chat::chat_clientClient client;
		std::string user_name;
	};

	typedef std::shared_ptr<Session> session_ptr;

	typedef std::map<protocol_ptr, session_ptr> Sessions;
	Sessions sessions_;

	session_ptr current_session_;
};

int main(int argc, char* argv[])
{
	(void) argc;
	(void) argv;

	auto handler = boost::make_shared<chat_server_handler>();
	auto processor = example::chat::chat_serverProcessor
		(
			handler
		);

	betabugs::networking::thrift_asio_server<chat_server_handler> server;

	boost::asio::io_service io_service;
	boost::asio::io_service::work work(io_service);

	server.serve(io_service, processor, handler, 1528);

	while (true)
	{
		while (io_service.poll_one());
		sleep(1);
		std::clog << "loop" << std::endl;
	}

	return 0;
}
