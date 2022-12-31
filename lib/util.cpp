#include "util.hpp"

namespace ambry
{
	Result destroy(const std::string &name)
	{
		#define TRY(fn) if (fn == -1) return {ResultType::IoFailure, strerror(errno)};

		TRY(remove((name + ".free").c_str()));
		TRY(remove((name + ".dat").c_str()));
		TRY(remove((name + ".idx").c_str()));

		return {};

		#undef TRY
	}
}