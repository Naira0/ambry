#pragma once

#include <sys/epoll.h>

#include <thread>
#include <vector>

#include "socket_util.hpp"
#include "log.hpp"

#include "aci.hpp"

struct Incoming
{
	std::string message;
	int fd;
};

class Server
{
public:
	
	Server(SockOpt opt) :
		m_opt(opt),
		m_lgr(m_opt.log_transports, "server.log")
	{}

	~Server()
	{
		close();
	}

	void start();

	inline ConInfo info() const
	{
		return m_ci;
	}

	void listen();

	void stop();

	void close();

	std::vector<Incoming> recv_from_all(int timeout = -1);
	
	void command_loop(aci::Interpreter &inter);

	inline int fd() const
	{
		return m_ci.fd;
	}

private:
	SockOpt m_opt;
	ConInfo m_ci;
	Logger m_lgr;

	std::thread m_listener;

	bool m_running = true;

	std::mutex m_mutex;

	int m_epoll_fd;
};