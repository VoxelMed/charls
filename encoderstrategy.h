// 
// (C) Jan de Vaan 2007-2010, all rights reserved. See the accompanying "License.txt" for licensed use. 
// 

#ifndef CHARLS_ENCODERSTRATEGY
#define CHARLS_ENCODERSTRATEGY

#include "processline.h"

// Purpose: Implements encoding to stream of bits. In encoding mode JpegLsCodec inherits from EncoderStrategy
class EncoderStrategy
{

public:
	explicit EncoderStrategy(const JlsParameters& info) :
		_info(info),
		valcurrent(0),
		bitpos(0),
		_compressedLength(0),
		_position(0),
		_isFFWritten(false),
		_bytesWritten(0),
		_compressedStream(nullptr)
	{
	}

	virtual ~EncoderStrategy() 
	{
	}

	LONG PeekByte();

	void OnLineBegin(LONG cpixel, void* ptypeBuffer, LONG pixelStride)
	{
		_processLine->NewLineRequested(ptypeBuffer, cpixel, pixelStride);
	}

	void OnLineEnd(LONG /*cpixel*/, void* /*ptypeBuffer*/, LONG /*pixelStride*/) { }

	virtual void SetPresets(const JlsCustomParameters& presets) = 0;

	virtual std::size_t EncodeScan(std::unique_ptr<ProcessLine> rawData, ByteStreamInfo* compressedData, void* pvoidCompare) = 0;

	virtual ProcessLine* CreateProcess(ByteStreamInfo rawStreamInfo) = 0;

protected:

	void Init(ByteStreamInfo* compressedStream)
	{
		bitpos = 32;
		valcurrent = 0;

		if (compressedStream->rawStream)
		{
			_compressedStream = compressedStream->rawStream;
			_buffer.resize(4000);
			_position = (BYTE*) &_buffer[0];
			_compressedLength = _buffer.size();
		}
		else
		{
			_position = compressedStream->rawData;
			_compressedLength = compressedStream->count;
		}
	}

	void AppendToBitStream(LONG value, LONG length)
	{
		ASSERT(length < 32 && length >= 0);
		ASSERT((!_qdecoder) || (length == 0 && value == 0) ||( _qdecoder->ReadLongValue(length) == value));

#ifndef NDEBUG
		if (length < 32)
		{
			int mask = (1 << (length)) - 1;
			ASSERT((value | mask) == mask);
		}
#endif

		bitpos -= length;
		if (bitpos >= 0)
		{
			valcurrent = valcurrent | (value << bitpos);
			return;
		}
		valcurrent |= value >> -bitpos;

		Flush();

		ASSERT(bitpos >=0);
		valcurrent |= value << bitpos;
	}

	void EndScan()
	{
		Flush();

		// if a 0xff was written, Flush() will force one unset bit anyway
		if (_isFFWritten)
			AppendToBitStream(0, (bitpos - 1) % 8);
		else
			AppendToBitStream(0, bitpos % 8);

		Flush();
		ASSERT(bitpos == 0x20);

		if (_compressedStream)
		{
			OverFlow();
		}
	}

	void OverFlow()
	{
		if (!_compressedStream)
			throw JlsException(CompressedBufferTooSmall);

		std::size_t bytesCount = _position-(BYTE*)&_buffer[0];
		std::size_t bytesWritten = (std::size_t)_compressedStream->sputn((char*)&_buffer[0], _position - (BYTE*)&_buffer[0]);

		if (bytesWritten != bytesCount)
			throw JlsException(CompressedBufferTooSmall);

		_position = (BYTE*)&_buffer[0];
		_compressedLength = _buffer.size();
	}

	void Flush()
	{
		if (_compressedLength < 4 && _compressedStream)
		{
			OverFlow();
		}

		for (LONG i = 0; i < 4; ++i)
		{
			if (bitpos >= 32)
				break;

			if (_isFFWritten)
			{
				// insert highmost bit
				*_position = BYTE(valcurrent >> 25);
				valcurrent = valcurrent << 7;
				bitpos += 7;
			}
			else
			{
				*_position = BYTE(valcurrent >> 24);
				valcurrent = valcurrent << 8;
				bitpos += 8;
			}

			_isFFWritten = *_position == 0xFF;
			_position++;
			_compressedLength--;
			_bytesWritten++;
		}
	}

	std::size_t GetLength()
	{
		return _bytesWritten - (bitpos -32)/8;
	}

	inlinehint void AppendOnesToBitStream(LONG length)
	{
		AppendToBitStream((1 << length) - 1, length);
	}

	std::unique_ptr<DecoderStrategy> _qdecoder;

protected:
	JlsParameters _info;
	std::unique_ptr<ProcessLine> _processLine;

private:
	unsigned int valcurrent;
	LONG bitpos;
	std::size_t _compressedLength;

	// encoding
	BYTE* _position;
	bool _isFFWritten;
	std::size_t _bytesWritten;

	std::vector<BYTE> _buffer;
	std::basic_streambuf<char>* _compressedStream;
};

#endif