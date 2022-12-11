#pragma once

// read-write class that manages writing and reading from and to the cache and on disk files

#include "cache.hpp"
#include "io_manager.hpp"
#include "types.hpp"

namespace ambry
{
	class RW
	{
	public:

		RW(DBContext &context, IoManager &io_manager) :
			m_context(context),
			m_io_manager(io_manager),
			m_cache(context, m_io_manager),
			m_options(m_context.options)
		{}

		size_t write(std::string_view slice);

		size_t update(size_t old_offset, uint32_t old_size, std::string_view slice);
		
		void free(size_t offset, size_t size);

	private:
		DBContext &m_context;
		Options &m_options;
		IoManager &m_io_manager;
		Cache m_cache;

		void set_changelog(size_t offset, uint32_t size);

		// TODO make sure it also erases the freelist in the .free file
		size_t manage_free(std::pair<uint32_t, FreeEntry> entry, size_t data_size);

		std::optional<std::pair<uint32_t, FreeEntry>> 
		find_free(size_t size);
		
	};
}