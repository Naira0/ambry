#include "socket_util.hpp"
#include "fmt.hpp"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define TRY(fn) \
	if (int status = (fn); status == -1) \
	{ \
		close(fd); \
		return status; \
	} \

#define INIT_TRY(fn, ci) \
	if ((fn) == -1) \
	{ \
		ci.err = strerror(errno); \
		continue; \
	} \

void* in_addr(sockaddr *addr)
{
	if (addr->sa_family == AF_INET)
	{
		return &( ( (sockaddr_in*)addr )->sin_addr );
	}

	return &( ( (sockaddr_in6*)addr )->sin6_addr );
}

int bind_sock(int fd, addrinfo *ai, SockOpt opt)
{
	int yes = 1;

	TRY(setsockopt(fd, SOL_SOCKET, opt.opt_name, &yes, sizeof(int)));

	TRY(bind(fd, ai->ai_addr, ai->ai_addrlen));

	return 0;
}

int connect_sock(int fd, addrinfo *ai)
{
	TRY(connect(fd, ai->ai_addr, ai->ai_addrlen));
	return 0;
}

int get_port(int fd)
{
	sockaddr_in sin;

	socklen_t sin_size = sizeof(sin);

	getsockname(fd, (sockaddr*)&sin, &sin_size);

	return ntohs(sin.sin_port);
}

ConInfo init(SockOpt opt)
{
	addrinfo hints{}, *ai, *p;

	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	const char *addr = opt.addr.data();

	if (opt.sock_act == SockAct::Bind)
	{
		hints.ai_flags = AI_PASSIVE;
		addr = nullptr;
	}

	int status = getaddrinfo(addr, opt.service.data(), &hints, &ai);

	if (status != 0)
	{
		return gai_strerror(status);
	}

	int fd {};

	ConInfo ci;

	for (p = ai; p; p = p->ai_next)
	{
		INIT_TRY(fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol), ci);

		switch (opt.sock_act)
		{
			case SockAct::Bind: 
			{
				INIT_TRY(bind_sock(fd, p, opt), ci);
				break;
			}
			case SockAct::Connect:
			{
				INIT_TRY(connect_sock(fd, p), ci);
				break;
			}
		}

		break;
	}

	ci.port = get_port(fd);

	inet_ntop(p->ai_family, in_addr((sockaddr*)&p), ci.ipstr, sizeof(ci.ipstr));

	freeaddrinfo(ai);

	if (!p)
	{
		return "could not find valid addrinfo node";
	}

	ci.fd = fd;

	return ci;
}

std::pair<std::string, int> recv(int fd, size_t max_size)
{
	std::string buff;

	buff.resize(max_size);

	int got = recv(fd, buff.data(), buff.size(), 0);

	if (got == -1)
	{
		return {{}, -1};
	}
	
	buff.resize(got);

	return {buff, got};
}

std::pair<std::string, int> recvall(int fd, size_t size)
{
	std::string buff;

	buff.resize(size);

	int 
		got{},
		n,
		left = buff.size();

	while (got < size)
	{
		n = recv(fd, buff.data()+got, left, 0);

		if (n == -1)
		{
			return {{}, -1};
		}

		got += n;
		left -= n;
	}

	return {buff, got};
}

int send(std::string_view message, int fd)
{
	int sent{};
	int n{};
	int size = message.size();
	
	while (sent < size)
	{
		n = send(fd, message.data()+sent, size-sent, 0);

		if (n == -1)
		{
			break;
		}

		sent += n;
	}

	return sent;
}