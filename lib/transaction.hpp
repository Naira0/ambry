#pragma once

#include "types.hpp"

namespace ambry
{
	class DB;

	class Transaction
	{
		enum class CmdType
		{
			Set, Update, Erase,
		};

		struct Command
		{
			CmdType type;
			std::string_view a1;
			std::string_view a2;
		};

	public:
		Transaction(DB &db) :
			m_db(db)
		{}

		Transaction(Transaction &&tr) :
			m_db(tr.m_db),
			m_cmds(std::move(tr.m_cmds))
		{}

		Transaction(const Transaction &tr) :
			m_db(tr.m_db),
			m_cmds(tr.m_cmds)
		{}

        Transaction& set(std::string_view key, std::string_view value);

        Transaction& update(const std::string &key, std::string_view value);

        Transaction& erase(const std::string &key);

		Result commit();

	private:
		DB &m_db;
		std::vector<Command> m_cmds;
	};
}

#include "db.hpp"
