#pragma once

#include <variant>
#include <string>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <optional>

namespace ambry
{

	// useful for less error prone switch statements 
	enum Type : uint8_t
	{
		I8, U8, I16, U16, I32, U32, I64, U64, Double, 
		StringT, ArrayT, MapT, AnyT
	};

	struct Value;

    using Array  = std::vector<Value>;
	using Map = std::unordered_map<std::string, Value>;

	using ValueBase  = std::variant<
	int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, double, 
	std::string, Array, Map>;

    struct Value : public ValueBase
    {
        Value() = default;

        template<typename T>
        Value(T value)
        {
            set_value(std::forward<T>(value));
        }

		Value(std::initializer_list<Value> &&value)
        {
            emplace<Array>(value);
        }

		Value(Map &&value)
        {
            emplace<Map>(value);
        }

        Value &operator=(Map &&value)
        {
            emplace<Map>(value);
            return *this;
        }

        Value &operator=(std::initializer_list<Value> &&value)
        {
            emplace<Array>(value);
            return *this;
        }

    private:
        template<typename T>
        void set_value(T &&value)
        {
			if constexpr (std::is_floating_point_v<T>)
			{
				emplace<double>(value);
			}
            else if constexpr (std::is_constructible_v<std::string, T>)
			{
                emplace<std::string>(value);
			}
			else
			{
				emplace<T>(value);
			}
        }
    };

	std::string serialize_value(const Value &value);

	std::string serialize(const Map &map);

	std::optional<Value> deserialize_value(std::string_view buff);

	std::optional<Map> deserialize(std::string_view buff);

	// recursively converts any value to a string
	std::string to_string(const Value &value);

	// determines if serialized data is a single value 
	static inline bool is_single(std::string_view buff)
	{
		if (buff.empty())
		{
			return false;
		}

		return buff[0] == 2;
	}

	// determines if serialized data is a map
	static inline bool is_map(std::string_view buff)
	{
		if (buff.empty())
		{
			return false;
		}
		
		return buff[0] == 1;
	}

	typedef bool(*ValidateFN)(Value &value);

	struct SchemaOpts
	{
		bool allow_undefined;
	};

	struct SchemaField
	{
		Type type = AnyT;
		bool optional = false;
		ValidateFN fn = nullptr;
		bool found = false;
	};

	using Struct = std::unordered_map<std::string_view, SchemaField>;

	struct Schema
	{
		SchemaOpts opts;
		Struct fields;
	};

	std::optional<Map> deserialize(std::string_view buff, Schema &schema);
};