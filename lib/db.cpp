#include "db.hpp"

#include <limits>
#include <sys/stat.h>
#include <sys/file.h>

#include <cstdio>
#include <cctype>
#include <cassert>
#include <cstring>

#include <charconv>
#include <filesystem>

#include "../fmt.hpp"

static constexpr size_t NPOS = std::numeric_limits<size_t>::max();

// char that will be used to seperate values in index file
static constexpr char IDX_SEP = ';';

#define IOFAILURE(message)    {ResultType::IoFailure, message}
#define MALFORMEDIDX(message) {ResultType::MalformedIdx, message}

namespace ambry
{

    DB::~DB()
    {
        flock(m_fidx->_fileno, LOCK_UN);
        flock(m_fdat->_fileno, LOCK_UN);

        std::fclose(m_fidx);
        std::fclose(m_fdat);
    }

    Result DB::open()
    {
        std::string idx_name = m_name + ".idx";
        std::string dat_name = m_name + ".dat";

        m_fidx = fopen(idx_name.c_str(), "r+");
        m_fdat = fopen(dat_name.c_str(), "rb+");

        if (!m_fidx || !m_fdat)
        {
            m_fidx = m_fidx  ? m_fidx : fopen(idx_name.c_str(), "w+");
            m_fdat = m_fdat  ? m_fdat : fopen(dat_name.c_str(), "wb+");

            if (!m_fidx || !m_fdat)
                return IOFAILURE("could not open idx or dat file");
        }

        if (flock(m_fidx->_fileno, LOCK_EX) != 0)
            return IOFAILURE("could not lock index file");

        if (flock(m_fdat->_fileno, LOCK_EX) != 0)
            return IOFAILURE("could not lock data file"); 

        Result result = read_index();

        if (!result.ok())
            return result;

        return read_data();
    }
    
    // reads all integers until it hits IDX_SEP char then returns the int
    size_t read_n(FILE *stream, std::string &buff, size_t &idx, size_t size)
    {
        char c;

        while (idx < size && (c = fgetc(stream)) != IDX_SEP)
        {
            idx++;
            buff += c;
        }

        idx++;

        size_t n {};

        auto result = std::from_chars(buff.data(), 
                        buff.data() + buff.size(), 
                        n);

        if (result.ec != std::errc())
            return NPOS;

        buff.clear();

        return n;
    }

    Result DB::read_index()
    {
        #define READ(field) \
            data.field = read_n(m_fidx, buff, i, fsize); \
            if (data.field == NPOS) \
                return MALFORMEDIDX("could not read index"); \

        struct stat st;

        if (fstat(m_fidx->_fileno, &st) != 0)
            return IOFAILURE("could not get stat on idx file");

        size_t fsize = st.st_size;

        std::string buff;
        
        for (size_t i = 0; i < fsize; i++)
        {
            char c = fgetc(m_fidx);

            if (!isdigit(c))
                continue;

            IndexData data;

            buff += c;

            // reads the key length
            size_t key_len = read_n(m_fidx, buff, i, fsize);

            if (key_len == NPOS)
                return MALFORMEDIDX("could not read key length");

            std::string key;

            key.reserve(key_len+1);

            // reads key
            for (size_t j = 0; j < key_len; j++, i++)
            {
                key += fgetc(m_fidx);
            }

            ++i;

            READ(offset);
            READ(length);

            data.type = (Type)read_n(m_fidx, buff, i, fsize);

            if ((size_t)data.type == NPOS)
                return MALFORMEDIDX("could not read type id");

            m_index.emplace(std::move(key), data);

        #undef READ
        }

        return {};
    }

    Result DB::read_data()
    {
        struct stat st;

        if (fstat(m_fdat->_fileno, &st) != 0)
            return IOFAILURE("could not get stat on idx file");

        size_t fsize = st.st_size;

        m_data.reserve(fsize * 2);
        
        m_data.write_from_stream(m_fdat, fsize);

        return {};
    }

    inline bool is_big_endian()
    {
        static const int i = 1;
        return (*(char*)&i) == 0;
    }

    template<class T>
    T bytes_as(uint8_t *bytes, size_t size)
    {
        T n{};

        auto data = (uint8_t*)&n;

        std::memcpy(data, bytes, size);

        return n;
    }

    Value DB::get(const std::string &key)
    {
        auto iter = m_index.find(key);

        if (iter == m_index.end())
            return {};

        auto [offset, length, type] = iter->second;

        auto cache = m_data.data() + offset;
       
        switch (type)
        {
            case Type::None:   return {};
            case Type::String: return std::string_view{(char*)cache, length};
            case Type::Int:    return bytes_as<int>(cache, length);
        }

        assert(!"invalid type in DB::get");
    }

    Result DB::set(std::string_view key, std::string_view value)
    {
        return set_bytes(key, (uint8_t*)value.data(), value.size(), Type::String);
    }

    Result DB::set(std::string_view key, int value)
    {
        return set_bytes(key, (uint8_t*)&value, sizeof(int), Type::Int);
    }

    Result DB::set_bytes(std::string_view key, const uint8_t *bytes, size_t size, Type type)
    {
        auto iter = m_index.emplace(key, IndexData{});

        if (!iter.second)
            return {ResultType::KeyNotInserted, "Could not insert key into index"};

        size_t offset = m_data.write(bytes, size);

        IndexData &data = iter.first->second;

        data.offset = offset;
        data.length = size;
        data.type   = type;

        return {};
    }

    Result DB::erase(const std::string &key)
    {
        auto iter = m_index.find(key);

        if (iter == m_index.end())
            return {ResultType::KeyNotFound, "key does not exist in index"};
        
        IndexData data = iter->second;
        
        m_index.erase(iter);

        m_data.free(data.offset, data.length);

        return {};
    }
}