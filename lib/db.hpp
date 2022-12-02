#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>

#include "cache.hpp"
#include "io_manager.hpp"
#include "types.hpp"

namespace ambry
{
    class DB
    {
       
    public:

        DB(std::string_view name, Options options = {}) :
            m_data(m_context),
            m_io_manager(m_context)
        {
            m_context.name = name;
            m_context.options = options;
        }

        Result open();

        Result set(std::string_view key, std::string_view value);

        // returns the value by its appropriate type. if the key was not found its index will be Type::None
        std::optional<std::string_view> 
        get(const std::string &key);

        Result erase(const std::string &key);

    public:
        DBContext m_context;
        IoManager m_io_manager;
        Cache m_data;

        Result read_index();
        Result read_data();

        Result set_bytes(std::string_view key, const uint8_t *bytes, uint32_t size);

    };
}