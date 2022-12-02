#pragma once

// an object concerned with all matters of file io

#include "types.hpp"
#include <cstdint>
#include <cstdio>
#include <array>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unordered_map>

namespace ambry
{
	class IoManager
	{
	public:
		struct Changelog
		{
			std::string_view key;

			size_t offset;
			uint32_t length;

			bool is_valid;
			bool insertion;
		};

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

		Result flush_free_list();

		Result flush_changelog();

		void update(size_t idx_offset, Changelog changelog);

	private:
		DBContext &m_context;

		std::map<size_t, size_t> m_free_changelog;
		std::multimap<uint32_t, Changelog> m_changelog;

		std::array<FILE*, 3> m_files;

		const std::array<std::string_view, 3> m_file_ext 
		{
			".dat", ".idx", ".free"
		};

		/*
			the index file format is as follows:
			2 bytes for the key length
			1 byte to determine if the key is valud. if its not valid it should skip to the next key
			the key
			8 bytes for the offset
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