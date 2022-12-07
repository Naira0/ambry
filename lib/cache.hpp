#pragma once

// an object responsible for managing the runtime database cache

#include <cstdint>
#include <cstddef>
#include <cstdio>

#include <vector>
#include <map>
#include <optional>

#include "io_manager.hpp"
#include "types.hpp"

namespace ambry
{
	class Cache
	{
	public:

		Cache(DBContext &context, IoManager &io_manager) :
			m_context(context),
			m_io_manager(io_manager)
		{}

		size_t write_to_free(std::pair<size_t, size_t> free_entry, const char *bytes, size_t size);;

		// writes n bytes to cache and returns the starting offset of the bytes written
		void write(const char *bytes, size_t offset, uint32_t size);

		// writes at the end of the cache and returns the starting offset
		size_t write_back(const char *bytes, size_t size);

		// writes at a specific offset in the cache
		void write_at(size_t offset, const char *bytes, size_t size);

		// adds offset and size to free list and possibly merges free list enteries
		void free(size_t offset, size_t size);

	public:
		DBContext &m_context;
		IoManager &m_io_manager;

		std::optional<std::pair<size_t, size_t>>

		find_free(size_t size);
	};
}