#include "snappy.h"
#include "snappy-sinksource.h"
#include <vector>
#include <iostream>
#include <cstring>

/** Source accepting a data stream. Used for Snappy compression library. */
class DataStreamSource : public snappy::Source
{
public:
	DataStreamSource(char* stream, int size)
		: mStream(stream), mRemaining(size)
	{
	}

	virtual ~DataStreamSource()
	{
		free(mStream);
	}

	size_t Available() const override
	{
		return mRemaining;
	}

	const char* Peek(size_t* len) override
	{
		*len = Available();
		return (char*)mStream + mBufferOffset;
	}

	void Skip(size_t n) override
	{
		mBufferOffset += n;
		mRemaining -= n;
	}
private:
	char* mStream;

	// File streams only
	char* mReadBuffer = nullptr;
	size_t mReadBufferContentSize = 0;
	size_t mRemaining;
	size_t mBufferOffset = 0;
};

/** Sink (destination) accepting a data stream. Used for Snappy compression library. */
class DataStreamSink : public snappy::Sink
{
	struct BufferPiece
	{
		char* buffer;
		size_t size;
	};

public:
	DataStreamSink() = default;
	virtual ~DataStreamSink()
	{
		for (auto& entry : mBufferPieces)
			free(entry.buffer);
	}

	void Append(const char* data, size_t n) override
	{
		if(mBufferPieces.size() == 0 || mBufferPieces.back().buffer != data)
		{
			BufferPiece piece;
			piece.buffer = (char*)malloc(n);
			piece.size = n;

			memcpy(piece.buffer, data, n);
			mBufferPieces.push_back(piece);
		}
		else
		{
			BufferPiece& piece = mBufferPieces.back();

			piece.size = n;
		}
	}

	char* GetAppendBuffer(size_t len, char* scratch) override
	{
		BufferPiece piece;
		piece.buffer = (char*)malloc(len);
		piece.size = 0;

		mBufferPieces.push_back(piece);
		return piece.buffer;
	}

	char* GetAppendBufferVariable(size_t min_size, size_t desired_size_hint, char* scratch, size_t scratch_size,
		size_t* allocated_size) override
	{
		BufferPiece piece;
		piece.buffer = (char*)malloc(desired_size_hint);
		piece.size = 0;

		mBufferPieces.push_back(piece);

		*allocated_size = desired_size_hint;
		return piece.buffer;
	}

	void AppendAndTakeOwnership(char* bytes, size_t n, void(*deleter)(void*, const char*, size_t),
		void *deleter_arg) override
	{
		BufferPiece& piece = mBufferPieces.back();

		if (piece.buffer != bytes)
		{
			memcpy(piece.buffer, bytes, n);
			(*deleter)(deleter_arg, bytes, n);
		}

		piece.size = n;
	}

	char* GetOutput()
	{
		size_t totalSize = 0;
		for (auto& entry : mBufferPieces)
			totalSize += entry.size;

		char* data = (char*)malloc(totalSize);
		char* dataPtr = data;
		for (auto& entry : mBufferPieces)
		{
			memcpy(dataPtr, entry.buffer, entry.size);
			dataPtr += entry.size;
		}

		return data;
	}

private:
	std::vector<BufferPiece> mBufferPieces;
};

int main()
{
	const char* stuffToCompress = "Compress me";

	char* buffer = (char*)malloc(sizeof(stuffToCompress));
	memcpy(buffer, stuffToCompress, sizeof(stuffToCompress));

	{
		DataStreamSource src(buffer, sizeof(stuffToCompress));
		DataStreamSink dst;

		size_t bytesWritten = snappy::Compress(&src, &dst);

		for(int i = 0; i < bytesWritten; i++)
			std::cout << dst.GetOutput()[i];
	}

	return 0;
}