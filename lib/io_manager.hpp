#pragma once

// an object concerned with all matters of file io

#include "types.hpp"
#include <cstdint>
#include <array>

#include <fcntl.h>

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
			m_ctx(context)
		{}

		IoManager(IoManager &&im, DBContext &context) :
			m_ctx(context),
			m_files(std::move(im.m_files)),
			m_file_ext(std::move(im.m_file_ext))
		{}

		~IoManager();

		Result load_structures();

		Result open_files();

		Result destroy();

		void cleanup();

		void flush_freelist();

		void insert(Entry &entry);

		void update(Entry &entry);

		void erase(Entry &entry);

		size_t update_freelist(size_t offset, uint32_t size);

		void erase_freelist(uint64_t offset);

		size_t write_dat(const char *bytes, size_t offset, uint32_t size);

		std::string read_dat(size_t offset, uint32_t size);

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

	private:
		DBContext &m_ctx;

		std::array<int, 3> m_files;

		const std::array<std::string_view, 3> m_file_ext 
		{
			".dat", ".idx", ".free"
		};
	};
}