#include "fmt.hpp"
#include "db.hpp"
#include "aci.hpp"

#include "log.hpp"
#include "server.hpp"
#include "socket_util.hpp"
#include <cstdint>

aci::Result quit_cb(aci::Ctx &ctx)
{
	ctx.inter.running = false;
	return {};
}

int main()
{
	Server server({
		.service   = "3000",
		.sock_act  = SockAct::Bind,
		.log_transports = LOG_FILE | LOG_STDOUT
	});

	server.start();

	server.listen();

	aci::DBTable dt;

	aci::Interpreter inter(dt);

	inter.init_commands();

	inter.ct["quit"] =
	{
		.description = "closes the server",
		.usage = "",
		.fn = quit_cb,
		.expect_wdb = false,
	};

	server.command_loop(inter);
}