#pragma once

#include "types.hpp"
#include "db.hpp"

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
	struct Cmd
	{
		std::string cmd;
		std::vector<std::string> args;
	};

	class Interpreter;

	typedef ambry::Result(*CmdFN)(Interpreter&, Cmd&);

	struct CmdHandle
	{
		int arity;
		std::string_view description;
		std::string_view usage;
		CmdFN fn;
		// if true well expect their to be a wdb set
		bool expect_wdb = true;
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
		// if caching is disabled the interpreter will store values in here
		std::set<std::string> cache;

		// useful for quit commands
		bool running = true;

		Interpreter(DBTable &dbt) :
			dbt(dbt)
		{}

		void init_commands();

		ambry::Result interpret(Cmd &cmd);
	};

	std::optional<Cmd> parse(std::string_view source);
}