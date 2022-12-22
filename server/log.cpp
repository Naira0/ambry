#include "log.hpp"
#include <unistd.h>

void Logger::write(std::string_view data)
{
	if (m_transports & LOG_STDOUT)
	{
		::write(STDOUT_FILENO, data.data(), data.size());
	}
	if (m_transports & LOG_FILE)
	{
		::write(m_fd, data.data(), data.size());
	}
}