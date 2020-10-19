#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>

class SHA1
{
public:
	enum { BlockSize = 512 / 8, HashBytes = 20 };
	SHA1()
	{
		reset();
	}
	~SHA1() = default;
public:
	void Update(const void* data, size_t numBytes)
	{
		const uint8_t* current = (const uint8_t*)data;
		if (m_bufferSize > 0)
		{
			while (numBytes > 0 && m_bufferSize < BlockSize)
			{
				m_buffer[m_bufferSize++] = *current++;
				numBytes--;
			}
		}

		// full buffer
		if (m_bufferSize == BlockSize)
		{
			processBlock((void*)m_buffer);
			m_numBytes += BlockSize;
			m_bufferSize = 0;
		}

		// no more data ?
		if (numBytes == 0)
			return;

		// process full blocks
		while (numBytes >= BlockSize)
		{
			processBlock(current);
			current += BlockSize;
			m_numBytes += BlockSize;
			numBytes -= BlockSize;
		}

		// keep remaining bytes in buffer
		while (numBytes > 0)
		{
			m_buffer[m_bufferSize++] = *current++;
			numBytes--;
		}
	}
	void Update(const std::string& data)
	{
		Update(data.data(), data.length());
	}

	std::vector<uint8_t> Final(void)
	{
		std::vector<uint8_t> buffer;
		buffer.resize(HashBytes);
		// process remaining bytes
		processBuffer();

		unsigned char* current = buffer.data();
		for (int i = 0; i < HashValues; i++)
		{
			*current++ = (m_hash[i] >> 24) & 0xFF;
			*current++ = (m_hash[i] >> 16) & 0xFF;
			*current++ = (m_hash[i] >> 8) & 0xFF;
			*current++ = m_hash[i] & 0xFF;
		}
		reset();
		return buffer;
	}
private:
	void reset()
	{
		m_numBytes = 0;
		m_bufferSize = 0;

		// according to RFC 1321
		m_hash[0] = 0x67452301;
		m_hash[1] = 0xefcdab89;
		m_hash[2] = 0x98badcfe;
		m_hash[3] = 0x10325476;
		m_hash[4] = 0xc3d2e1f0;
	}
	/// process 64 bytes
	void processBlock(const void* data)
	{
		uint32_t a = m_hash[0];
		uint32_t b = m_hash[1];
		uint32_t c = m_hash[2];
		uint32_t d = m_hash[3];
		uint32_t e = m_hash[4];

		// data represented as 16x 32-bit words
		const uint32_t* input = (uint32_t*)data;
		// convert to big endian
		uint32_t words[80];
		for (int i = 0; i < 16; i++)
#if defined(__BYTE_ORDER) && (__BYTE_ORDER != 0) && (__BYTE_ORDER == __BIG_ENDIAN)
			words[i] = input[i];
#else
			words[i] = swap(input[i]);
#endif

		// extend to 80 words
		for (int i = 16; i < 80; i++)
			words[i] = rotate(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);

		// first round
		for (int i = 0; i < 4; i++)
		{
			int offset = 5 * i;
			e += rotate(a, 5) + f1(b, c, d) + words[offset] + 0x5a827999; b = rotate(b, 30);
			d += rotate(e, 5) + f1(a, b, c) + words[offset + 1] + 0x5a827999; a = rotate(a, 30);
			c += rotate(d, 5) + f1(e, a, b) + words[offset + 2] + 0x5a827999; e = rotate(e, 30);
			b += rotate(c, 5) + f1(d, e, a) + words[offset + 3] + 0x5a827999; d = rotate(d, 30);
			a += rotate(b, 5) + f1(c, d, e) + words[offset + 4] + 0x5a827999; c = rotate(c, 30);
		}

		// second round
		for (int i = 4; i < 8; i++)
		{
			int offset = 5 * i;
			e += rotate(a, 5) + f2(b, c, d) + words[offset] + 0x6ed9eba1; b = rotate(b, 30);
			d += rotate(e, 5) + f2(a, b, c) + words[offset + 1] + 0x6ed9eba1; a = rotate(a, 30);
			c += rotate(d, 5) + f2(e, a, b) + words[offset + 2] + 0x6ed9eba1; e = rotate(e, 30);
			b += rotate(c, 5) + f2(d, e, a) + words[offset + 3] + 0x6ed9eba1; d = rotate(d, 30);
			a += rotate(b, 5) + f2(c, d, e) + words[offset + 4] + 0x6ed9eba1; c = rotate(c, 30);
		}

		// third round
		for (int i = 8; i < 12; i++)
		{
			int offset = 5 * i;
			e += rotate(a, 5) + f3(b, c, d) + words[offset] + 0x8f1bbcdc; b = rotate(b, 30);
			d += rotate(e, 5) + f3(a, b, c) + words[offset + 1] + 0x8f1bbcdc; a = rotate(a, 30);
			c += rotate(d, 5) + f3(e, a, b) + words[offset + 2] + 0x8f1bbcdc; e = rotate(e, 30);
			b += rotate(c, 5) + f3(d, e, a) + words[offset + 3] + 0x8f1bbcdc; d = rotate(d, 30);
			a += rotate(b, 5) + f3(c, d, e) + words[offset + 4] + 0x8f1bbcdc; c = rotate(c, 30);
		}

		// fourth round
		for (int i = 12; i < 16; i++)
		{
			int offset = 5 * i;
			e += rotate(a, 5) + f2(b, c, d) + words[offset] + 0xca62c1d6; b = rotate(b, 30);
			d += rotate(e, 5) + f2(a, b, c) + words[offset + 1] + 0xca62c1d6; a = rotate(a, 30);
			c += rotate(d, 5) + f2(e, a, b) + words[offset + 2] + 0xca62c1d6; e = rotate(e, 30);
			b += rotate(c, 5) + f2(d, e, a) + words[offset + 3] + 0xca62c1d6; d = rotate(d, 30);
			a += rotate(b, 5) + f2(c, d, e) + words[offset + 4] + 0xca62c1d6; c = rotate(c, 30);
		}

		// update hash
		m_hash[0] += a;
		m_hash[1] += b;
		m_hash[2] += c;
		m_hash[3] += d;
		m_hash[4] += e;
	}

