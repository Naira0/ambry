#pragma once

// an object responsible for managing the runtime database cache

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <vector>
#include <map>
#include <optional>

namespace ambry
{
	class Cache
	{
	public:

		Cache() = default;

		Cache(size_t mem_size)
		{
			m_data.reserve(mem_size);
		}

		void reserve(size_t size);

		// writes n bytes to cache and returns the starting offset of the bytes written
		size_t write(const uint8_t *bytes, size_t size);

		// writes at the end of the cache and returns the starting offset
		size_t write_back(const uint8_t *bytes, size_t size);

		// writes at a specific offset in the cache
		void write_at(size_t offset, const uint8_t *bytes, size_t size);
		
		// returns a pointer from to the given offset in the cache
		uint8_t* read(size_t offset);

		// returns the raw data of the cache
		uint8_t* data();

		// writes to the cache from a C file stream
		void write_from_stream(FILE *stream, size_t size);

		inline constexpr size_t	
		size() const;

		void free(size_t offset, size_t size);

	public:
		std::vector<uint8_t> m_data;
		std::map<size_t, size_t> m_free_list;

		std::optional<std::pair<size_t, size_t>> 
		find_free(size_t size);
	};
}