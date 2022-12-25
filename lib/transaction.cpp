#include "transaction.hpp"
#include "types.hpp"

namespace ambry
{
	Transaction& Transaction::set(std::string_view key, std::string_view value)
	{
		m_cmds.emplace_back(Command{CmdType::Set, key, value});
		return *this;
	}

	Transaction& Transaction::update(const std::string &key, std::string_view value)
	{
		m_cmds.emplace_back(Command{CmdType::Update, key, value});
		return *this;
	}

	Transaction& Transaction::erase(const std::string &key)
	{
		m_cmds.emplace_back(Command{CmdType::Erase, key, ""});
		return *this;
	}

	Result Transaction::commit()
	{
		#define HANDLE(fn)     \
		result = fn;     \
		if (!result.ok())  \
			return result; \
		
		Result result;

		for (auto command : m_cmds)
		{
			switch (command.type)
			{
				case CmdType::Set:
				{
					HANDLE(m_db.set(command.a1, command.a2));
					break;
				}
				case CmdType::Update:
				{
					HANDLE(m_db.update(std::string{command.a1}, command.a2));
					break;
				}
				case CmdType::Erase:
				{
					HANDLE(m_db.erase(std::string{command.a1}));
					break;
				}
			}
		}

		return {};

		#undef HANDLE
	}
}