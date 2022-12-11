#include "io_manager.hpp"
#include "types.hpp"
#include <bits/chrono.h>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <mutex>
#include <thread>

#include <algorithm>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/uio.h>

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
			flock(f, LOCK_UN);
			close(f);
		}
	}

	Result IoManager::open_files()
	{
		for (int i = 0; i < m_files.size(); i++)
		{
			std::string_view ext = m_file_ext[i];

			std::string name = (m_context.name + ext.data());

			int f = open(name.c_str(), O_NONBLOCK | O_RDWR | O_CREAT, 0777);

			if (f == -1)
			{
				return {ResultType::IoFailure, "could not open one of db files"};
			}

			flock(f, LOCK_EX);

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

	char read_byte(int fd)
	{
		char buff[1];
		read(fd, buff, 1);
		return *buff;
	}

	int write_byte(int fd, char byte)
	{
		return write(fd, &byte, 1);
	}

	template<class T>
	T read_n(int fd, uint8_t endian)
	{
		T n{};

		auto bytes = (char*)&n;

		read(fd, bytes, sizeof(T));

		if (machine_endian() != endian)
		{
			std::reverse(bytes, bytes+sizeof(T));
		}

		n = *(T*)bytes;
		
		return n;
	}

	size_t get_fsize(int fd)
	{
		struct stat st;

		if (fstat(fd, &st) == -1)
			return std::string::npos;

		return st.st_size;
	}

	uint8_t handle_endian(int fd)
	{
		uint8_t endian = read_byte(fd);

		lseek(fd, -1, SEEK_CUR);
		write_byte(fd, machine_endian());

		return endian;
	}

	int curr_offset(int fd)
	{
		return lseek(fd, 0, SEEK_CUR);
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
		int fd = m_files[IDX];

		size_t fsize = get_fsize(fd);

		if (fsize == std::string::npos)
		{
			return {ResultType::IoFailure, "could not stat one of db files"};
		}

		uint8_t endian = handle_endian(fd);

		while (curr_offset(fd) < fsize)
		{
			IndexData data;

			auto key_len = read_n<uint16_t>(fd, endian);

			data.idx_offset = curr_offset(fd);

			uint8_t is_valid = read_byte(fd);

			if (!is_valid)
			{
				uint64_t offset = key_len + sizeof(uint64_t) + sizeof(uint32_t);
				lseek(fd, offset, SEEK_CUR);
				continue;
			}

			std::string key;

			key.resize(key_len);

			read(fd, key.data(), key_len);

			data.offset = read_n<uint64_t>(fd, endian);
			data.length = read_n<uint32_t>(fd, endian);

			m_context.index.emplace(std::move(key), data);
		}

		return {};
	}

	Result IoManager::load_dat()
	{
		int fd = m_files[DAT];

		size_t fsize = get_fsize(fd);

		if (fsize == std::string::npos)
			return {ResultType::IoFailure, "could not stat one of db files"};

		m_context.data.reserve(fsize * 2);
		m_context.data.resize(fsize);

		read(fd, m_context.data.data(), fsize);

		return {};
	}

	/*
		the free list format is as follows:
		8 bytes for the offset
		4 bytes for the length
	*/
	Result IoManager::load_free()
	{
		int fd = m_files[FREE];

		size_t fsize = get_fsize(fd);

		if (fsize == std::string::npos)
			return {ResultType::IoFailure, "could not stat one of db files"};

		uint8_t endian = handle_endian(fd);

		while (curr_offset(fd) < fsize)
		{
			auto offset = read_n<size_t>(fd, endian);
			auto length = read_n<uint32_t>(fd, endian);

			if (offset == 0 && length == 0)
			{
				continue;
			}

			m_context.free_list.emplace(offset, length);
		}

		return {};
	}

	void IoManager::flush_changelog()
	{
		int fd = m_files[DAT];

		lseek(fd, 0, SEEK_SET);

		auto data = (const char*)m_context.data.data();

		for (auto [offset, length] : m_context.changelog)
		{
			write_dat(data+offset, offset, length);
		}

		m_context.changelog.clear();
	}

	void IoManager::insert(Entry &entry)
	{
		int fd = m_files[IDX];
		
		lseek(fd, 0, SEEK_END);

		std::string_view key = entry.first;
		IndexData &data = entry.second;

		data.idx_offset = curr_offset(fd)+2;

		constexpr int n = 5;

		uint16_t len = key.size();
		char b[1] = {1};

		iovec iov[n]
		{
			{(char*)&len, 2},
			{b, 1},
			{(void*)key.data(), key.size()},
			{(char*)&data.offset, 8},
			{(char*)&data.length, 4}
		};
		
		writev(fd, iov, n);
	}

	void IoManager::erase(Entry &entry)
	{
		int fd = m_files[IDX];

		lseek(fd, entry.second.idx_offset, SEEK_SET);

		write_byte(fd, 0);
	}

	void IoManager::update(Entry &entry)
	{
		int fd = m_files[IDX];

		IndexData data = entry.second;

		iovec iov[]
		{
			{(char*)&data.offset, 8},
			{(char*)&data.length, 4}
		};

		pwritev(fd, iov, 2, data.idx_offset + entry.first.size() + 1);
	}

	void IoManager::erase_freelist(uint64_t offset)
	{
		int fd = m_files[FREE];

		static const char data[12]{};

		pwrite(fd, data, 12, offset);
	}

	size_t IoManager::update_freelist(size_t offset, uint32_t size)
	{
		int fd = m_files[FREE];

		size_t n = lseek(fd, 0, SEEK_END);

		iovec iov[]
		{
			{(char*)&offset, 8},
			{(char*)&size, 4}
		};

		writev(fd, iov, 2);

		return n;
	}

	size_t IoManager::write_dat(const char *bytes, size_t offset, uint32_t size)
	{
		int fd = m_files[DAT];

		if (offset == std::string::npos)
		{
			offset = lseek(fd, 0, SEEK_END);
		}

		pwrite(fd, bytes, size, offset);

		return offset;
	}

	std::string IoManager::read_dat(size_t offset, uint32_t size)
	{
		int fd = m_files[DAT];

		std::string buff;

		buff.resize(size);

		pread(fd, buff.data(), size, offset);

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