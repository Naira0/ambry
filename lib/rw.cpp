#include "rw.hpp"
#include "types.hpp"
#include <cstdint>

namespace ambry
{
	size_t RW::write(std::string_view slice)
	{
		size_t size = slice.size();
		const char *data = slice.data();

		size_t offset = std::string::npos;

		auto free_opt = find_free(slice.size());

		if (free_opt)
		{
			offset = manage_free(free_opt.value(), size);
		}

		if (m_context.options.enable_cache)
		{
			m_cache.write(data, offset, size);
		}

		offset = m_io_manager.write_dat(data, offset, size);

		return offset;
	}

	size_t RW::update(size_t old_offset, uint32_t old_size, std::string_view slice)
	{
		size_t size   = slice.size();
		size_t offset = std::string::npos;

		if (old_size < size)
		{
			offset = old_offset;
		}
		else
		{
			auto free_opt = find_free(size);

			if (free_opt)
			{
				offset = manage_free(free_opt.value(), size);
			}
		}
		
		offset = m_io_manager.write_dat(slice.data(), offset, size);
		
		if (m_context.options.enable_cache)
		{
			m_cache.write_at(offset, slice.data(), size);
		}

		return offset;
	}

	void RW::free(size_t offset, size_t size)
	{
		// auto iter = m_context.free_list.find(offset+size);

		// if (iter != m_context.free_list.end())
		// {
		// 	size += iter->first;
		// 	m_context.free_list.erase(iter);
		// }

		// iter = m_context.free_list.find(offset-size);

		// if (iter != m_context.free_list.end())
		// {
		// 	offset -= size;
		// 	size += iter->first;
		// 	m_context.free_list.erase(iter);
		// }

		uint32_t off = m_io_manager.update_freelist(offset, size);

		m_context.free_list.emplace(size, FreeEntry{offset, off});
	}

	size_t RW::manage_free(std::pair<uint32_t, FreeEntry> entry, size_t data_size)
	{
		auto [size, free] = entry;

		m_context.free_list.erase(size);
		m_io_manager.erase_freelist(free.free_list_offset);

		size_t diff = size-data_size;

		if (diff)
		{
			size_t new_offset = free.offset+data_size;

			uint32_t off = m_io_manager.update_freelist(new_offset, diff);

			auto iter = m_context.free_list.emplace(diff, FreeEntry{new_offset, off});
		}

		return free.offset;
	}

	std::optional<std::pair<uint32_t, FreeEntry>> 
	RW::find_free(size_t size)
	{
		for (auto entry : m_context.free_list)
		{
			if (entry.first > size)
			{
				return entry;
			}
		}

		return std::nullopt;
	}
}