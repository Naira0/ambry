#include "io_manager.hpp"
#include "types.hpp"
#include <bits/chrono.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <mutex>
#include <thread>

#include <algorithm>
#include <sys/file.h>
#include <sys/stat.h>

#include "../fmt.hpp"

/*
	the first byte of all database files (except .dat) should be either 0 or 1 to determine endianness of the data
*/

namespace ambry
{
	IoManager::~IoManager()
	{
		if (m_context.options.flush_mode != FlushMode::Constant)
		{
			flush_changelog();
		}

		for (auto f : m_files)
		{
			flock(f->_fileno, LOCK_UN);
			fclose(f);
		}
	}

	Result IoManager::open_files()
	{
		for (int i = 0; i < m_files.size(); i++)
		{
			std::string_view ext = m_file_ext[i];

			std::string name = (m_context.name + ext.data());

			FILE *f = fopen(name.c_str(), "rb+");

			if (!f)
			{
				f = fopen(name.c_str(), "wb+");

				if (!f)
					return {ResultType::IoFailure, "could not open one of db files"};
			}

			flock(f->_fileno, LOCK_EX);

			m_files[i] = f;
		}

		return {};
	}

	Result IoManager::load_structures()
	{

	#define HANDLE(fn)     \
		result = fn();     \
		if (!result.ok())  \
			return result; \

		Result result;

		HANDLE(open_files);
		HANDLE(load_index);
		HANDLE(load_dat);
		HANDLE(load_free);

		return {};

	#undef HANDLE
	}

	inline uint8_t machine_endian()
	{
		static const int i = 1;
		return ((char*)&i)[0];
	}

	template<class T>
	T read_n(FILE *stream, uint8_t endian)
	{
		T n{};

		auto bytes = (char*)&n;

		for (T i = 0; i < sizeof(T); i++)
		{
			bytes[i] = fgetc(stream);
		}

		if (machine_endian() != endian)
		{
			std::reverse(bytes, bytes+sizeof(T));
		}

		n = *(T*)bytes;
		
		return n;
	}

	size_t stat_f(int fd)
	{
		struct stat st;

		if (fstat(fd, &st) != 0)
			return std::string::npos;

		return st.st_size;
	}

	uint8_t handle_endian(FILE *f)
	{
		uint8_t endian = fgetc(f);

		fseek(f, -1, SEEK_CUR);
		fputc(machine_endian(), f);

		return endian;
	}

	/*
		the index format is as follows:
		2 bytes for the key length
		1 byte to determine if the key is valid. if its not valid it should skip to the next key
		the key
		8 bytes for the offset in the dat file
		4 bytes for the length of the data
	*/
	Result IoManager::load_index()
	{
		FILE *f = m_files[IDX];

		size_t fsize = stat_f(f->_fileno);

		if (fsize == std::string::npos)
			return {ResultType::IoFailure, "could not stat one of db files"};

		uint8_t endian = handle_endian(f);

		while (ftell(f) < fsize)
		{
			IndexData data;

			auto key_len = read_n<uint16_t>(f, endian);

			data.idx_offset = ftell(f);

			uint8_t is_valid = fgetc(f);

			if (!is_valid)
			{
				size_t offset = key_len + sizeof(size_t) + sizeof(uint32_t);
				fseek(f, offset, SEEK_CUR);
				continue;
			}

			std::string key;

			key.reserve(key_len+1);

			for (uint16_t i = 0; i < key_len; i++)
			{
				key += fgetc(f);
			}

			data.offset = read_n<size_t>(f, endian);
			data.length = read_n<uint32_t>(f, endian);

			m_context.index.emplace(std::move(key), data);
		}

		return {};
	}

	Result IoManager::load_dat()
	{
		FILE *f = m_files[DAT];

		size_t fsize = stat_f(f->_fileno);

		if (fsize == std::string::npos)
			return {ResultType::IoFailure, "could not stat one of db files"};

		m_context.data.reserve(fsize * 2);

		for (size_t i = 0; i < fsize; i++)
		{
			m_context.data.emplace_back(fgetc(f));
		}

		return {};
	}

	/*
		the free list format is as follows:
		8 bytes for the offset
		4 bytes for the length
	*/
	Result IoManager::load_free()
	{
		FILE *f = m_files[FREE];

		size_t fsize = stat_f(f->_fileno);

		if (fsize == std::string::npos)
			return {ResultType::IoFailure, "could not stat one of db files"};

		uint8_t endian = handle_endian(f);

		while (ftell(f) < fsize)
		{
			auto offset = read_n<size_t>(f, endian);
			auto length = read_n<uint32_t>(f, endian);

			m_context.free_list.emplace(offset, length);
		}

		f = fdopen(f->_fileno, "wb+");

		return {};
	}

	template<class T>
	void write_n(T n, FILE *stream)
	{
		fwrite((char*)&n, 1, sizeof(T), stream);
	}

	void IoManager::flush_changelog()
	{
		FILE *dat = m_files[DAT];

		rewind(dat);

		auto data = (const char*)m_context.data.data();

		for (auto [offset, length] : m_context.changelog)
		{
			write_dat(data+offset, offset, length);
		}

		m_context.changelog.clear();
	}

	void IoManager::insert(Entry &entry)
	{
		FILE *idx = m_files[IDX];

		fseek(idx, 0, SEEK_END);

		std::string_view key = entry.first;
		IndexData &data = entry.second;

		write_n((uint16_t)key.size(), idx);

		data.idx_offset = ftell(idx);

		write_n((uint8_t)1, idx);

		fwrite(key.data(), key.size(), sizeof(char), idx);

		write_n(data.offset, idx);
		write_n(data.length, idx);
	}

	void IoManager::erase(Entry &entry)
	{
		FILE *idx = m_files[IDX];

		fseek(idx, entry.second.idx_offset, SEEK_SET);

		fputc(0, idx);
	}

	void IoManager::update(Entry &entry)
	{
		FILE *idx = m_files[IDX];

		IndexData data = entry.second;

		fseek(idx, data.idx_offset + entry.first.size() + 1, SEEK_SET);

		write_n(data.offset, idx);
		write_n(data.length, idx);
	}

	void IoManager::update_freelist(size_t offset, uint32_t size)
	{
		FILE *f = m_files[FREE];

		write_n(offset, f);
		write_n(size, f);
	}

	size_t IoManager::write_dat(const char *bytes, size_t offset, uint32_t size)
	{
		FILE *f = m_files[DAT];

		if (offset == std::string::npos)
		{
			fseek(f, 0, SEEK_END);
			offset = ftell(f);
		}

		fseek(f, offset, SEEK_SET);

		fwrite(bytes, size, 1, f);

		return offset;
	}

	std::string IoManager::read_dat(size_t offset, uint32_t size)
	{
		FILE *f = m_files[DAT];

		fseek(f, offset, SEEK_SET);

		auto data = m_context.data.data()+offset;

		std::string buff;

		buff.reserve(size+1);
		
		for (uint32_t i = 0; i < size; i++)
		{
			buff += fgetc(f);
		}

		return buff;
	}

	void IoManager::launch_timed_flush()
	{
		using namespace std::chrono;

		std::thread 
		{
		[&]
		{
			while (m_context.options.flush_mode == FlushMode::Periodic)
			{
				std::this_thread::sleep_for(milliseconds(m_context.options.flush_time));

				std::scoped_lock lock { m_mutex };

				flush_changelog();
			}
		}
		}.detach();
	}
};