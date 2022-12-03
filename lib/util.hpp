#pragma once

#include <alloca.h>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace ambry
{
	template<class T>
	std::string_view make_slice(T n)
	{
		static_assert(std::is_arithmetic<T>(), "T is not an arithmetic type");

		auto bytes = (char*)&n;

		return std::string_view{bytes, sizeof(T)};
	}

	template<class T>
	T slice_to(std::string_view slice)
	{
		static_assert(std::is_arithmetic<T>(), "T is not an arithmetic type");

		T n{};

		auto bytes = alloca(sizeof(T));

		std::memcpy(bytes, slice.data(), sizeof(T));

		n = *(T*)bytes;

		return n;
	}
}