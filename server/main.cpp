#include "fmt.hpp"
#include "db.hpp"

#include "log.hpp"
#include "server.hpp"
#include "socket_util.hpp"

int main()
{
	Server server({
		.service   = "3000",
		.opt_name  = SO_REUSEADDR,
		.sock_act  = SockAct::Bind,
		.verbose   = true,
		.log_transports = LOG_FILE | LOG_STDOUT
	});

	server.start();
}