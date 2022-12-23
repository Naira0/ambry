#include "server.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <sys/epoll.h>

#define LOG(level, message, ...) \
	if (m_opt.verbose) \
	{ \
	  	m_lgr.level(message, ##__VA_ARGS__);	\
	} \

#define LOG_ERRNO(level) \
	if (m_opt.verbose) \
	{ \
	  	m_lgr.level(strerror(errno));	\
	} \

void Server::start()
{
	m_ci = init(m_opt);

	if (!m_ci.ok())
	{
		LOG(fatal, m_ci.err);
		return;
	}

	LOG(info, "started on {}:{}", m_ci.ipstr, m_ci.port);

	m_epoll_fd = epoll_create1(0);
}

void Server::listen()
{
	int status = ::listen(m_ci.fd, m_opt.backlog);

	if (status == -1)
	{
		LOG_ERRNO(fatal);
	}

	m_listener = std::thread
	{
		[&]
		{
			char ip_str[INET6_ADDRSTRLEN];

			sockaddr_storage client;

			while (m_running)
			{
				socklen_t sin_size = sizeof(client);

				int cfd = accept(m_ci.fd, (sockaddr*)&client, &sin_size);

				if (cfd == -1)
				{
					LOG(warn, strerror(errno));
					continue;
				}

				inet_ntop(client.ss_family, in_addr((sockaddr*)&client), ip_str, sizeof(ip_str));

				LOG(info, "accepted connection from {}", ip_str);

				epoll_event event;

				event.data.fd = cfd;
				event.events = EPOLLIN;

				int status = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, cfd, &event);

				if (status == -1)
				{
					LOG_ERRNO(warn);
					continue;
				}
			}
		}
	};
}

std::vector<Incoming> Server::recv_all(int timeout)
{
	std::vector<epoll_event> events;

	events.resize(m_opt.max_epoll_size);

	int size = epoll_wait(m_epoll_fd, events.data(), events.size(), timeout);

	if (size <= 0)
	{
		if (size == -1)
		{
			LOG_ERRNO(warn);
		}

		return {};
	}

	std::vector<Incoming> messages;

	messages.reserve(size);

	for (int i = 0; i < size; i++)
	{
		int fd = events[i].data.fd;

		auto message = recv(fd);

		if (message.second == -1)
		{
			LOG_ERRNO(warn);
			epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
			continue;
		}

		messages.emplace_back(Incoming { std::move(message.first), fd });
	}

	return messages;
}