#pragma once

// an object concerned with all matters of file io

#include "types.hpp"
#include <cstdint>
#include <cstdio>
#include <array>

#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <sys/file.h>
#include <unordered_map>

namespace ambry
{
	class IoManager
	{
		using Entry = std::pair<const std::string, IndexData>;
	public:

		enum FType : uint8_t
		{
			DAT, IDX, FREE
		};

		IoManager(DBContext &context) :
			m_context(context)
		{}

		~IoManager();

		Result load_structures();

		Result open_files();

		void flush_freelist();

		void flush_changelog();

		void insert(Entry &entry);

		void update(Entry &entry);

		void erase(Entry &entry);

		void update_freelist(size_t offset, uint32_t size);

		void write_dat(size_t offset, uint32_t size);

		void launch_timed_flush();

	private:
		DBContext &m_context;
		std::mutex m_mutex;

		std::array<FILE*, 3> m_files;

		const std::array<std::string_view, 3> m_file_ext 
		{
			".dat", ".idx", ".free"
		};

		/*
			the index file format is as follows:
			2 bytes for the key length
			1 byte to determine if the key is valid. if its not valid it should skip to the next key
			the key
			8 bytes for the offset in the data file/cache
			4 bytes for the length of the data
		*/
		Result load_index();

		Result load_dat();

		/*
			the free list file format is as follows:
			8 bytes for the offset
			4 bytes for the length
		*/
		Result load_free();
		
	};
}