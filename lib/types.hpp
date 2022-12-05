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
        size_t offset;
        uint32_t length;
        // the offset of the raw record in the .idx file
        uint32_t idx_offset;
    };

	enum class FlushMode : uint8_t
	{
		Constant,
		OnClose,
        Periodic,
        Never,
	};

	struct Options
	{
		FlushMode flush_mode = FlushMode::OnClose;
        
        bool disable_cache   = false;
        bool async_flush     = true;

        // periodic flush time
        int flush_time = 60'000;
	};

    // important shared data
    struct DBContext
    {
        std::unordered_map<std::string, IndexData> index;
        std::vector<uint8_t> data;
        std::map<size_t, uint32_t> free_list;
        Options options;
        std::string name;
        std::map<size_t, size_t> changelog;

        DBContext() = default;
    };
}