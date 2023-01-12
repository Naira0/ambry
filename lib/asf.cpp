#include "asf.hpp"
#include "util.hpp"

#include <algorithm>
#include <cassert>

#include "../shared/fmt.hpp"

// one byte for the type, 4 for the size.
// a serialized value should always be more than this
constexpr int VALUE_HEADER_SIZE = 5;
// one byte is for type (1 key value, 2 single value) and one byte for endianness
constexpr int HEADER_SIZE = 2;

template<class T>
void write_to(std::string &buff, T n)
{
	auto *bytes = (char*)&n;

	for (size_t i = 0; i < sizeof(T); i++)
	{
		buff += bytes[i];
	}
}

template<class T>
T read_to(std::string_view buff, bool should_reverse = false)
{
	T n;

	auto *bytes = (char*)&n;

	for (size_t i = 0; i < sizeof(T); i++)
	{
		bytes[i] = buff[i];
	}

	if (should_reverse)
	{
		std::reverse(bytes, bytes+sizeof(T));
	}

	n = *(T*)bytes;

	return n;
}

std::string _serialize(const ambry::Value &value);

std::string serialize_string(const ambry::Value &value)
{
	std::string buff;

	auto str = std::get<ambry::StringT>(value);

	write_to(buff, (uint32_t)str.size());

	buff += str;

	return buff;
}

std::string serialize_array(const ambry::Value &value)
{
	std::string buff;

	auto vec = std::get<ambry::Array>(value);

	write_to(buff, (uint32_t)vec.size());

	for (const auto &val : vec)
	{
		buff += _serialize(val);
	}

	return buff;
}

std::string serialize_map(const ambry::Value &value)
{
	std::string buff;

	auto map = std::get<ambry::MapT>(value);

	write_to(buff, (uint32_t)map.size());

	for (const auto &[key, value]: map)
	{
		write_to(buff, (uint16_t)key.size());

		buff += key;

		buff += _serialize(value);
	}

	return buff;
}

std::string _serialize(const ambry::Value &value)
{
	std::string buff;

	write_to(buff, (uint8_t)value.index());

#define INTEGRAL_CASE(t, s) case t: write_to(buff, s); write_to(buff, std::get<t>(value));  break;

	using namespace ambry;

	switch (value.index())
	{	
		INTEGRAL_CASE(I8, 1)
		INTEGRAL_CASE(U8, 1)
		INTEGRAL_CASE(I16, 2)
		INTEGRAL_CASE(U16, 2)
		INTEGRAL_CASE(I32, 4)
		INTEGRAL_CASE(U32, 4)
		INTEGRAL_CASE(I64, 8)
		INTEGRAL_CASE(U64, 8)
		INTEGRAL_CASE(Double, sizeof(double))
		case StringT:
		{
			buff += serialize_string(value);
			break;
		}
		case ArrayT:
		{
			buff += serialize_array(value);
			break;
		}
		case MapT:
		{
			buff += serialize_map(value);
			break;
		}
	}

	return buff;

#undef INTEGRAL_CASE
}

std::string ambry::serialize_value(const Value &value)
{
	std::string buff;
	
	// data type (single value)
	buff += 2;
	// the endianness of the data (1 for little 0 for big)
	buff += machine_endian();

	buff += _serialize(value);

	return buff;
}

std::string ambry::serialize(const Map &map)
{
	std::string buff;
	
	// data type (key value pairs)
	buff += 1;
	// the endianness of the data (1 for little 0 for big)
	buff += machine_endian();

	buff += serialize_map(map);

	return buff;
}

std::optional<
	std::pair<ambry::Value, size_t>> 
_deserialize(std::string_view buff, bool should_reverse);

std::optional<
	std::pair<ambry::Value, size_t>>
deserialize_array(std::string_view buff, bool should_reverse, uint32_t len)
{
	ambry::Array out;

	size_t offset = 0;

	for (int i = 0; i < len; i++)
	{
		auto opt = _deserialize(buff.substr(offset), should_reverse);

		if (!opt)
		{
			return std::nullopt;
		}

		auto &[value, size] = opt.value();

		offset += VALUE_HEADER_SIZE + size;

		out.emplace_back(std::move(value));
	}

	return {{ out, offset }};
}

std::optional<
	std::pair<ambry::Map, size_t>> 
deserialize_map(std::string_view buff, bool should_reverse, uint32_t len)
{
	ambry::Map out;

	size_t offset = 0;

	for (int i = 0; i < len; i++)
	{
		auto key_len = read_to<uint16_t>(buff.substr(offset), should_reverse);

		std::string key { buff.substr(offset+2, key_len) };

		offset += 2 + key_len;

		auto opt = _deserialize(buff.substr(offset), should_reverse);

		if (!opt)
		{
			return std::nullopt;
		}

		auto &[value, size] = opt.value();

		offset += VALUE_HEADER_SIZE + size;

		out.emplace(std::move(key), std::move(value));
	}

	return {{ out, offset }};
}

std::optional<
	std::pair<ambry::Value, size_t>> 
