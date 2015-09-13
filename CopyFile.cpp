// CopyFile.cpp : Defines the entry point for the console application.
//


#include <deque>
#include <iostream>
#include <thread>
#include <asio.hpp>
#include <exception>
#include <tuple>
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

	asio::async_write_at(
		dest_file,
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
	asio::async_read_at(src_file,file_offset, asio::buffer(buffer_, BUFFSIZE),
		[buffer_, file_offset, &dest_file,&src_file](const asio::error_code& error, // Result of operation.
			std::size_t bytes_transferred) // Number of bytes written)
	{
		if (bytes_transferred > 0)
			write_data(dest_file, buffer_, file_offset, BUFFSIZE);
		else
		{
			delete[] buffer_;
		}
		if (bytes_transferred == BUFFSIZE)
			read_data(src_file, dest_file, file_offset + bytes_transferred, BUFFSIZE);

	});
}
void truncate_file(char* fName, LARGE_INTEGER siz)
{
	//Open destination file again and truncate its size
	HANDLE hfileDst = CreateFile
		(
			fName,
			GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			0,
			NULL
			);
	if (hfileDst != INVALID_HANDLE_VALUE)
	{

		SetFilePointerEx(hfileDst, siz, NULL, FILE_BEGIN);
		SetEndOfFile(hfileDst);
		CloseHandle(hfileDst);
	}
}

std::tuple<HANDLE, HANDLE,LARGE_INTEGER> get_file_handles(char* src_fname, char* dest_fname)
{
	LARGE_INTEGER liFileSizeSrc;
	//Open Source File
	//Create Destination file
	//Set destination file size to multiple of BUFFSIZE 
	//	and greater than or equal to Source File size
	HANDLE hfileSrc = CreateFile(
		src_fname,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
		NULL
		);
	if (hfileSrc == INVALID_HANDLE_VALUE)
	{
		throw std::invalid_argument(src_fname);
	}

	HANDLE hfileDst = CreateFile(
		dest_fname,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
		hfileSrc
		);
	if (hfileDst == INVALID_HANDLE_VALUE)
	{
		CloseHandle(hfileSrc);
		throw std::invalid_argument(dest_fname);
	}
	GetFileSizeEx(hfileSrc, &liFileSizeSrc);
	LARGE_INTEGER liFileSizeDst;
	liFileSizeDst.QuadPart = roundup(liFileSizeSrc.QuadPart, BUFFSIZE);
	SetFilePointerEx(hfileDst, liFileSizeDst, NULL, FILE_BEGIN);
	SetEndOfFile(hfileDst);
	return std::make_tuple(hfileSrc, hfileDst, liFileSizeSrc);
}

int main(int argc, char* argv[])
{
	LARGE_INTEGER liFileSizeSrc;
	if (argc<3)
	{
		std::cout << "Usage: " << argv[0] << " DestinationFile SourceFile" << std::endl;
		return 0;
	}
	DWORD k = GetTickCount();
	try
	{
        asio::io_context io_context;
		random_access_handle src_file(io_context);
        random_access_handle dest_file(io_context);
		asio::error_code ec;
		HANDLE src, dest;
		std::tie(src,dest, liFileSizeSrc)= get_file_handles(argv[2], argv[1]);
		src_file.assign(src, ec);

		dest_file.assign(dest,ec);

        uint64_t offset=0;
		read_data(src_file, dest_file, offset, BUFFSIZE);
       
        io_context.run();
	}
	catch (std::exception& ex)
	{
		std::cout << "Exception " << ex.what() << std::endl;
		return 1;
	}
	truncate_file(argv[1], liFileSizeSrc);
	std::cout << "time taken=" << GetTickCount() - k << std::endl;
	return 0;
}
