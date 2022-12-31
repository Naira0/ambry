#include "aci.hpp"
#include "../lib/db.hpp"
#include "../lib/types.hpp"
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <cstring>
#include <unordered_set>
#include <openssl/md5.h>

#include "../shared/fmt.hpp"
#include "../lib/util.hpp"

#define INTER_ERR(msg) {ambry::ResultType::InterpretError, msg}
#define KEY_NOT_FND {ambry::ResultType::InterpretError, "there is no database by that name"}
#define TO_RES(v) Result{v.type, std::string(v.message)}

namespace aci
{

	ambry::DB** Interpreter::get_wdb(int from)
	{
		auto iter = logins.find(from);

		if (iter == logins.end())
		{
			return nullptr;
		}

		return &iter->second.wdb;
	}

	Result update_set_impl(Ctx &ctx, uint8_t m)
	{
		auto iter = ctx.cmd.args.begin();

		const std::string &key = *iter;

		std::string args;

		iter++;

		for (; iter != ctx.cmd.args.end(); iter++)
		{
			args += std::move(*iter);
		}

		auto res = m ? ctx.wdb->set(key, args) : ctx.wdb->update(key, args);

		return TO_RES(res);
	}

	Result open_cb(Ctx &ctx)
	{
		const std::string &name = ctx.cmd.args.front();

		if (ctx.inter.restricted_dbs.contains(name))
		{
			return INTER_ERR("the provided database is restricted");
		}

		auto [iter, emplaced] = ctx.inter.dbt.emplace(name, ambry::DB(name));
		
		if (!emplaced)
		{
			ctx.wdb = &iter->second;
			return {};
		}

		ctx.wdb = &iter->second;

		auto res = ctx.wdb->open();

		return TO_RES(res);
	}

	Result close_cb(Ctx &ctx)
	{
		auto iter = ctx.inter.dbt.find(ctx.cmd.args.front());

		if (iter == ctx.inter.dbt.end())
		{
			return KEY_NOT_FND;
		}

		if (&iter->second == ctx.wdb)
		{
			ctx.wdb = nullptr;
		}

		iter->second.close();

		ctx.inter.dbt.erase(iter);

		return {};
	}

	Result switch_cb(Ctx &ctx)
	{
		auto iter = ctx.inter.dbt.find(ctx.cmd.args.front());

		if (iter == ctx.inter.dbt.end())
		{
			return KEY_NOT_FND;
		}

		ctx.wdb = &iter->second;

		return {};
	}

	Result cache_mode_cb(Ctx &ctx)
	{
		const std::string &s = ctx.cmd.args.front();

		bool on;

		if (s == "on")
		{
			on = true;
		}
		else if (s == "off")
		{
			on = false;
		}
		else
		{
			return INTER_ERR("exepcted on or off");
		}

		ctx.wdb->switch_cache(on);

		return {};
	}

	Result set_cb(Ctx &ctx)
	{
		return update_set_impl(ctx, 1);
	}

	Result get_cb(Ctx &ctx)
	{
		const std::string &key = ctx.cmd.args.front();

		if (ctx.wdb->is_cached())
		{
			auto opt = ctx.wdb->get_cached(key);

			if (!opt)
			{
				return {ambry::ResultType::KeyNotFound, "key not found"};
			}

			return {ambry::ResultType::Ok, std::string{ opt.value() }};
		}
		else
		{
			auto opt = ctx.wdb->get(key);

			if (!opt)
			{
				return {ambry::ResultType::KeyNotFound, "key not found"};
			}

			return {ambry::ResultType::Ok, std::move(opt.value()) };
		}
	}

	Result close_all_cb(Ctx &ctx)
	{
		for (auto &[_, db] : ctx.inter.dbt) 
		{
			db.close();
		}

		ctx.wdb = nullptr;
		ctx.inter.dbt.clear();

		return {};
	}

	Result destroy_cb(Ctx &ctx)
	{
		std::string &name = ctx.cmd.args.front();

		if (ctx.inter.restricted_dbs.contains(name))
		{
			return INTER_ERR("the provided database is restricted");
		}
		
		auto iter = ctx.inter.dbt.find(name);

		if (iter == ctx.inter.dbt.end())
		{
			auto result = ambry::destroy(name);
			return TO_RES(result);
		}

		if (&iter->second == ctx.wdb)
		{
			ctx.wdb = nullptr;
		}

		auto res = iter->second.destroy();

		return TO_RES(res);
	}

