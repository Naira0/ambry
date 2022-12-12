#pragma once

#include <cstdint>
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
    };
}