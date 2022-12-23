#pragma once

#include <sys/epoll.h>

#include <thread>
#include <vector>

#include "socket_util.hpp"
#include "log.hpp"

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

	void start();

	inline ConInfo info() const
	{
		return m_ci;
	}

	void listen();

	std::vector<Incoming> recv_all(int timeout = -1);
	

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