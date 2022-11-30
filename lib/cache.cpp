#include "cache.hpp"

#include "../fmt.hpp"

namespace ambry
{
	void Cache::reserve(size_t size)
	{
		m_data.reserve(size);
	}

	size_t Cache::write_back(const uint8_t *bytes, size_t size)
	{
		size_t n = m_data.size();

		for (size_t i = 0; i < size; i++)
		{
			m_data.emplace_back(bytes[i]);
		}

		return n;
	}

	void Cache::write_at(size_t offset, const uint8_t *bytes, size_t size)
	{
		for (size_t i = offset, j = 0; j < size; i++, j++)
		{
			m_data[i] = bytes[j];
		}
	}

	size_t Cache::write(const uint8_t *bytes, size_t size)
	{
		auto entry_opt = find_free(size);

		if (entry_opt)
		{
			auto [offset, e_size] = entry_opt.value();

			write_at(offset, bytes, size);

			m_free_list.erase(offset);

			size_t diff = e_size-size;

			if (diff)
				m_free_list.emplace(offset+diff, diff);

			return offset;
		}

		return write_back(bytes, size);
	}

	uint8_t* Cache::read(size_t offset)
	{
		return m_data.data() + offset;
	}

	uint8_t* Cache::data()
	{
		return m_data.data();
	}

	void Cache::write_from_stream(FILE *stream, size_t size)
	{
		for (size_t i = 0; i < size; i++)
		{
			m_data.emplace_back(fgetc(stream));
		}
	}

	inline constexpr size_t
	Cache::size() const
	{
		return m_data.size();
	}

	void Cache::free(size_t offset, size_t size)
	{
		size_t start = offset;

		auto iter = m_free_list.find(offset-size);

		if (iter != m_free_list.end())
		{
			start -= size;
			size += iter->second;
			m_free_list.erase(iter);
		}

		iter = m_free_list.find(offset+size);

		if (iter != m_free_list.end())
		{
			size += iter->second;
			m_free_list.erase(iter);
		}

		m_free_list.emplace(start, size);
	}

	std::optional<std::pair<size_t, size_t>> 
	Cache::find_free(size_t size)
	{
		for (auto entry : m_free_list)
		{
			if (entry.second >= size)
			{
				return entry;
			}
		}

		return std::nullopt;
	}
}