	Result destroy_all_cb(Ctx &ctx)
	{
		for (auto &[_, db] : ctx.inter.dbt)
		{
			ambry::Result result = db.destroy();

			if (!result.ok())
			{
				return TO_RES(result);
			}
		}

		ctx.inter.dbt.clear();

		ctx.wdb = nullptr;

		return {};
	}

	Result cmds_cb(Ctx &ctx)
	{
		std::string output;

		for (auto [cmd, ch] : ctx.inter.ct)
		{
			std::string arg_count;

			if (ch.arity == -1)
			{
				arg_count = "...";
			}
			else
			{
				arg_count = std::to_string(ch.arity);
			}

			output += fmt::format("- {}({}):\n\tDescription: {}\n\tUsage: {}{}\n",
					  cmd, arg_count, ch.description, cmd, ch.usage);
		}

		return {{}, output};
	}

	Result update_cb(Ctx &ctx)
	{
		return update_set_impl(ctx, 0);
	}

	Result erase_cb(Ctx &ctx)
	{
		auto res = ctx.wdb->erase(ctx.cmd.args.front());
		return TO_RES(res);
	}

	Result list_open_cb(Ctx &ctx)
	{
		std::string output;

		output += "name - size\n";

		for (auto &[_, db] : ctx.inter.dbt)
		{
			output += fmt::format("{} - {}\n", db.name(), db.size());
		}

		return {{}, output};
	}

	Result create_user_cb(Ctx &ctx)
	{
		auto iter = ctx.cmd.args.begin();

		std::string &username = *iter++;
		std::string &password = *iter++;

		std::string buff;

		if (password.size() > 255)
		{
			return INTER_ERR("password length must be less then 256 chars");
		}

		buff += (uint8_t)password.size();
		buff += password;

		if (ctx.inter.users.size() == 0)
		{
			buff += (uint8_t)5;
			buff += "admin";

			ctx.inter.ct["create_user"].perms = ADMIN;

			goto set;
		}

		for (; iter != ctx.cmd.args.end(); iter++)
		{
			if (iter->size() > 255)
			{
				return INTER_ERR("role length must be less then 256 chars");
			}

			if (!ctx.inter.roles.contains(*iter))
			{
				return INTER_ERR("invalid rolename provided");
			}

			buff += (uint8_t)iter->size();
			buff += *iter;
		}
set:
		auto res = ctx.inter.users.set(username, buff);

		return TO_RES(res);
	}

	Result create_role_cb(Ctx &ctx)
	{
		auto iter = ctx.cmd.args.begin();

		std::string &name = *iter++;

		if (name.size() > 256)
		{
			return INTER_ERR("role length must be less then 256 chars");
		}

		FlagT f;

		for (; iter != ctx.cmd.args.end(); iter++)
		{
			auto flag = perm_table.find(*iter);

			if (flag == perm_table.end())
			{
				return INTER_ERR("invalid flag name provided");
			}

			f.set(flag->second);
		}

		auto ulong = f.to_ulong();

		auto res = ctx.inter.roles.set(name, {(char*)&ulong, sizeof(ulong)});

		return TO_RES(res);
	}

	std::string_view get_field(const std::string &str, size_t off)
	{
		uint8_t len = str[off];
		return { str.data()+off+1, len };
	}

	Result login_cb(Ctx &ctx)
	{
		if (ctx.inter.logins.contains(ctx.fd))
		{
			return INTER_ERR("you are already logged in to a user account");
		}

		std::string &username = ctx.cmd.args[0];
		std::string &password = ctx.cmd.args[1];

		auto opt = ctx.inter.users.get(username);

		if (!opt)
		{
			return INTER_ERR("invalid username provided");
		}

		std::string &data = opt.value();

		std::string_view userpass = get_field(data, 0);

		if (password != userpass)
		{
			return INTER_ERR("password does not match");
		}

		ctx.inter.logins.emplace(ctx.fd, std::move(username));

		return {};
	}

	Result add_roles_cb(Ctx &ctx)
	{
		auto iter = ctx.cmd.args.begin();

		std::string &username = *iter++;

		auto opt = ctx.inter.users.get(username);

		if (!opt)
		{
			return INTER_ERR("invalid username provided");
		}

		std::string &data = opt.value();

		for (; iter != ctx.cmd.args.end(); iter++)
		{
			if (!ctx.inter.roles.contains(*iter))
			{
				return INTER_ERR("invalid role name provided");
			}

			data += (uint8_t)iter->size();
			data += *iter;
		}

		auto res = ctx.inter.users.update(username, data);

		return TO_RES(res);
	}

