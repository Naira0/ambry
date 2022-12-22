#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <string_view>

enum class SockAct : uint8_t
{
	Connect, Bind
};

struct SockOpt
{
	std::string_view addr;
	std::string_view service;

	int opt_name;

	SockAct sock_act;

	bool verbose = true;

	uint8_t log_transports;
};

struct ConInfo
{
	int fd;
	int port;
	char ipstr[INET6_ADDRSTRLEN];
	std::string_view err;

	ConInfo() = default;

	ConInfo(const char *err) :
			err(err)
		{}

	ConInfo(std::string_view err) :
		err(err)
	{}

	inline bool ok() const
	{
		return err.empty();
	}
};

ConInfo init(SockOpt opt);

int get_port(int fd);
