#pragma once

#include "fmt.hpp"
#include <ctime>
#include <fcntl.h>

static constexpr uint8_t LOG_STDOUT = 0X1;
static constexpr uint8_t LOG_FILE = 0X2;

static std::string time_str()
{
	time_t tt;
	tm *info;

	std::string buff;

	static constexpr int MAX_LEN = 18;

	buff.resize(MAX_LEN);

	time(&tt);

	info = localtime(&tt);

	strftime(buff.data(), MAX_LEN, "%x %X", info);

	return buff;
}

class Logger
{
public:
	Logger(uint8_t transports, std::string_view filename = "") :
		m_transports(transports),
		m_filename(filename)
	{
		m_fd = open(filename.data(), O_CREAT | O_WRONLY | O_NONBLOCK | O_APPEND, 0777);
	}

	void write(std::string_view data);

	template<typename... A>
    inline void info(std::string_view fmt, A&&... a)
    {
		std::string message = fmt::format(fmt, std::forward<A>(a)...);
		write(fmt::format("[INFO]  {} |{} {}\n", time_str(), message));
    }

	template<typename... A>
    inline void warn(std::string_view fmt, A&&... a)
    {
		std::string message = fmt::format(fmt, std::forward<A>(a)...);
		write(fmt::format("[WARN]  {} |{} {}\n", time_str(), message));
    }

	template<typename... A>
    inline void fatal(std::string_view fmt, A&&... a)
    {
		std::string message = fmt::format(fmt, std::forward<A>(a)...);
		write(fmt::format("[FATAL] {} |{} {}\n", time_str(), message));

		std::exit(-1);
    }

private:
	uint8_t m_transports;
	int m_fd;
	std::string_view m_filename;
};
