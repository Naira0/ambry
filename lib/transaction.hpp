#pragma once

#include "types.hpp"

namespace ambry
{
	class DB;

	class Transaction
	{
		enum class CommandType
		{
			Set, Update, Erase,
		};

		struct Command
		{
			CommandType type;
			std::string_view a1;
			std::string_view a2;
		};

	public:
		Transaction(DB &db) :
			m_db(db)
		{}

        Transaction& set(std::string_view key, std::string_view value);

        Transaction& update(const std::string &key, std::string_view value);

        Transaction& erase(const std::string &key);

		Result commit();

	private:
		DB &m_db;
		std::vector<Command> m_commands;
	};
}

#include "db.hpp"