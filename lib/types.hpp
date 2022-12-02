#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>
#include <variant>
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

     struct IndexData
    {
        size_t offset;
        uint32_t length;
        // the offset of the raw record in the .idx file
        uint32_t idx_offset;
    };

	enum class FlushMode : uint8_t
	{
		ConstantFlush,
		FlushAtEnd,
        FlushPeriodically,
	};

	struct Options
	{
		FlushMode flush_node = FlushMode::FlushAtEnd;
	};

    // important shared data
    struct DBContext
    {
        std::unordered_map<std::string, IndexData> index;
        std::vector<uint8_t> data;
        std::map<size_t, uint32_t> free_list;
        Options options;
        std::string name;

        DBContext() = default;
    };
}