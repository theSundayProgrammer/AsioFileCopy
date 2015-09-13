// CopyFile.cpp : Defines the entry point for the console application.
//

#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <asio.hpp>
#include <asio/error.hpp>
const DWORD BUFFSIZE = (2 * 1024);  // The size of an I/O buffer
// roundup 
// returns  min{k: k >= Value && k%Multiple == 0}
template<typename T, typename S>
T roundup(T Value, S Multiple)
{
	T retVal = (Value / Multiple) * Multiple;
	//Invariant retVal % Multiple == 0 
	while (retVal < Value) 
		retVal += Multiple;
	return retVal;
}
using asio::windows::random_access_handle;

void write_data(
    random_access_handle& dest_file, 
    unsigned char* buffer_, 
    uint64_t file_offset,
    size_t buf_size, 
    size_t buf_offset=0)
{
	
	if (buf_size == 0)
	{
		delete[] buffer_;
		return;
	}

	dest_file.async_write_some_at(
        file_offset, 
        asio::buffer(buffer_ ,buf_size),
		[=, &dest_file]
			(const asio::error_code& error, // Result of operation.
				std::size_t bytes_transferred)
			{
					write_data(
						dest_file,
						buffer_,
						file_offset+bytes_transferred,
						buf_size- bytes_transferred,
						buf_offset+ bytes_transferred);
	       }
	);
}
void read_data(random_access_handle& src_file,
	random_access_handle& dest_file,
	uint64_t file_offset,
	size_t buf_size)
{
	unsigned char* buffer_ = new unsigned char[BUFFSIZE];
	src_file.async_read_some_at(file_offset, asio::buffer(buffer_, BUFFSIZE),
		[buffer_, file_offset, &dest_file,&src_file](const asio::error_code& error, // Result of operation.
			std::size_t bytes_transferred) // Number of bytes written)
	{
		if (bytes_transferred > 0)
			write_data(dest_file, buffer_, file_offset, BUFFSIZE);
		if (bytes_transferred == BUFFSIZE)
			read_data(src_file, dest_file, file_offset + bytes_transferred, BUFFSIZE);

	});
}

int main(int argc, char* argv[])
{
	LARGE_INTEGER liFileSizeSrc;
	if (argc<3)
	{
		printf("Usage: %s DestinationFile SourceFile\n", argv[0]);
		return 0;
	}
	DWORD k = GetTickCount();
	{
        asio::io_context io_context;
		//Open Source File
		//Create Destination file
		//Set destination file size to multiple of BUFFSIZE 
		//	but greater than or equal to Source File size
		random_access_handle src_file(io_context);
        random_access_handle dest_file(io_context);
        HANDLE hfileSrc = CreateFile(
			argv[2],
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
			NULL
			);
		if (hfileSrc == INVALID_HANDLE_VALUE)
		{
			printf("invalid source file\n");
			return 1;
		}
        asio::error_code ec;
        src_file.assign(hfileSrc,ec);


		HANDLE hfileDst = CreateFile(
			argv[1],
			GENERIC_WRITE,
			0,
			NULL,
			CREATE_ALWAYS,
			FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
			hfileSrc
			);
		if (hfileDst == INVALID_HANDLE_VALUE)
		{
			printf("invalid destination file\n");
			return 2;
		}
		GetFileSizeEx(hfileSrc, &liFileSizeSrc);
		LARGE_INTEGER liFileSizeDst;
		liFileSizeDst.QuadPart = roundup(liFileSizeSrc.QuadPart, BUFFSIZE);
		SetFilePointerEx(hfileDst, liFileSizeDst, NULL, FILE_BEGIN);
		SetEndOfFile(hfileDst);
        dest_file.assign(hfileDst,ec);
        uint64_t offset=0;
        unsigned char* buffer_ = new unsigned char[BUFFSIZE];
		read_data(src_file, dest_file, offset, BUFFSIZE);
       
        io_context.run();
	}
	{
		//Open destination file again and truncate its size
		HANDLE hfileDst = CreateFile
			(
			argv[1],
			GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING, 
			0,
			NULL
			);
		if (hfileDst != INVALID_HANDLE_VALUE)
		{

			SetFilePointerEx(hfileDst, liFileSizeSrc, NULL, FILE_BEGIN);
			SetEndOfFile(hfileDst);
            CloseHandle(hfileDst);
		}
	}
	printf("time taken=%d\n", GetTickCount() - k);
	return 0;
}
