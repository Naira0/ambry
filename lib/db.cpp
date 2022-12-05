#include "db.hpp"

#include <limits>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/file.h>

#include <cstdio>
#include <cctype>
#include <cassert>
#include <cstring>

#include <charconv>
#include <filesystem>

#include "../fmt.hpp"
#include "transaction.hpp"
#include "types.hpp"

namespace ambry
{

    Result DB::open()
    {
        return m_io_manager.load_structures();
    }

    std::optional<std::string_view>
    DB::get(const std::string &key)
    {
        auto iter = m_context.index.find(key);

        if (iter == m_context.index.end())
            return {};

        IndexData data = iter->second;

        auto cache = m_context.data.data() + data.offset;

        return std::string_view{(char*)cache, data.length};
    }

    Result DB::set(std::string_view key, std::string_view value)
    {
        auto iter = m_context.index.emplace(key, IndexData{});

        if (!iter.second)
            return {ResultType::KeyNotInserted, "Could not insert key into index"};

        uint32_t size = value.size();
        size_t offset = m_data.write(value.data(), size);

        IndexData &data = iter.first->second;

        data.offset = offset;
        data.length = size;

        m_io_manager.insert(*iter.first);

        return {};
    }

    Result DB::erase(const std::string &key)
    {
        auto iter = m_context.index.find(key);

        if (iter == m_context.index.end())
            return {ResultType::KeyNotFound, "key does not exist in index"};
        
        IndexData data = iter->second;

        m_context.index.erase(iter);

        m_data.free(data.offset, data.length);

        m_io_manager.erase(*iter);

        return {};
    }

    Result DB::update(const std::string &key, std::string_view value)
    {
        auto iter = m_context.index.find(key);

        if (iter == m_context.index.end())
            return {ResultType::KeyNotFound, "key does not exist in index"};

        IndexData &data = iter->second;

        size_t size = value.size();

        data.length = size;

        if (size < data.length)
        {
            m_data.free(data.offset + size, data.length-size);
            m_data.write_at(data.offset, value.data(), size);

            data.offset = data.offset;
        }
        else
        {
            m_data.free(data.offset, data.length);
            data.offset = m_data.write(value.data(), size);
        }

        // this should ideally not be called here as its not the databases responsibility but skill issue
        if (m_context.options.flush_mode == FlushMode::Constant)
		{
			m_io_manager.write_dat(data.offset, size);
		}
		else
		{
			m_context.changelog.emplace(data.offset, size);
		}

        m_io_manager.update(*iter);

        return {};
    }

    Transaction DB::begin_transaction()
    {
        return Transaction(*this);
    }

    DB::Iterator DB::begin()
    {
        return Iterator(m_context.index.begin(), *this);
    }

    DB::Iterator DB::end()
    {
        return Iterator(m_context.index.end(), *this);
    }

    inline size_t DB::size() const
    {
        return m_context.index.size();
    }

    std::string_view DB::operator[](const std::string &key)
    {
        auto opt = get(key);

        if (!opt.has_value())
            throw std::out_of_range("key does not exist in database index");

        return opt.value();
    }

    bool DB::contains(const std::string &key) const
    {
        return m_context.index.contains(key);
    }

    void DB::flush()
    {
        m_io_manager.flush_changelog();
    }

    void DB::set_flush_mode(FlushMode flush_mode)
    {
        if (flush_mode == FlushMode::Periodic)
        {
            m_io_manager.launch_timed_flush();
        }
        else if (flush_mode == FlushMode::Constant)
        {
            m_io_manager.flush_changelog();
            m_context.data.clear();
            m_context.data.shrink_to_fit();
        }

        m_context.options.flush_mode = flush_mode;
    }

    void DB::set_flush_time(int ms)
    {
        m_context.options.flush_time = ms;
    }
}