#include "cache.hpp"

#include "../fmt.hpp"
#include "types.hpp"

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

	void Cache::write(const char *bytes, size_t offset, uint32_t size)
	{
		if (offset == std::string::npos)
		{
			write_back(bytes, size);
		}
		else
		{
			write_at(offset, bytes, size);
		}
	}
}