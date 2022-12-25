#pragma once

#include <string>

#include "cache.hpp"
#include "io_manager.hpp"
#include "types.hpp"
#include "transaction.hpp"
#include "rw.hpp"
#include <iostream>

namespace ambry
{
    class DB
    {
    public:

        class Iterator;

        DB(std::string_view name, Options options = {}) :
            m_im(m_ctx),
            m_rw(m_ctx, m_im)
        {
            m_ctx.name = name;
            m_ctx.options = options;
        }

        DB(DB &&db) :
            m_ctx(std::move(db.m_ctx)),
            m_im(std::move(db.m_im), m_ctx),
            m_rw(std::move(db.m_rw), m_ctx, m_im)
        {}

        DB(const DB &db) :
            m_ctx(db.m_ctx),
            m_im(db.m_im),
            m_rw(db.m_rw)
        {}

        Result open();

        void close();

        Result destroy();

        Result set(std::string_view key, std::string_view value);

        Result update(const std::string &key, std::string_view value);

        std::optional<std::string_view> 
        get_cached(const std::string &key);

        std::optional<std::string>
        get(const std::string &key);

        Result erase(const std::string &key);

        Transaction begin_transaction();

        Iterator begin();

        Iterator end();

        size_t size() const;

        std::string_view operator[](const std::string &key);

        bool contains(const std::string &key) const;

        void reserve(size_t size);

        void switch_cache(bool on);

        std::string_view name() const;
        
        bool is_cached() const;

    private:
        friend Iterator;

        DBContext m_ctx;
        IoManager m_im;
        RW m_rw;

        Result read_index();
        Result read_data();

        Result set_bytes(std::string_view key, const uint8_t *bytes, uint32_t size);

    public:
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
                if (m_db.m_ctx.options.enable_cache)
                {
                    return {m_iter->first, m_db.get_cached(m_iter->first).value()};
                }
                else
                {
                    auto value = m_cached_strings.emplace_back(m_db.get(m_iter->first).value());
                    return {m_iter->first, value};
                }
            }

        private:
            std::unordered_map<std::string, IndexData>::iterator
            m_iter;
            std::vector<std::string> m_cached_strings;
            DB &m_db;
        };
    };
}