	Result remove_roles_cb(Ctx &ctx)
	{
		auto iter = ctx.cmd.args.begin();

		std::string &username = *iter++;

		auto opt = ctx.inter.users.get(username);

		if (!opt)
		{
			return INTER_ERR("invalid username provided");
		}

		std::string &data = opt.value();

		std::string buff;

		buff.reserve(data.size());

		buff += data.substr(0, data[0]+1);

		std::set<std::string_view> roles {iter, ctx.cmd.args.end()};

		size_t offset = buff.size();

		while (offset < data.size())
		{
			std::string_view role = get_field(data, offset);
			offset += role.size()+1;

			if (roles.contains(role))
			{
				continue;
			}

			buff += (uint8_t)iter->size();
			buff += *iter;

			iter++;
		}

		auto res = ctx.inter.users.update(username, buff);

		return TO_RES(res);
	}

	Result logout_cb(Ctx &ctx)
	{
		ctx.inter.logins.erase(ctx.fd);
		return {};
	}

	Result delete_user_cb(Ctx &ctx)
	{
		ambry::Result result = ctx.inter.users.erase(ctx.cmd.args.front());

		if (!result.ok())
		{
			return INTER_ERR("the provided user does not exist");
		}

		return {};
	}

	Result delete_role_cb(Ctx &ctx)
	{
		ambry::Result result = ctx.inter.roles.erase(ctx.cmd.args.front());

		if (!result.ok())
		{
			return INTER_ERR("the provided rolename does not exist");
		}

		return {};
	}

	Result active_users_cb(Ctx &ctx)
	{
		std::string output;

		for (auto &[_, login] : ctx.inter.logins)
		{
			output += login.name + '\n';
		}

		return {{}, output};
	}

	FlagT from_str(std::string_view str)
	{
		unsigned long ulong;

		memcpy((char*)&ulong, str.data(), sizeof(ulong));

		return ulong;
	}

	std::string perm_string(std::string_view flag_raw)
	{
		std::string output;

		FlagT flag = from_str(flag_raw);

		for (auto [name, value] : perm_table)
		{
			if (flag.test(value))
			{
				output += name;
				output += ' ';
			}
		}

		return output;
	}

	Result show_roles_cb(Ctx &ctx)
	{
		std::string output;

		for (const auto &[role, value] : ctx.inter.roles)
		{
			output += role;
			output += ' ' + perm_string(value);
			output += '\n';
		}

		return {{}, output};
	}

	Result user_roles_cb(Ctx &ctx)
	{
		std::string output;

		auto opt = ctx.inter.users.get(ctx.cmd.args.front());

		if (!opt)
		{
			return INTER_ERR("invalid username provided");
		}

		std::string &data = opt.value();

		size_t offset = data[0]+1;

		while (offset < data.size())
		{
			std::string role_name { get_field(data, offset) };
			offset += role_name.size()+1;

			auto opt = ctx.inter.roles.get(role_name);

			// probably deleted
			if (!opt)
			{
				continue;
			}

			output += role_name;
			output += ' ' + perm_string(opt.value()) + '\n';
		}

		return {{}, output};
	}

	Result show_users_cb(Ctx &ctx)
	{
		std::string output;

		for (auto [username, _] : ctx.inter.users)
		{
			output += fmt::format("{}\n", username);
		}

		return {{}, output};
	}

	Result working_db_cb(Ctx &ctx)
	{
		return {{}, std::string{ ctx.wdb->name() }};
	}