_deserialize(std::string_view buff, bool should_reverse)
{
	uint8_t type = buff[0];

	auto len = read_to<uint32_t>(buff.data()+1);

	buff = buff.substr(VALUE_HEADER_SIZE);

#define INTEGRAL_CASE(t, T) case t: return {{ read_to<T>(buff, should_reverse), sizeof(T) }};

	using namespace ambry;

	switch (type)
	{
		INTEGRAL_CASE(I8, int8_t)
		INTEGRAL_CASE(U8, uint8_t)
		INTEGRAL_CASE(I16, int16_t)
		INTEGRAL_CASE(U16, uint16_t)
		INTEGRAL_CASE(I32, int32_t)
		INTEGRAL_CASE(U32, uint32_t)
		INTEGRAL_CASE(I64, int64_t)
		INTEGRAL_CASE(U64, uint64_t)
		INTEGRAL_CASE(Double, double)
		case StringT:
		{
			return {{ std::string { buff.substr(0, len) }, len }};
		}
		case ArrayT:
		{
			return deserialize_array(buff, should_reverse, len);
		}
		case MapT:
		{
			auto opt = deserialize_map(buff, should_reverse, len);

			if (!opt)
			{
				return std::nullopt;
			}

			return opt.value();
		}
	}

#undef INTEGRAL_CASE
}

#define DESERIALIZE_CHECK(type) \
	if (				\
	   buff.empty()  \
	|| buff.size() <= HEADER_SIZE \
	|| type(buff)) \
	{ \
		return std::nullopt; \
	} \

std::optional<ambry::Value> 
ambry::deserialize_value(std::string_view buff)
{
	DESERIALIZE_CHECK(is_map);

	bool should_reverse = buff[1] != machine_endian();

	auto opt = _deserialize(buff.substr(6), should_reverse);

	if (!opt)
	{
		return std::nullopt;
	}

	return opt.value().first;
}

std::optional<ambry::Map> 
ambry::deserialize(std::string_view buff)
{
	DESERIALIZE_CHECK(is_single);
	
	bool should_reverse = buff[1] != machine_endian();

	auto len = read_to<uint32_t>(buff.substr(2));

	auto opt = deserialize_map(buff.substr(6), should_reverse, len);

	if (!opt)
	{
		return std::nullopt;
	}

	return opt.value().first;
}

std::string ambry::to_string(const Value &value)
{
#define INTEGRAL_CASE(t) case t: return std::to_string(std::get<t>(value));

	switch (value.index())
	{
		INTEGRAL_CASE(I32)
		case StringT: return "\"" + std::get<StringT>(value) + "\"";
		case ArrayT:
		{
			auto vec = std::get<ArrayT>(value);

			auto iter = vec.begin();

			std::string buff = "[ " + to_string(*iter++);

			for (; iter != vec.end(); iter++)
			{
				buff += ", " + to_string(*iter);
			}

			buff += " ]";

			return buff;
		} 
		case MapT:
		{
			std::string buff = "{\n";

			auto map = std::get<MapT>(value);

			for (const auto &[k, v] : map)
			{
				buff += k + ": " + to_string(v) + '\n';
			}

			buff += "}";

			return buff;
		}
	}

#undef INTEGRAL_CASE
}

#define CHECK(expr) if (iter != schema.fields.end() && (expr)) return std::nullopt

std::optional<
	std::pair<ambry::Map, size_t>> 
deserialize_map(std::string_view buff, bool should_reverse, uint32_t len, ambry::Schema &schema)
{
	ambry::Map out;

	size_t offset = 0;

	for (int i = 0; i < len; i++)
	{
		auto key_len = read_to<uint16_t>(buff.substr(offset), should_reverse);

		std::string key { buff.substr(offset+2, key_len) };

		auto iter = schema.fields.find(key);

		if (iter == schema.fields.end() && !schema.opts.allow_undefined)
		{
			return std::nullopt;
		}
		else
		{
			iter->second.found = true;
		}

		offset += 2 + key_len;

		uint8_t type = buff[offset];

		CHECK(iter->second.type != ambry::AnyT && type != iter->second.type);

		auto opt = _deserialize(buff.substr(offset), should_reverse);

		if (!opt)
		{
			return std::nullopt;
		}

		auto &[value, size] = opt.value();

		if (iter != schema.fields.end() && iter->second.fn)
		{
			if (!iter->second.fn(value))
			{
				return std::nullopt;
			}
		}

		offset += VALUE_HEADER_SIZE + size;

		out.emplace(std::move(key), std::move(value));
	}

	for (auto &[_, field] : schema.fields)
	{
		if (!field.optional && !field.found)
		{
			return std::nullopt;
		}
	}

	return {{ out, offset }};
}

#undef CHECK

std::optional<ambry::Map> ambry::deserialize(std::string_view buff, Schema &schema)
{
	DESERIALIZE_CHECK(is_single);
	
	bool should_reverse = buff[1] != machine_endian();

	auto len = read_to<uint32_t>(buff.substr(2));

	auto opt = deserialize_map(buff.substr(6), should_reverse, len, schema);

	if (!opt)
	{
		return std::nullopt;
	}

	return opt.value().first;
}
