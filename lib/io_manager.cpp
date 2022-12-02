#include "io_manager.hpp"
#include "types.hpp"
#include <cstdio>
#include <fcntl.h>

#include <algorithm>

#include "../fmt.hpp"

/*
	the first byte of all database files (except .dat) should be either 0 or 1 to determine endianness of the data
*/

namespace ambry
{
	IoManager::~IoManager()
	{
		if (m_context.options.flush_node == FlushMode::FlushAtEnd)
		{
			flush_changelog();
			flush_free_list();
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

		uint8_t endian = fgetc(f);

		while (ftell(f) < fsize)
		{
			IndexData data;

			auto key_len = read_n<uint16_t>(f, endian);

			data.idx_offset = ftell(f);

			uint8_t is_valid = fgetc(f);

			if (!is_valid)
			{
				size_t offset = key_len + sizeof(size_t) + sizeof(uint32_t);
				fseek(f, offset, SEEK_SET);
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

		uint8_t endian = fgetc(f);

		while (ftell(f) < fsize)
		{
			auto offset = read_n<size_t>(f, endian);
			auto length = read_n<uint32_t>(f, endian);

			m_context.free_list.emplace(offset, length);
		}

		return {};
	}

	template<class T>
	void write_n(T n, FILE *stream)
	{
		fwrite((char*)&n, 1, sizeof(T), stream);
	}

	Result IoManager::flush_free_list()
	{
		FILE *f = m_files[FREE];

		rewind(f);

		fputc(machine_endian(), f);

		for (auto [offset, size] : m_context.free_list)
		{
			write_n(offset, f);
			write_n(size, f);
		}

		return {};
	}

	Result IoManager::flush_changelog()
	{
		FILE *dat = m_files[DAT];
		FILE *idx = m_files[IDX];

		rewind(dat);
		rewind(idx);

		fputc(machine_endian(), idx);

		uint8_t *data = m_context.data.data();

		std::vector<Changelog> insertions;

		for (auto [idx_offset, changelog] : m_changelog)
		{	
			if(changelog.insertion)
			{
				insertions.push_back(changelog);
				goto write_dat;
			}

			fseek(idx, idx_offset, SEEK_SET);

			fputc(changelog.is_valid, idx);

			fseek(idx, changelog.key.size(), SEEK_SET);

			write_n(changelog.offset, idx);
			write_n(changelog.length, idx);

   write_dat:
			if (changelog.is_valid)
				fwrite(data+changelog.offset, changelog.length, sizeof(uint8_t), dat);
		}

		fseek(idx, 0, SEEK_END);

		for (auto changelog : insertions)
		{
			write_n((uint16_t)changelog.key.size(), idx);
			write_n((uint8_t)1, idx);
			
			fwrite(changelog.key.data(), changelog.key.size(), sizeof(uint8_t), idx);

			write_n(changelog.offset, idx);
			write_n(changelog.length, idx);
		}

		return {};
	}

	void IoManager::update(size_t idx_offset, Changelog changelog)
	{
		m_changelog.emplace(idx_offset, changelog);
	}

};