#include "aci.hpp"
#include "db.hpp"
#include "types.hpp"
#include <cstddef>
#include <iostream>
#include <optional>

#include "fmt.hpp"

#define INTER_ERR(msg) {ambry::ResultType::InterpretError, msg}
#define KEY_NOT_FND {ambry::ResultType::InterpretError, "there is no database by that name"}

namespace aci
{
	ambry::Result update_set_impl(Interpreter &inter, Cmd &cmd, uint8_t m)
	{
		auto iter = cmd.args.begin();

		const std::string &key = *iter;

		std::string args;

		iter++;

		for (; iter != cmd.args.end(); iter++)
		{
			args += std::move(*iter);
		}

		return m ? inter.wdb->set(key, args) : inter.wdb->update(key, args);
	}

	ambry::Result open_cb(Interpreter &inter, Cmd &cmd)
	{
		const std::string &name = cmd.args.front();

		auto [iter, emplaced] = inter.dbt.emplace(name, ambry::DB(name));
		
		if (!emplaced)
		{
			return INTER_ERR("a database with that name already exists");
		}

		inter.wdb = &iter->second;

		return inter.wdb->open();
	}

	ambry::Result close_cb(Interpreter &inter, Cmd &cmd)
	{
		auto iter = inter.dbt.find(cmd.args.front());

		if (iter == inter.dbt.end())
		{
			return KEY_NOT_FND;
		}

		iter->second.close();

		if (&iter->second == inter.wdb)
		{
			inter.wdb = nullptr;
		}

		inter.dbt.erase(iter);

		return {};
	}

	ambry::Result switch_cb(Interpreter &inter, Cmd &cmd)
	{
		auto iter = inter.dbt.find(cmd.args.front());

		if (iter == inter.dbt.end())
		{
			return KEY_NOT_FND;
		}

		inter.wdb = &iter->second;

		return {};
	}

	ambry::Result cache_mode_cb(Interpreter &inter, Cmd &cmd)
	{
		const std::string &s = cmd.args.front();

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

		inter.wdb->switch_cache(on);

		return {};
	}

	ambry::Result set_cb(Interpreter &inter, Cmd &cmd)
	{
		return update_set_impl(inter, cmd, 1);
	}

	ambry::Result get_cb(Interpreter &inter, Cmd &cmd)
	{
		const std::string &key = cmd.args.front();

		if (inter.wdb->is_cached())
		{
			auto opt = inter.wdb->get_cached(key);

			if (!opt)
			{
				return {ambry::ResultType::KeyNotFound, "key not found"};
			}

			return {ambry::ResultType::Ok, opt.value()};
		}
		else
		{
			auto opt = inter.wdb->get(key);

			if (!opt)
			{
				return {ambry::ResultType::KeyNotFound, "key not found"};
			}

			auto [s, _] = inter.cache.emplace(opt.value());

			return {ambry::ResultType::Ok, *s};
		}
	}

	ambry::Result close_all_cb(Interpreter &inter, Cmd &cmd)
	{
		for (auto &[_, db] : inter.dbt) 
		{
			db.close();
		}

		inter.wdb = nullptr;
		inter.dbt.clear();

		return {};
	}

	ambry::Result destroy_cb(Interpreter &inter, Cmd &cmd)
	{
		auto iter = inter.dbt.find(cmd.args.front());

		if (iter == inter.dbt.end())
		{
			return KEY_NOT_FND;
		}

		if (&iter->second == inter.wdb)
		{
			inter.wdb = nullptr;
		}

		return iter->second.destroy();
	}

	ambry::Result destroy_all_cb(Interpreter &inter, Cmd &cmd)
	{

		for (auto &[_, db] : inter.dbt)
		{
			ambry::Result result = db.destroy();

			if (!result.ok())
			{
				return result;
			}
		}

		inter.dbt.clear();

		inter.wdb = nullptr;

		return {};
	}

	ambry::Result cmds_cb(Interpreter &inter, Cmd &cmd)
	{
		for (auto [cmd, ch] : inter.ct)
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

			fmt::println("- {}({}):\n\tDescription: {}\n\tUsage: {}{}",

			cmd, arg_count, ch.description, cmd, ch.usage);
		}

		return {};
	}

	ambry::Result update_cb(Interpreter &inter, Cmd &cmd)
	{
		return update_set_impl(inter, cmd, 0);
	}

	ambry::Result erase_cb(Interpreter &inter, Cmd &cmd)
	{
		return inter.wdb->erase(cmd.args.front());
	}

	ambry::Result list_open_cb(Interpreter &inter, Cmd &cmd)
	{
		fmt::println("name - size");

		for (auto &[_, db] : inter.dbt)
		{
			fmt::println("{} - {}", db.name(), db.size());
		}

		return {};
	}

	void Interpreter::init_commands()
	{
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
			.usage = " [db_name]",
			.fn = close_cb,
			.expect_wdb = false,
		};

		ct["switch"] = 
		{
			.arity = 1,
			.description = "switches the current database to one with the give arguments name",
			.usage = " [db_name]",
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

	ambry::Result Interpreter::interpret(Cmd &cmd)
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

		return ch.fn(*this, cmd);
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
			return c == ' ' || c == '\n' || c == '\r';
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

		if (at_end)
		{
			return std::string{};
		}

		if (ctx.peek() == '"')
		{
			return parse_quoted(ctx);
		}

		size_t start = ctx.pos;

		ctx.advance_if([&]{ return !ctx.is_unimportant(); });

		return std::string{ ctx.str.substr(start, ctx.pos-start) };
	}

	std::optional<Cmd> parse(std::string_view source)
	{
		if (source.empty())
		{
			return std::nullopt;
		}

		Cmd cmd;

		ParserCtx ctx(source);

		auto opt = parse_arg(ctx);

		if (!opt)
		{
			return std::nullopt;
		}

		cmd.cmd = std::move(opt.value());

		while (!ctx.at_end())
		{
			auto arg = parse_arg(ctx);

			if (!arg)
			{
				return std::nullopt;
			}

			cmd.args.push_back(std::move(arg.value()));
		}

		return cmd;
	}
}