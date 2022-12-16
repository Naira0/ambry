#include "db.hpp"

#include <stdexcept>
#include <cassert>

#include "transaction.hpp"
#include "types.hpp"

namespace ambry
{
    Result DB::open()
    {
        return m_io_manager.load_structures();
    }

    void DB::close()
    {
        m_io_manager.cleanup();

        m_context.data.clear();
        m_context.data.shrink_to_fit();

		m_context.index.clear();
		m_context.free_list.clear();
    }

    Result DB::destroy()
    {
        return m_io_manager.destroy();
    }

    std::optional<std::string_view>
    DB::get_cached(const std::string &key)
    {
        assert(m_context.options.enable_cache && "caching must be turned on to use this function");

        auto iter = m_context.index.find(key);

        if (iter == m_context.index.end())
            return {};

        IndexData data = iter->second;

        auto cache = m_context.data.data() + data.offset;

        return std::string_view{(char*)cache, data.length};
    }

    std::optional<std::string>
    DB::get(const std::string &key)
    {
        auto iter = m_context.index.find(key);

        if (iter == m_context.index.end())
            return {};

        IndexData data = iter->second;
        
        return m_io_manager.read_dat(data.offset, data.length);
    }

    Result DB::set(std::string_view key, std::string_view value)
    {
        auto [pair, emplaced] = m_context.index.emplace(key, IndexData{});

        if (!emplaced)
            return {ResultType::KeyNotInserted, "Could not insert key into index"};

        size_t offset = m_rw.write(value);

        IndexData &data = pair->second;

        data.offset = offset;
        data.length = value.size();

        m_io_manager.insert(*pair);

        return {};
    }

    Result DB::erase(const std::string &key)
    {
        auto iter = m_context.index.find(key);

        if (iter == m_context.index.end())
            return {ResultType::KeyNotFound, "key does not exist in index"};
        
        IndexData data = iter->second;

        m_context.index.erase(iter);

        m_rw.free(data.offset, data.length);

        m_io_manager.erase(*iter);

        return {};
    }

    Result DB::update(const std::string &key, std::string_view value)
    {
        auto iter = m_context.index.find(key);

        if (iter == m_context.index.end())
            return {ResultType::KeyNotFound, "key does not exist in index"};

        IndexData &data = iter->second;

        data.offset = m_rw.update(data.offset, data.length, value);
        data.length = value.size();

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

    size_t DB::size() const
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

    void DB::reserve(size_t size)
    {
        m_context.data.reserve(size);
    }

    void DB::switch_cache(bool on)
    {
        if (on)
        {
            m_context.options.enable_cache = true;
            m_io_manager.load_dat();
        }
        else
        {
            m_context.data.clear();
            m_context.data.shrink_to_fit();
        }
    }

    std::string_view DB::name() const
    {
        return m_context.name;
    }

    bool DB::is_cached() const
    {
        return m_context.options.enable_cache;
    }
}