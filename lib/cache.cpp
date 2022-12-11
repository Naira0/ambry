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