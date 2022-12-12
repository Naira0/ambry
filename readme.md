# Ambry
Ambry is a key value database.

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
caching can be enabled to greatly improve read speeds, however, it will increase write speeds.

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