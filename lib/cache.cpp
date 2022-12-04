#include "cache.hpp"

#include "../fmt.hpp"

#define WRITE_BACK(b) \
		for (size_t i = 0; i < size; i++) \
		{ \
			m_context.data.emplace_back(b); \
		} \

namespace ambry
{
	size_t Cache::write_back(const char *bytes, size_t size)
	{
		size_t n = m_context.data.size();

		WRITE_BACK(bytes[i]);

		return n;
	}

	void Cache::write_at(size_t offset, const char *bytes, size_t size)
	{
		for (size_t i = offset, j = 0; j < size; i++, j++)
		{
			m_context.data[i] = bytes[j];
		}
	}

	size_t Cache::write_to_free(std::pair<size_t, size_t> free_entry, const char *bytes, size_t size)
	{
		auto [offset, e_size] = free_entry;

		write_at(offset, bytes, size);

		m_context.free_list.erase(offset);

		size_t diff = e_size-size;

		if (diff)
		{
			size_t new_offset = offset+size;

			m_context.free_list.emplace(new_offset, diff);

			m_io_manager.update_freelist(new_offset, diff);
		}

		return offset;
	}

	size_t Cache::write(const char *bytes, size_t size)
	{
		auto entry_opt = find_free(size);

		size_t n{};

		if (entry_opt)
		{
			n = write_to_free(entry_opt.value(), bytes, size);
		}
		else
		{
			n = write_back(bytes, size);
		}

		m_context.changelog.emplace(n, size);

		return n;
	}

	void Cache::free(size_t offset, size_t size)
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

	std::optional<std::pair<size_t, size_t>> 
	Cache::find_free(size_t size)
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