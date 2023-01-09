# Ambry
The ambry database library

## Usage
```cpp
using namespace ambry;

int main()
{
	DB db("my_db");

	Result result = db.open();

	if (!result.ok())
	{
		// handle error
	}

	db.set("hello", "world");
	db.set("ambry", "ambry is a database");
	db.set("erase", "me!");

	db.erase("erase");

	db.update("ambry", "sucks");

	for (auto [key, value] : db)
	{
		std::cout << key << ": " << value << '\n';
	}
} 
```

## Caching
caching can be enabled to greatly improve read speeds since it gets a slice from the cache and results in constant time reads.

```cpp
	DB db("my_db",  {
		.enable_cache = true,
	});

	Result result = db.open();

	if (!result.ok())
	{
		// handle error
	}

	db.set("hello", "world");
	
	// to get data from cache use get_cached instead of get instead
	std::cout << db.get_cached("hello").value() << '\n';
```

## Transactions
transactions just store a buffered seqeunce of commands to execute at a later time.

```cpp
	Transaction tr = db.begin_transaction();

	// will return the first error encountered when commiting
	Result result = tr
	.set("hello", "transactions!")
	.set("this", "can be chained!")
	.commit();
```

## Serialization 

ambry comes with a serialization lib called asf (ambry serialization format)

```cpp
#include "lib/db.hpp"
#include "lib/asf.hpp"

const std::string user_id = "123";

int main()
{
	ambry::Map user_in
	{
		{"name", "john"},
		{"password", "plsdonthackme"},
		{"email", "johns@email.com"},
		{"friends", {"alex", "john", "jim"}}
	};

	std::string buff_in = ambry::serialize(user_in);

	ambry::DB db("data");

	db.open();

	db.set(user_id, buff_in);

	std::string buff_out = db.get(user_id).value();

	ambry::Map user_out = ambry::deserialize(buff_out).value();
}
```
it supports strings, arrays, maps, and various different arithmetic types of different sizes. you can nest maps and arrays infinitely just like json.
