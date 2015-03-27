#ifndef IO_STREAM_READER_H
#define IO_STREAM_READER_H

#include "io/IoSys.h"

namespace rde
{
class Stream;

// Stream reader.
// Default implementation simply forwards calls to underlying stream.
class StreamReader
{
public:
	explicit StreamReader(Stream* stream);
	virtual ~StreamReader();

	virtual long Read(void* data, long bytes);
	virtual int32 ReadInt32();
	virtual int16 ReadInt16();
	virtual void ReadASCIIZ(char* buffer, long bufferSize);
	virtual void Flush();
	virtual void Close();

	Stream* GetStream() const	{ return m_stream; }

protected:
	Stream*	m_stream;
};

} // rde

#endif // IO_STREAM_READER