	void Interpreter::init_commands()
	{
		ct["create_user"] = 
		{
			.arity = 2,
			.description = "creates a new user account",
			.usage = " <user name> <password> [role] ...",
			.fn = create_user_cb,
			.expect_wdb = false,
			.perms = users.size() == 0 ? 0 : set_perms(CREATE_USER),
		};

		ct["working_db"] = 
		{
			.description = "returns the working db name",
			.fn = working_db_cb,
			.perms = set_perms(INFO),
		};

		ct["show_users"] = 
		{
			.description = "returns a list of all users",
			.fn = show_users_cb,
			.expect_wdb = false,
			.perms = set_perms(INFO),
		};

		ct["user_roles"] = 
		{
			.arity = 1,
			.description = "show all the roles a user has",
			.usage = " <user name>",
			.fn = user_roles_cb,
			.expect_wdb = false,
			.perms = set_perms(INFO),
		};

		ct["active_users"] = 
		{
			.description = "returns a list of active users",
			.fn = active_users_cb,
			.expect_wdb = false,
			.perms = set_perms(INFO),
		};

		ct["show_roles"] = 
		{
			.description = "returns a list of roles",
			.fn = show_roles_cb,
			.expect_wdb = false,
			.perms = set_perms(INFO),
		};

		ct["delete_role"] = 
		{
			.arity = 1,
			.description = "deletes a role by its name",
			.usage = " <role name>",
			.fn = delete_role_cb,
			.expect_wdb = false,
			.perms = set_perms(DELETE_ROLE)
		};

		ct["create_role"] = 
		{
			.arity = 1,
			.description = "creates a new role with permissions",
			.usage = " <role name> <permission> ...",
			.fn = create_role_cb,
			.expect_wdb = false,
			.perms = set_perms(CREATE_ROLE)
		};

		ct["delete_user"] = 
		{
			.arity = 1,
			.description = "deletes a user",
			.usage = " <user name>",
			.fn = delete_user_cb,
			.expect_wdb = false,
			.perms = set_perms(DELETE_USER)
		};

		ct["login"] = 
		{
			.arity = 2,
			.description = "login command",
			.usage = " <username> <password>",
			.fn = login_cb,
			.expect_wdb = false,
		};

		ct["logout"] = 
		{
			.description = "logs out the command sender or does nothing if the command sender is not logged in",
			.fn = logout_cb,
			.expect_wdb = false,
		};

		ct["add_roles"] = 
		{
			.arity = 2,
			.description = "adds any number of roles to a provided user",
			.usage = " <username> <role> ...",
			.fn = add_roles_cb,
			.expect_wdb = false,
			.perms = set_perms(ADD_ROLES)
		};

		ct["remove_roles"] = 
		{
			.arity = 2,
			.description = "removes any number of roles to a provided user",
			.usage = " <username> <role> ...",
			.fn = remove_roles_cb,
			.expect_wdb = false,
			.perms = set_perms(REMOVE_ROLES)
		};

		ct["open"] = 
		{
			.arity = 1,
			.description = "opens a database",
			.usage = " <db_name>",
			.fn = open_cb,
			.expect_wdb = false,
			.perms = set_perms(OPEN)
		};

		ct["close"] = 
		{
			.arity = 1,
			.description = "closes a database or the working database if only one db is open",
			.usage = " <db_name>",
			.fn = close_cb,
			.expect_wdb = false,
			.perms = set_perms(CLOSE)
		};

		ct["switch"] = 
		{
			.arity = 1,
			.description = "switches the current database to one with the give arguments name",
			.usage = " <db_name>",
			.fn = switch_cb,
			.expect_wdb = false,
		};

		ct["cache_mode"] = 
		{
			.arity = 1,
			.description = "turns the cache mode either on or off",
			.usage = " <on/off>",
			.fn = cache_mode_cb,
			.perms = set_perms(CHANGE_OPT)
		};

		ct["set"] = 
		{
			.arity = -1,
			.description = "sets a value to the working database. any additional values will be concatenated together",
			.usage = " <key> <value> <...>",
			.fn = set_cb,
			.perms = set_perms(SET),
		};

		ct["update"] = 
		{
			.arity = -1,
			.description = "updates a value to the working database. any additional values will be concatenated together",
			.usage = " <key> <value> <...>",
			.fn = update_cb,
			.perms = set_perms(UPDATE)
		};

		ct["get"] = 
		{
			.arity = 1,
			.description = "gets a value from the workind database from the given key",
			.usage = " <key>",
			.fn = get_cb,
			.perms = set_perms(GET),
		};

		ct["erase"] = 
		{
			.arity = 1,
			.description = "erases a value from the workind database from the given key",
			.usage = " <key>",
			.fn = erase_cb,
			.perms = set_perms(ERASE)
		};

		ct["close_all"] = 
		{
			.arity = 0,
			.description = "closes all open databases",
			.usage = "",
			.fn = close_all_cb,
			.perms = set_perms(CLOSE)
		};

		ct["destroy"] = 
		{
			.arity = 1,
			.description = "destroys a database by its name (does not need to be open)",
			.usage = " <db_name>",
			.fn = destroy_cb,
			.expect_wdb = false,
			.perms = set_perms(DESTROY),
		};

		ct["destroy_all"] = 
		{
			.arity = 0,
			.description = "destroys all open databases",
			.usage = "",
			.fn = destroy_all_cb,
			.perms = set_perms(DESTROY)
		};

		ct["cmds"] = 
		{
			.arity = 0,
			.description = "displays all commands",
			.usage = "",
			.fn = cmds_cb,
			.expect_wdb = false,
			.perms = set_perms(INFO)
		};

		ct["list_open"] = 
		{
			.arity = 0,
			.description = "displays all open databases",
			.usage = "",
			.fn = list_open_cb,
			.expect_wdb = false,
			.perms = set_perms(INFO)
		};
	}

