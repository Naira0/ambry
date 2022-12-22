#include "server.hpp"

#define LOG(level, message, ...) \
	if (m_opt.verbose) \
	{ \
	  	m_lgr.level(message, ##__VA_ARGS__);	\
	} \

void Server::start()
{
	m_ci = init(m_opt);

	if (!m_ci.ok())
	{
		LOG(fatal, m_ci.err);
		return;
	}

	LOG(info, "listening on port {}", m_ci.port);
}