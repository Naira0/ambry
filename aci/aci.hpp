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
	using FlagT = std::bitset<15>;
	
	enum FLAG 
	{
		SET,
		GET,
		ERASE,
		UPDATE,
		CREATE_USER,
		DELETE_USER,
		CREATE_ROLE,
		DELETE_ROLE,
		ADD_ROLES,
		REMOVE_ROLES,
		OPEN,
		CLOSE,
		CHANGE_OPT,
		DESTROY,
		INFO,
	};

	#define SETF(f) { #f, f },

	static std::unordered_map<std::string_view, FLAG> perm_table
	{
		SETF(SET)
		SETF(GET)
		SETF(ERASE)
		SETF(UPDATE)
		SETF(CREATE_USER)
		SETF(DELETE_USER)
		SETF(CREATE_ROLE)
		SETF(ADD_ROLES)
		SETF(REMOVE_ROLES)
		SETF(OPEN)
		SETF(CLOSE)
		SETF(CHANGE_OPT)
		SETF(DESTROY)
		SETF(INFO)
	};

	static auto ADMIN = FlagT().set();

	#undef SETF

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
		ambry::DB *&wdb;
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

	struct Login
	{
		std::string name;
		ambry::DB *wdb;
	};

	template<class ...A>
	constexpr FlagT set_perms(A ...a)
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

		// the internal database used for storing user and role information
		ambry::DB users;
		ambry::DB roles;

		// dbs that are disallowed to perform ops on
		std::set<std::string_view> restricted_dbs;

		// useful for quit commands
		bool running = true;

		std::unordered_map<int, Login> logins;

		Interpreter(DBTable &dbt) :
			dbt(dbt),
			users("__ACI_USERS__", {.enable_cache = true}),
			roles("__ACI_ROLES__", {.enable_cache = true})
		{
			users.open();
			roles.open();

			restricted_dbs.emplace("__ACI_USERS__");
			restricted_dbs.emplace("__ACI_ROLES__");

			auto admin_int = ADMIN.to_ulong();

			roles.set("admin", {(char*)&admin_int, sizeof(admin_int)});
		}

		void init_commands();

		Result interpret(Cmd &cmd, int from);

		bool calculate_perms(int from, FlagT needed);

		ambry::DB** get_wdb(int from);
	};

	std::vector<Cmd> parse(std::string_view source);
}