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

	for (auto [key, value] : db)
	{
		std::cout << key << ": " << value << '\n';
	}
} 
```
