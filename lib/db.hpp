#pragma once

#include <string>

#include "cache.hpp"
#include "io_manager.hpp"
#include "types.hpp"
#include "transaction.hpp"

namespace ambry
{
    class DB
    {
    public:

        class Iterator;

        DB(std::string_view name, Options options = {}) :
            m_io_manager(m_context),
            m_data(m_context, m_io_manager)
        {
            m_context.name = name;
            m_context.options = options;
        }

        Result open();

        Result set(std::string_view key, std::string_view value);

        Result update(const std::string &key, std::string_view value);

        std::optional<std::string_view> 
        get(const std::string &key);

        Result erase(const std::string &key);

        Transaction begin_transaction();

        Iterator begin();

        Iterator end();

        inline size_t size() const;

        std::string_view operator[](const std::string &key);

        bool contains(const std::string &key) const;

    public:
        DBContext m_context;
        IoManager m_io_manager;
        Cache m_data;

        Result read_index();
        Result read_data();

        Result set_bytes(std::string_view key, const uint8_t *bytes, uint32_t size);

        class Iterator
        {
        public:
            Iterator(auto iter, DB &db) :
                m_iter(iter),
                m_db(db)
            {}

            Iterator& operator++()
            {
                m_iter++;
                return *this;
            }

            Iterator operator++(int)
            {
                Iterator temp = *this;
                m_iter++;
                return temp;
            }

            bool operator==(Iterator other)
            {
                return m_iter == other.m_iter;
            }

            bool operator!=(Iterator other)
            {
                return m_iter != other.m_iter;
            }

            std::pair<std::string_view, std::string_view> 
            operator*()
            {
                return {m_iter->first, m_db.get(m_iter->first).value_or("no key found")};
            }

        private:
            std::unordered_map<std::string, IndexData>::iterator 
            m_iter;
            DB &m_db;
        };
    };
}