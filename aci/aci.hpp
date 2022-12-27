#pragma once

#include "../lib/types.hpp"
#include "../lib/db.hpp"

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <variant>
#include <set>

// ambry command interpreter 

namespace aci
{
	// the underlying type of the flag
	using FlagT = uint32_t;

	#define FLAG static constexpr FlagT

	FLAG SET = 1 << 0;
	FLAG GET = 1 << 1;
	FLAG CRUD = 1 << 2;
	FLAG VIEW = 1 << 3;

	FLAG ADMIN = std::numeric_limits<FlagT>::max();

	#define SET(f) { #f, f },

	static std::unordered_map<std::string_view, FlagT> perm_table
	{
		SET(SET)
		SET(GET)
		SET(CRUD)
		SET(VIEW)
	};

	#undef SET

	struct Cmd
	{
		std::string cmd;
		std::vector<std::string> args;
	};

	class Interpreter;

	using Result = ambry::BasicResult<std::string>;

	struct Ctx
	{
		Interpreter &inter;
		Cmd &cmd;
		int fd;
	};

	typedef Result(*CmdFN)(Ctx&);

	struct CmdHandle
	{
		int arity;
		std::string_view description;
		std::string_view usage;
		CmdFN fn;
		// if true well expect their to be a wdb set
		bool expect_wdb = true;
		FlagT perms = 0;
	};

	using CmdTable = std::unordered_map<std::string_view, CmdHandle>;
	using DBTable  = std::unordered_map<std::string, ambry::DB>;

	struct Interpreter
	{
		CmdTable ct;
		// a reference to table of open databases
		DBTable &dbt;
		// the working database
		ambry::DB *wdb{};

		// the internal database used for storing user and role information
		ambry::DB users;
		ambry::DB roles;

		// useful for quit commands
		bool running = true;

		std::unordered_map<int, std::string> logins;

		Interpreter(DBTable &dbt) :
			dbt(dbt),
			users("__ACI_USERS__"),
			roles("__ACI_ROLES__")
		{
			users.open();
			roles.open();

			roles.set("admin", std::string_view{(char*)&ADMIN, sizeof(FlagT)});
		}

		void init_commands();

		Result interpret(Cmd &cmd, int from);

		bool calculate_perms(int from, FlagT needed);
	};

	std::vector<Cmd> parse(std::string_view source);
}