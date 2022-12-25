#include "db.hpp"

#include <stdexcept>
#include <cassert>

#include "transaction.hpp"
#include "types.hpp"

namespace ambry
{
    Result DB::open()
    {
        return m_im.load_structures();
    }

    void DB::close()
    {
        m_im.cleanup();

        m_ctx.data.clear();
        m_ctx.data.shrink_to_fit();

		m_ctx.index.clear();
		m_ctx.free_list.clear();
    }

    Result DB::destroy()
    {
        return m_im.destroy();
    }

    std::optional<std::string_view>
    DB::get_cached(const std::string &key)
    {
        assert(m_ctx.options.enable_cache && "caching must be turned on to use this function");

        auto iter = m_ctx.index.find(key);

        if (iter == m_ctx.index.end())
            return {};

        IndexData data = iter->second;

        auto cache = m_ctx.data.data() + data.offset;

        return std::string_view{(char*)cache, data.length};
    }

    std::optional<std::string>
    DB::get(const std::string &key)
    {
        auto iter = m_ctx.index.find(key);

        if (iter == m_ctx.index.end())
            return {};

        IndexData data = iter->second;
        
        return m_im.read_dat(data.offset, data.length);
    }

    Result DB::set(std::string_view key, std::string_view value)
    {
        auto [pair, emplaced] = m_ctx.index.emplace(key, IndexData{});

        if (!emplaced)
            return {ResultType::KeyNotInserted, "Could not insert key into index"};

        size_t offset = m_rw.write(value);

        IndexData &data = pair->second;

        data.offset = offset;
        data.length = value.size();

        m_im.insert(*pair);

        return {};
    }

    Result DB::erase(const std::string &key)
    {
        auto iter = m_ctx.index.find(key);

        if (iter == m_ctx.index.end())
            return {ResultType::KeyNotFound, "key does not exist in index"};
        
        IndexData data = iter->second;

        m_ctx.index.erase(iter);

        m_rw.free(data.offset, data.length);

        m_im.erase(*iter);

        return {};
    }

    Result DB::update(const std::string &key, std::string_view value)
    {
        auto iter = m_ctx.index.find(key);

        if (iter == m_ctx.index.end())
            return {ResultType::KeyNotFound, "key does not exist in index"};

        IndexData &data = iter->second;

        data.offset = m_rw.update(data.offset, data.length, value);
        data.length = value.size();

        m_im.update(*iter);

        return {};
    }

    Transaction DB::begin_transaction()
    {
        return Transaction(*this);
    }

    DB::Iterator DB::begin()
    {
        return Iterator(m_ctx.index.begin(), *this);
    }

    DB::Iterator DB::end()
    {
        return Iterator(m_ctx.index.end(), *this);
    }

    size_t DB::size() const
    {
        return m_ctx.index.size();
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
        return m_ctx.index.contains(key);
    }

    void DB::reserve(size_t size)
    {
        m_ctx.data.reserve(size);
    }

    void DB::switch_cache(bool on)
    {
        if (on)
        {
            m_ctx.options.enable_cache = true;
            m_im.load_dat();
        }
        else
        {
            m_ctx.data.clear();
            m_ctx.data.shrink_to_fit();
        }
    }

    std::string_view DB::name() const
    {
        return m_ctx.name;
    }

    bool DB::is_cached() const
    {
        return m_ctx.options.enable_cache;
    }
}