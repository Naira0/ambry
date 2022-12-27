# Ambry command interpreter
an extendible command interpreter library made for the cli and the server.

## Usage
```cpp

// parses a raw command and returns a vector of commands (commands are newline seperated)
auto cmd = aci::parse("set my_key \"some value\"");

if (cmd.empty())
{
	// handle error
}

// define a table of open databases
aci::DBTable dbt;

// will take a reference to the dbt for the commands to work on
aci::Interpreter interpreter(dbt);

// will init the command table with the default commands
interpreter.init_commands();

interpreter.interpret(cmd.value());

// you can extend the command table like so
interpreter.ct["my_cmd"] = 
{
	// number of args. use -1 for infinite args
	.arity = 3, 
	// description and usage can be used for a help command
	.description = "my epic command!",
	// the command name will be concatenated to the begining
	.usage = " a b c",
	// the function to call for the command logic
	.fn = echo_cb
};

```