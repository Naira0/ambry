#pragma once

// an object responsible for managing the runtime database cache

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

		// writes n bytes to cache and returns the starting offset of the bytes written
		void write(const char *bytes, size_t offset, uint32_t size);

		// writes at the end of the cache and returns the starting offset
		size_t write_back(const char *bytes, size_t size);

		// writes at a specific offset in the cache
		void write_at(size_t offset, const char *bytes, size_t size);

	public:
		DBContext &m_context;
		IoManager &m_io_manager;
	};
}