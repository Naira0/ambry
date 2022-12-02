#include "db.hpp"

#include <limits>
#include <sys/stat.h>
#include <sys/file.h>

#include <cstdio>
#include <cctype>
#include <cassert>
#include <cstring>

#include <charconv>
#include <filesystem>

#include "../fmt.hpp"

#define MALFORMEDIDX(message) {ResultType::MalformedIdx, message}

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
        return set_bytes(key, (uint8_t*)value.data(), value.size());
    }

    Result DB::set_bytes(std::string_view key, const uint8_t *bytes, uint32_t size)
    {
        auto iter = m_context.index.emplace(key, IndexData{});

        if (!iter.second)
            return {ResultType::KeyNotInserted, "Could not insert key into index"};

        size_t offset = m_data.write(bytes, size);

        m_io_manager.update(0, {
            key,
            offset,
            size,
            true,
            true,
        });

        IndexData &data = iter.first->second;

        data.offset = offset;
        data.length = size;

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

        return {};
    }
}