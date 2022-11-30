#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <variant>

#include "cache.hpp"

namespace ambry
{
    enum class ResultType : uint8_t
    {
        Ok,
        IoFailure,
        MalformedIdx,
        MalformedDat,
        KeyNotInserted,
        EraseFailure,
        KeyNotFound,
    };

    struct Result
    {
        ResultType type = ResultType::Ok;
        std::string_view message;

        [[nodiscard]]
        inline bool ok() const
        {
            return type == ResultType::Ok;
        }
    };

    enum Type : uint8_t
    {
        None, String, Int
    };

    struct IndexData
    {
        size_t 
            offset,
            length;
        Type type;
    };

    using Value = std::variant<
        std::monostate, 
        std::string_view, 
        int>;

    class DB
    {
    public:

        DB(std::string_view name) :
                m_name(name)
        {}

        ~DB();

        Result open();

        Result set(std::string_view key, std::string_view value);
        Result set(std::string_view key, int value);

        // returns the value by its appropriate type. if the key was not found its index will be Type::None
        Value get(const std::string &key);

        Result erase(const std::string &key);

    public:
        std::string m_name;

        FILE 
            *m_fidx, 
            *m_fdat;

        std::unordered_map<std::string, IndexData> m_index;
        Cache m_data;

        Result read_index();
        Result read_data();

        Result set_bytes(std::string_view key, const uint8_t *bytes, size_t size, Type type);
    };
}