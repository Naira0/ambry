#pragma once

#include <cstdint>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <map>
#include <vector>
#include <string>

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
        ParseError,
        InterpretError,
    };

    template<class T>
    struct BasicResult
    {
        ResultType type = ResultType::Ok;
        T message;

        [[nodiscard]]
        inline bool ok() const
        {
            return type == ResultType::Ok;
        }
    };

    using Result = BasicResult<std::string_view>;

    struct IndexData
    {
        uint64_t offset;
        uint32_t length;
        // the offset of the raw record in the .idx file
        uint32_t idx_offset;
    };

    struct FreeEntry
    {
        uint64_t offset;
        uint32_t free_list_offset;
    };

	struct Options
	{
        bool enable_cache = false;
	};

    // important shared data
    struct DBContext
    {
        std::unordered_map<std::string, IndexData> index;
        std::vector<uint8_t> data;
        std::multimap<uint32_t, FreeEntry> free_list;
        Options options;
        std::string name;
        
        DBContext() = default;

        DBContext(DBContext &&ctx) :
            index(std::move(ctx.index)),
            data(std::move(ctx.data)),
            free_list(std::move(ctx.free_list)),
            options(ctx.options),
            name(std::move(ctx.name))
        {}

        DBContext(const DBContext &ctx) :
            index(ctx.index),
            data(ctx.data),
            free_list(ctx.free_list),
            options(ctx.options),
            name(ctx.name)
        {}
    };
}