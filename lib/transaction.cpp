#include "transaction.hpp"
#include "types.hpp"

namespace ambry
{
	Transaction& Transaction::set(std::string_view key, std::string_view value)
	{
		m_commands.emplace_back(Command{CommandType::Set, key, value});
		return *this;
	}

	Transaction& Transaction::update(const std::string &key, std::string_view value)
	{
		m_commands.emplace_back(Command{CommandType::Update, key, value});
		return *this;
	}

	Transaction& Transaction::erase(const std::string &key)
	{
		m_commands.emplace_back(Command{CommandType::Erase, key, ""});
		return *this;
	}

	Result Transaction::commit()
	{
		#define HANDLE(fn)     \
		result = fn;     \
		if (!result.ok())  \
			return result; \
		
		Result result;

		for (auto command : m_commands)
		{
			switch (command.type)
			{
				case CommandType::Set:
				{
					HANDLE(m_db.set(command.a1, command.a2));
					break;
				}
				case CommandType::Update:
				{
					HANDLE(m_db.update(std::string{command.a1}, command.a2));
					break;
				}
				case CommandType::Erase:
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