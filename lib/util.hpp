#pragma once

#include "types.hpp"
#include <alloca.h>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <string>

namespace ambry
{

	Result destroy(const std::string &name);

	// returns 1 for little indian 0 for big
	static inline 
	uint8_t machine_endian()
	{
		static int i = 1;
		return ((char*)&i)[0];
	}
	
	template<class T>
	std::string to_bytes(T n)
	{
		static_assert(std::is_arithmetic<T>(), "T is not an arithmetic type");

		auto bytes = (char*)&n;

		return {bytes, sizeof(T)};
	}

	template<class T>
	T bytes_to(std::string_view slice)
	{
		static_assert(std::is_arithmetic<T>(), "T is not an arithmetic type");

		T n{};

		auto bytes = alloca(sizeof(T));

		std::memcpy(bytes, slice.data(), sizeof(T));

		n = *(T*)bytes;

		return n;
	}
}