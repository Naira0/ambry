#include "rw.hpp"

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

		if (m_options.flush_mode == FlushMode::Constant)
		{
			offset = m_io_manager.write_dat(data, offset, size);
		}
		else
		{
			set_changelog(offset, size);
		}

		if (m_options.enable_cache)
		{
			m_cache.write(data, offset, size);
		}

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

		if (m_context.options.flush_mode == FlushMode::Constant)
		{
			offset = m_io_manager.write_dat(slice.data(), offset, size);
		}
		else
		{
			set_changelog(offset, size);
		}

		if (m_options.enable_cache)
		{
			m_cache.write_at(offset, slice.data(), size);
		}

		return offset;
	}

	void RW::free(size_t offset, size_t size)
	{
		auto iter = m_context.free_list.find(offset+size);

		if (iter != m_context.free_list.end())
		{
			size += iter->second;
			m_context.free_list.erase(iter);
		}

		iter = m_context.free_list.find(offset-size);

		if (iter != m_context.free_list.end())
		{
			offset -= size;
			size += iter->second;
			m_context.free_list.erase(iter);
		}

		m_context.free_list.emplace(offset, size);

		m_io_manager.update_freelist(offset, size);
	}

	void RW::set_changelog(size_t offset, uint32_t size)
	{
		auto &changelog = m_context.changelog;

		auto iter = changelog.find(offset);

		if (offset != std::string::npos && iter != changelog.end())
		{
			changelog.erase(iter);
		}

		changelog.emplace(offset, size);
	}

	size_t RW::manage_free(std::pair<size_t, size_t> entry, size_t data_size)
	{
		auto [offset, e_size] = entry;

		m_context.free_list.erase(offset);

		size_t diff = e_size-data_size;

		if (diff)
		{
			size_t new_offset = offset+data_size;

			m_context.free_list.emplace(new_offset, diff);

			m_io_manager.update_freelist(new_offset, diff);
		}

		return offset;
	}

	std::optional<std::pair<size_t, size_t>> 
	RW::find_free(size_t size)
	{
		for (auto entry : m_context.free_list)
		{
			if (entry.second > size)
			{
				return entry;
			}
		}

		return std::nullopt;
	}
}