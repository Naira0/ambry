#include "aci.hpp"
#include "../lib/db.hpp"
#include "../lib/types.hpp"
#include <cstddef>
#include <iostream>
#include <optional>
#include <cstring>

#include "../shared/fmt.hpp"

#define INTER_ERR(msg) {ambry::ResultType::InterpretError, msg}
#define KEY_NOT_FND {ambry::ResultType::InterpretError, "there is no database by that name"}
#define TO_RES(v) Result{v.type, std::string(v.message)}

namespace aci
{
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

		auto res = m ? ctx.inter.wdb->set(key, args) : ctx.inter.wdb->update(key, args);

		return TO_RES(res);
	}

	Result open_cb(Ctx &ctx)
	{
		const std::string &name = ctx.cmd.args.front();

		auto [iter, emplaced] = ctx.inter.dbt.emplace(name, ambry::DB(name));
		
		if (!emplaced)
		{
			return INTER_ERR("a database with that name already exists");
		}

		ctx.inter.wdb = &iter->second;

		auto res = ctx.inter.wdb->open();

		return TO_RES(res);
	}

	Result close_cb(Ctx &ctx)
	{
		auto iter = ctx.inter.dbt.find(ctx.cmd.args.front());

		if (iter == ctx.inter.dbt.end())
		{
			return KEY_NOT_FND;
		}

		iter->second.close();

		if (&iter->second == ctx.inter.wdb)
		{
			ctx.inter.wdb = nullptr;
		}

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

		ctx.inter.wdb = &iter->second;

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

		ctx.inter.wdb->switch_cache(on);

		return {};
	}

	Result set_cb(Ctx &ctx)
	{
		return update_set_impl(ctx, 1);
	}

	Result get_cb(Ctx &ctx)
	{
		const std::string &key = ctx.cmd.args.front();

		if (ctx.inter.wdb->is_cached())
		{
			auto opt = ctx.inter.wdb->get_cached(key);

			if (!opt)
			{
				return {ambry::ResultType::KeyNotFound, "key not found"};
			}

			return {ambry::ResultType::Ok, std::string{ opt.value() }};
		}
		else
		{
			auto opt = ctx.inter.wdb->get(key);

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

		ctx.inter.wdb = nullptr;
		ctx.inter.dbt.clear();

		return {};
	}

	Result destroy_cb(Ctx &ctx)
	{
		auto iter = ctx.inter.dbt.find(ctx.cmd.args.front());

		if (iter == ctx.inter.dbt.end())
		{
			return KEY_NOT_FND;
		}

		if (&iter->second == ctx.inter.wdb)
		{
			ctx.inter.wdb = nullptr;
		}

		return TO_RES(iter->second.destroy());
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

		ctx.inter.wdb = nullptr;

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
		auto res = ctx.inter.wdb->erase(ctx.cmd.args.front());
		return TO_RES(res);
	}

	Result list_open_cb(Ctx &ctx)
	{
		std::string output;

		output += "name - size";

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

		for (; iter != ctx.cmd.args.end(); iter++)
		{
			if (iter->size() > 255)
			{
				return INTER_ERR("role length must be less then 256 chars");
			}

			buff += (uint8_t)iter->size();
			buff += *iter;
		}

		auto res = ctx.inter.users.set(username, buff);

		return TO_RES(res);
	}

	Result create_role_cb(Ctx &ctx)
	{
		auto iter = ctx.cmd.args.begin();

		std::string &name = *iter++;

		if (name.size() > 256 )
		{
			return INTER_ERR("role length must be less then 256 chars");
		}

		std::string buff;

		constexpr size_t size = sizeof(FlagT);

		buff.resize(ctx.cmd.args.size() * size);

		for (size_t off = 0; iter != ctx.cmd.args.end(); iter++)
		{
			auto flag = perm_table.find(*iter);

			if (flag == perm_table.end())
			{
				return INTER_ERR("invalid flag name provided");
			}

			memcpy(buff.data()+off, (char*)&flag->second, size);

			off += size;
		}

		auto res = ctx.inter.users.set(name, buff);

		return TO_RES(res);
	}

	std::string_view get_field(const std::string &str, size_t off)
	{
		uint8_t len = str[off];
		return { str.begin()+off, str.begin()+len };
	}

	Result login_cb(Ctx &ctx)
	{
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
			if (ctx.inter.roles.contains(*iter))
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

			if (!roles.contains(role))
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
	
	void Interpreter::init_commands()
	{
		ct["create_user"] = 
		{
			.arity = 2,
			.description = "creates a new user account",
			.usage = " <user name> <password> [role] ...",
			.fn = create_user_cb,
			.expect_wdb = false,
			.perms = ADMIN,
		};

		ct["create_role"] = 
		{
			.arity = 2,
			.description = "creates a new role with permissions",
			.usage = " <role name> <permission> ...",
			.fn = create_role_cb,
			.expect_wdb = false,
		};

		ct["login"] = 
		{
			.arity = 2,
			.description = "login command",
			.usage = " <username> <password>",
			.fn = login_cb,
			.expect_wdb = false,
		};

		ct["add_roles"] = 
		{
			.arity = 2,
			.description = "adds any number of roles to a provided user",
			.usage = " <username> <role> ...",
			.fn = add_roles_cb,
			.expect_wdb = false,
		};

		ct["remove_roles"] = 
		{
			.arity = 2,
			.description = "removes any number of roles to a provided user",
			.usage = " <username> <role> ...",
			.fn = remove_roles_cb,
			.expect_wdb = false,
		};

		ct["open"] = 
		{
			.arity = 1,
			.description = "opens a database",
			.usage = " <db_name>",
			.fn = open_cb,
			.expect_wdb = false,
		};

		ct["close"] = 
		{
			.arity = 1,
			.description = "closes a database or the working database if only one db is open",
			.usage = " <db_name>",
			.fn = close_cb,
			.expect_wdb = false,
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
		};

		ct["set"] = 
		{
			.arity = -1,
			.description = "sets a value to the working database. any additional values will be concatenated together",
			.usage = " <key> <value> <...>",
			.fn = set_cb,
		};

		ct["update"] = 
		{
			.arity = -1,
			.description = "updates a value to the working database. any additional values will be concatenated together",
			.usage = " <key> <value> <...>",
			.fn = update_cb,
		};

		ct["get"] = 
		{
			.arity = 1,
			.description = "gets a value from the workind database from the given key",
			.usage = " <key>",
			.fn = get_cb,
		};

		ct["erase"] = 
		{
			.arity = 1,
			.description = "erases a value from the workind database from the given key",
			.usage = " <key>",
			.fn = erase_cb,
		};

		ct["close_all"] = 
		{
			.arity = 0,
			.description = "closes all open databases",
			.usage = "",
			.fn = close_all_cb,
		};

		ct["destroy"] = 
		{
			.arity = 1,
			.description = "destroys a database by its name",
			.usage = " <db_name>",
			.fn = destroy_cb,
		};

		ct["destroy_all"] = 
		{
			.arity = 0,
			.description = "destroys all open databases",
			.usage = "",
			.fn = destroy_all_cb,
		};

		ct["cmds"] = 
		{
			.arity = 0,
			.description = "displays all commands",
			.usage = "",
			.fn = cmds_cb,
			.expect_wdb = false,
		};

		ct["list_open"] = 
		{
			.arity = 0,
			.description = "displays all open databases",
			.usage = "",
			.fn = list_open_cb,
			.expect_wdb = false,
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

		auto opt = users.get(iter->second);

		if (!opt)
		{
			return false;
		}

		std::string &data = opt.value();

		size_t offset = data[0]+1;

		std::set<FlagT> looking;

		while (offset < data.size())
		{
			std::string_view role_name = get_field(data, offset);
			offset += role_name.size()+1;

			auto opt = roles.get(std::string{role_name});

			if (!opt)
			{
				return false;
			}

			FlagT flag;

			memcpy(opt.value().data(), (char*)&flag, sizeof(FlagT));

			for (auto [_, perm] : perm_table)
			{
				if ((needed & perm) && looking.contains(flag))
				{
					looking.erase(flag);
				}
				else if ((needed & perm) && !(flag & perm))
				{
					looking.emplace(perm);
				}
			}
		}

		return looking.empty();
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

		if (ch.expect_wdb && !wdb)
		{
			return INTER_ERR("expected an open database but none found");
		}

		Ctx ctx
		{
			*this,
			cmd,
			from,
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