# AsioFileCopy
Windows specific application that demonstrates the use of Asio random access handles in a file copy application.
This a port of a IOCP_FileCopy to use Asio. 

##Architecture
The application consists of three parts. 

First source file is opened. Then the destination file is created and its size set to a (roof(source_file_size/buffer_size))*buffer_size. The buffer_size chosen is 2048.

Next the file is copied using Asio

Finally the destination file is reopened and set to the source file size. 

The reason for this three phase technique is that asynchronous write is done using only multiple chunks of block size.

