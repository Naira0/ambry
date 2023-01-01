#include "server.hpp"
#include "aci.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <fcntl.h>
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
				event.events = EPOLLIN | EPOLLRDHUP;

				int status = epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, cfd, &event);

				if (status == -1)
				{
					LOG_ERRNO(warn);
					continue;
				}

				m_cons.emplace(cfd);
			}
		}
	};
}

std::vector<Incoming> Server::recv_from_all(aci::Interpreter &inter, int timeout)
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

		if (events[i].events & EPOLLRDHUP)
		{
			m_cons.erase(fd);
			inter.logins.erase(fd);
			epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
			continue;
		}

		auto [m, n] = recvall(fd, 4);

		if (n <= 0)
		{
			LOG_ERRNO(warn);
			continue;
		}

		auto len_bytes = m.substr(0, 4);
		auto len = *(uint32_t*)len_bytes.data();

		auto [data, n2] = recvall(fd, len);

		messages.emplace_back(Incoming { std::move(data), fd });
	}

	return messages;
}

void Server::stop()
{
	m_running = false;

	if (m_listener.joinable())
	{
		m_listener.join();
	}
}

void Server::close()
{
	stop();

	::close(m_ci.fd);
	::close(m_epoll_fd);
}

/*
	one byte for the status code
	four bytes for the length of the message
	then the message
*/
std::string construct_responce(const aci::Result &result)
{
	std::string buff;

	buff.resize(5 + result.message.size());

	buff[0] = (uint8_t)result.type;

	uint32_t size = result.message.size();

	memcpy(buff.data()+1, (char*)&size, 4);
	memcpy(buff.data()+5, result.message.data(), result.message.size());

	return buff;
}

void Server::command_loop(aci::Interpreter &inter)
{
	while (true)
	{
		auto messages = recv_from_all(inter, -1);

		for (auto &[m, fd] : messages)
		{	
			LOG(info, "recieved message: {}", m);

			auto cmd = aci::parse(m);

			if (cmd.empty())
			{
				continue;
			}

			for (auto &c : cmd)
			{
				aci::Result result = inter.interpret(c, fd);

				//LOG(info, "'{}' executed by {}", c.cmd, )

				send(construct_responce(result), fd);
			}
		}
	}
}