	void processBuffer(void)
	{
		size_t paddedLength = m_bufferSize * 8;

		// plus one bit set to 1 (always appended)
		paddedLength++;

		// number of bits must be (numBits % 512) = 448
		size_t lower11Bits = paddedLength & 511;
		if (lower11Bits <= 448)
			paddedLength += 448 - lower11Bits;
		else
			paddedLength += 512 + 448 - lower11Bits;
		// convert from bits to bytes
		paddedLength /= 8;

		// only needed if additional data flows over into a second block
		unsigned char extra[BlockSize];

		// append a "1" bit, 128 => binary 10000000
		if (m_bufferSize < BlockSize)
			m_buffer[m_bufferSize] = 128;
		else
			extra[0] = 128;

		size_t i;
		for (i = m_bufferSize + 1; i < BlockSize; i++)
			m_buffer[i] = 0;
		for (; i < paddedLength; i++)
			extra[i - BlockSize] = 0;

		// add message length in bits as 64 bit number
		uint64_t msgBits = 8 * (m_numBytes + m_bufferSize);
		// find right position
		unsigned char* addLength;
		if (paddedLength < BlockSize)
			addLength = m_buffer + paddedLength;
		else
			addLength = extra + paddedLength - BlockSize;

		// must be big endian
		*addLength++ = (unsigned char)((msgBits >> 56) & 0xFF);
		*addLength++ = (unsigned char)((msgBits >> 48) & 0xFF);
		*addLength++ = (unsigned char)((msgBits >> 40) & 0xFF);
		*addLength++ = (unsigned char)((msgBits >> 32) & 0xFF);
		*addLength++ = (unsigned char)((msgBits >> 24) & 0xFF);
		*addLength++ = (unsigned char)((msgBits >> 16) & 0xFF);
		*addLength++ = (unsigned char)((msgBits >> 8) & 0xFF);
		*addLength = (unsigned char)(msgBits & 0xFF);

		// process blocks
		processBlock(m_buffer);
		// flowed over into a second block ?
		if (paddedLength > BlockSize)
			processBlock(extra);
	}

	static uint32_t f1(uint32_t b, uint32_t c, uint32_t d)
	{
		return d ^ (b & (c ^ d)); // original: f = (b & c) | ((~b) & d);
	}

	static uint32_t f2(uint32_t b, uint32_t c, uint32_t d)
	{
		return b ^ c ^ d;
	}

	static uint32_t f3(uint32_t b, uint32_t c, uint32_t d)
	{
		return (b & c) | (b & d) | (c & d);
	}

	static uint32_t rotate(uint32_t a, uint32_t c)
	{
		return (a << c) | (a >> (32 - c));
	}

	static uint32_t swap(uint32_t x)
	{
		return (x >> 24) |
			((x >> 8) & 0x0000FF00) |
			((x << 8) & 0x00FF0000) |
			(x << 24);
	}
private:
	/// size of processed data in bytes
	uint64_t m_numBytes;
	/// valid bytes in m_buffer
	size_t   m_bufferSize;
	/// bytes not processed yet
	uint8_t  m_buffer[BlockSize];

	enum { HashValues = HashBytes / 4 };
	/// hash, stored as integers
	uint32_t m_hash[HashValues];
};
