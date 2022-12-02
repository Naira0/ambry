#pragma once

// an object responsible for managing the runtime database cache

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <vector>
#include <map>
#include <optional>

#include "types.hpp"

namespace ambry
{
	class Cache
	{
	public:

		Cache(DBContext &context) :
			m_context(context)
		{}

		size_t write_to_free(std::pair<size_t, size_t> free_entry, const uint8_t *bytes, size_t size);;

		// writes n bytes to cache and returns the starting offset of the bytes written
		size_t write(const uint8_t *bytes, size_t size);

		// writes at the end of the cache and returns the starting offset
		size_t write_back(const uint8_t *bytes, size_t size);

		// writes at a specific offset in the cache
		void write_at(size_t offset, const uint8_t *bytes, size_t size);

		// adds offset and size to free list and possibly merges free list enteries
		void free(size_t offset, size_t size);

	public:
		DBContext &m_context;

		std::optional<std::pair<size_t, size_t>> 
		find_free(size_t size);
	};
}