	bool Interpreter::calculate_perms(int from, FlagT needed)
	{
		if (needed == 0)
		{
			return true;
		}

		auto iter = logins.find(from);

		if (iter == logins.end())
		{
			return false;
		}

		auto opt = users.get(iter->second.name);

		if (!opt)
		{
			return false;
		}

		std::string &data = opt.value();

		size_t offset = data[0]+1;

		int must_satisfy = needed.count();
		int satisfied = 0;

		while (offset < data.size())
		{
			std::string_view role_name = get_field(data, offset);
			offset += role_name.size()+1;

			auto opt = roles.get(std::string{role_name});

			if (!opt)
			{
				continue;
			}

			FlagT flag = from_str(opt.value());

			for (auto [_, perm] : perm_table)
			{
				if (needed.test(perm) && flag.test(perm))
				{
					satisfied++;
					needed.flip(perm);
				}
			}
		}

		return satisfied >= must_satisfy;
	}

	Result Interpreter::interpret(Cmd &cmd, int from)
	{
		auto iter = ct.find(cmd.cmd);

		if (iter == ct.end())
		{
			return INTER_ERR("command does not exist");
		}

		auto &[_, ch] = *iter;

		if (ch.arity != -1 && cmd.args.size() < ch.arity)
		{
			return INTER_ERR("Not enough arguments to command");
		}

		ambry::DB **wdb = get_wdb(from);

		if (ch.expect_wdb && !(*wdb))
		{
			return INTER_ERR("expected an open database but none found");
		}

		Ctx ctx
		{
			*this,
			cmd,
			from,
			*wdb
		};

		if (!calculate_perms(from, ch.perms))
		{
			return INTER_ERR("you do not meet the permissions to execute this command");
		}

		return ch.fn(ctx);
	}

	struct ParserCtx
	{
		std::string_view str;
		size_t pos{};

		ParserCtx(std::string_view str) :
			str(str)
		{}

		inline bool at_end() const
		{
			return pos >= str.size();
		}

		inline bool is_unimportant() const
		{
			char c = str[pos];
			return c == ' ' || c == '\t' || c == '\r' || c == '\n';
		}

		template<class FN>
		inline bool advance_if(FN fn)
		{
			while (!at_end() && fn())
			{
				pos++;
			}

			return at_end();
		}

		inline char peek() const
		{
			if (at_end())
				return '\0';
			return str[pos];
		}
	};

	char escape(char c)
	{
		switch (c)
		{
			case '"':  return '"';
			case 'n':  return '\n';
			case 'r':  return '\r';
			case 't':  return '\t';
			case 'b':  return '\b';
			case 'a':  return '\a';
			case '\\': return '\\';
			default:   return c;
		}
	}

	std::string parse_quoted(ParserCtx &ctx)
	{
		++ctx.pos;

		std::string buff;

		buff.reserve(ctx.str.size()-ctx.pos);

		while (!ctx.at_end() && ctx.peek() != '"')
		{
			char c = ctx.peek();

			if (c == '\\' && ctx.pos+2 < ctx.str.size())
			{
				ctx.pos++;
				c = escape(ctx.peek());
			}

			buff += c;

			ctx.pos++;
		}

		ctx.pos++;

		return buff;
	}

	std::optional<std::string> parse_arg(ParserCtx &ctx)
	{
		bool at_end = ctx.advance_if([&]{ return ctx.is_unimportant(); });

		if (at_end || ctx.peek() == '\n')
		{
			return std::nullopt;
		}

		if (ctx.peek() == '"')
		{
			return parse_quoted(ctx);
		}

		size_t start = ctx.pos;

		ctx.advance_if([&]{ return !ctx.is_unimportant(); });

		return std::string{ ctx.str.substr(start, ctx.pos-start) };
	}

	std::vector<Cmd> parse(std::string_view source)
	{
		std::vector<Cmd> output;

		ParserCtx ctx(source);

		while (!ctx.at_end())
		{
			Cmd cmd;

			auto opt = parse_arg(ctx);

			if (!opt)
			{
				return {};
			}

			cmd.cmd = std::move(opt.value());

			while (!ctx.at_end() && ctx.peek() != '\n')
			{
				auto arg = parse_arg(ctx);

				if (!arg)
				{
					break;
				}

				cmd.args.push_back(std::move(arg.value()));
			}

			output.push_back(std::move(cmd));
		}

		return output;
	}
}