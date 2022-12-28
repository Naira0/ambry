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
#include <bitset>

// ambry command interpreter 

namespace aci
{
	// the underlying type of the flag
	using FlagT = std::bitset<32>;
	
	enum FLAG 
	{
		SET,
		GET, 
		VIEW,
	};

	#define SET(f) { #f, f },

	static std::unordered_map<std::string_view, FLAG> perm_table
	{
		SET(SET)
		SET(GET)
		SET(VIEW)
	};

	static auto ADMIN = FlagT().set();

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

	template<class ...A>
	FlagT set_perms(A ...a)
	{
		FlagT output;

		((output.set(a)), ...);

		return output;
	}

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
			users("__ACI_USERS__", {.enable_cache = true}),
			roles("__ACI_ROLES__", {.enable_cache = true})
		{
			users.open();
			roles.open();

			auto admin_int = ADMIN.to_ullong();

			roles.set("admin", {(char*)&admin_int, sizeof(admin_int)});
		}

		void init_commands();

		Result interpret(Cmd &cmd, int from);

		bool calculate_perms(int from, FlagT needed);
	};

	std::vector<Cmd> parse(std::string_view source);
}