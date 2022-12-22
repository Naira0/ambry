#pragma once

#include "socket_util.hpp"
#include "log.hpp"

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

private:
	SockOpt m_opt;
	ConInfo m_ci;
	Logger m_lgr;
};