#include "fmt.hpp"
#include "db.hpp"
#include "aci.hpp"

#include "log.hpp"
#include "server.hpp"
#include "socket_util.hpp"
#include <cstdint>

#include "flags.hpp"

SockOpt init_opts(int argc, char **argv)
{
	flag::Parser parser(std::span{argv, (size_t)argc}, {
        .flag_prefix = "-",
        .separator = "=",
        .strict_flags = true,
    });

	flag::Result result = parser
		.set({
			.name = "port",
			.description = "sets the port for the server to run on",
			.data = "3000",
			.type = flag::String,
			.aliases = {"p"},
		})
		.set({
			.name = "verbose",
			.description = "if set the server will log everything",
			.data = false,
			.type = flag::Bool,
			.aliases = {"v"},
		})
		.set({
			.name = "transport_stdout",
			.description = "enables stdout transport",
			.data = false,
			.type = flag::Bool,
			.aliases = {"tstdout"},
		})
		.set({
			.name = "transport_file",
			.description = "enables logging to a server.log file transport",
			.data = false,
			.type = flag::Bool,
			.aliases = {"tfile"},
		})
		.set({
			.name = "backlog",
			.description = "the number connections it will keep in the backlog",
			.data = 20.f,
			.type = flag::Number,
			.aliases = {"bl"},
		})
		.set({
			.name = "poll_size",
			.description = "the number connections it will attempt to read from",
			.data = 256.f,
			.type = flag::Number,
			.aliases = {"ps"},
		})
		.parse();

	if (!result.ok())
	{
		fmt::fatal("could not parse flags {}", result.error);
	}

#define GET(t, id) std::get<t>(parser.get(id).value()->data)

	uint8_t transports;

	if (GET(flag::Bool, "tfile"))
	{
		transports |= LOG_FILE;
	}

	if (GET(flag::Bool, "tstdout"))
	{
		transports |= LOG_STDOUT;
	}

	SockOpt opt
	{
		.service = GET(flag::String, "port"),
		.sock_act = SockAct::Bind,
		.verbose = GET(flag::Bool, "verbose"),
		.log_transports = transports,
		.backlog = int(GET(flag::Number, "backlog")),
		.max_epoll_size = int(GET(flag::Number, "poll_size")),
	};

#undef GET

	return opt;
}

int main(int argc, char **argv)
{
	SockOpt opt = init_opts(argc, argv);

	Server server(opt);

	server.start();

	server.listen();

	aci::DBTable dt;

	aci::Interpreter inter(dt);

	inter.init_commands();

	server.command_loop(inter);
}