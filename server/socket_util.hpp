#pragma once

#include <asm-generic/socket.h>
#include <cstdint>
#include <netinet/in.h>
#include <optional>
#include <string_view>

enum class SockAct : uint8_t
{
	Connect, Bind
};

struct SockOpt
{
	std::string_view addr;
	std::string_view service;

	int opt_name = SO_REUSEADDR;

	SockAct sock_act;

	bool verbose = true;

	uint8_t log_transports;

	int backlog = 20;

	int max_epoll_size = 256;
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

void* in_addr(sockaddr *addr);

std::pair<std::string, int> recv(int fd);
