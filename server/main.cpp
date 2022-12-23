#include "fmt.hpp"
#include "db.hpp"

#include "log.hpp"
#include "server.hpp"
#include "socket_util.hpp"

int main()
{
	Server server({
		.service   = "3000",
		.sock_act  = SockAct::Bind,
		.log_transports = LOG_FILE | LOG_STDOUT
	});

	server.start();

	server.listen();

	while (true)
	{
		auto messages = server.recv_all(-1);

		for (auto &m : messages)
		{
			fmt::print("{}", m.message);
		}
	}
}