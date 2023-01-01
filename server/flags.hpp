// taken from my flag parser library https://github.com/Naira0/flag-parser

#include <initializer_list>
#include <string_view>
#include <span>
#include <vector>
#include <variant>
#include <string>
#include <unordered_map>
#include <charconv>
#include <thread>
#include <list>
#include <optional>

namespace flag
{
    // the type of a Flag
    enum Type : uint8_t 
    {
        String, Number, Bool 
    };

    struct Result
    {
        // the flag id that caused the error. likely an incorrect flag id was used
        std::string_view flag_id;
        // the error message
        std::string_view error;

        inline bool ok() const
        {
            return error.empty();
        }
    };

    // the variant used to store flag data
    using FlagData = std::variant<std::string_view, double, bool>;

    struct Flag;

    typedef Result (*FlagFn)(Flag&);

    struct Flag 
    {
        // the name of the flag
        std::string_view name;

        // the flag description
        std::string_view description;

        // the actual data of the flag. its also the default value if set.
        FlagData data{};

        // the type of the flag. by default it will be Any
        Type type = String;

        // the flag aliases
        std::initializer_list<std::string_view> aliases;

        // the function that will be called when the flag is triggered. must call Parser::call.
        FlagFn fn = nullptr;

        // will be set to true if the flag is ever triggered
        bool triggered = false;

    };

    // a lookup table of flags. keys can be aliases or the flag name.
    using FlagTable = std::unordered_map<std::string_view, Flag*>;

    // a list of flags. uses a list instead of a vector to avoid invalidating the references of the lookup table as it resizes.
    using Flags = std::list<Flag>;

    struct Options 
    {
        // the prefix used for all flag names
        std::string_view flag_prefix = "-";
        // the characters that will be used to separate the flag name and value
        std::string_view separator = "=";
        // when set to true the parse function will fail when an incorrect flag is used
        bool strict_flags = true;
    };

    class Parser 
    {
    public:

        using View     = std::span<char*>;
        using Flagless = std::vector<std::string_view>;

        Parser(View args, Options options) :
            m_options(options),
            m_args(args)
        {
        }

        Parser& set(Flag &&flag)
        {
            Flag &f = m_flags.emplace_back(flag);

            m_table.emplace(flag.name, &f);

            for (auto alias : f.aliases)
                m_table.emplace(alias, &f);

            return *this;
        }

        Result parse()
        {
            m_flagless.reserve(m_args.size());

            for (size_t a = 0; a < m_args.size(); a++)
            {
                std::string_view arg = m_args[a];

                if (!is_flag(arg))
                {
                    m_flagless.push_back(arg);
                    continue;
                }

                size_t prefix_end = m_options.flag_prefix.size();

                auto [sep_start, found_sep] = parse_flag(arg, prefix_end);

                std::string_view id = arg.substr(prefix_end, sep_start - prefix_end);

                auto flag_opt = get(id);

                if (!flag_opt.has_value())
                {
                    if (m_options.strict_flags)
                        return Result{id, "invalid flag id used"};
                    else continue;
                }

                Flag *flag = flag_opt.value();

                if (flag->type == Bool)
                {
                    flag->data = true;
                    flag->triggered = true;
                    continue;
                }

                std::string_view value_str;

                Result fail_result {id, "could not set flag value"};

                if (found_sep && ++sep_start < arg.size())
                    value_str = arg.substr(sep_start);
                else if (a+1 < m_args.size())
                    value_str = m_args[++a];
                else
                    return fail_result;
                
                bool ok = set_value(value_str, *flag);

                if (!ok)
                    return fail_result;
            }

            return Result{};
        }

        // calls all flag functions. returns the first result that has an error.
        Result call()
        {
            for (Flag &flag : m_flags)
            {
                if (flag.triggered && flag.fn)
                {
                    Result result = (*flag.fn)(flag);

                    if (!result.ok())
                        return result;
                }
            }

            return {};
        }

        /*
        * returns an array of flags

        * this is where the actual data is stored
        */
        Flags& flags()
        {
            return m_flags;
        }

        /* 
        * returns a lookup table for flags

        * both aliases and flag names can be used to lookup a flag

        * flag is returned as a pointer
        */
        FlagTable& table()
        {
            return m_table;
        }

        // a shorthand for looking up flags by alias or name. checks to see if the flag is valid beforehand and returns an optional to the flag pointer.
        std::optional<Flag*> get(std::string_view id) const
        {
            auto it = m_table.find(id);

            if (it == m_table.end())
                return {};

            return { it->second };
        }

        std::string to_string() const 
        {
            std::string output;

            for (const Flag &flag : m_flags)
            {
                output += m_options.flag_prefix;
                output += flag.name;
                output += "\t\t";
                output += flag.description;
                output += "\n";
            }

            return output;
        }

        // returns the arguments without flags
        Flagless& args()
        {
            return m_flagless;
        }

        Options& options()
        {
            return m_options;
        }

    private:
        Flags m_flags;
        FlagTable m_table;
        Options m_options;
        View m_args;
        Flagless m_flagless;

        bool is_flag(std::string_view str) const
        {
            size_t size = m_options.flag_prefix.size();

            if (str.size() <= size)
                return false;

            for (size_t i = 0; i < size; i++)
            {
                if (str[i] != m_options.flag_prefix[i])
                    return false;
            }

            return true; 
        }

        std::pair<size_t, bool> parse_flag(std::string_view str, size_t offset = 0)
        {
            std::string_view sep = m_options.separator;

            for (size_t i = offset; i < str.size(); i++)
            {
                if (str[i] == sep[0])
                {
                    bool found = true;
                    size_t j;

                    for (j = i; j < sep.size(); j++, i++)
                    {
                        if (str[i] != sep[j])
                            found = false;
                    }

                    if (found)
                        return {j, true};
                }
            }

            return {str.size(), false};
        }

        bool set_value(std::string_view str, Flag &flag)
        {
            switch (flag.type)
            {
                case String: flag.data = str; break;
                case Number: 
                {
                   
                    double value{};

                    auto result = std::from_chars(
                        str.data(), 
                        str.data()+str.size(), 
                        value);

                    if (result.ec != std::errc())
                        return false;

                    flag.data = value;

                    break;
                }
            }

            flag.triggered = true;
            return true;
        }
    };
}