#pragma once
#include <cstdint>
#include <memory>

class DataBufferPrivateImpl;
class DataBuffer
{
	friend class DataBufferView;
	friend class DataBufferIterator;
public:
	DataBuffer(const DataBuffer&) = delete;
	DataBuffer(void);
	DataBuffer(DataBuffer&& value);
	~DataBuffer(void);
public:
	size_t Size(void) const;
	size_t GapSize(void) const;
	size_t CopyData(size_t index, size_t size, void* buffer) const;
	uint8_t GetAt(size_t index) const;
	void SetAt(size_t index, uint8_t value);
	void Append(const void* data, size_t size);
	void AppendFill(uint8_t fill, size_t size);
	void Prepend(const void* data, size_t size);
	void PrependFill(uint8_t fill, size_t size);
	void Insert(const void* data, size_t index, size_t size);
	void InsertFill(size_t index, size_t size, uint8_t fill);
	void Replace(const void* dest,size_t destSize, size_t srcIndex, size_t srcSize);
	void Fill(uint8_t fill, size_t index, size_t size);
	void Remove(size_t index, size_t size);
	void Compress(void);
	void Resize(size_t size);
	void Clear(void);
private:
	std::unique_ptr<DataBufferPrivateImpl> m_Impl;
};

class DataBufferView
{
public:
	DataBufferView(void) = delete;
	DataBufferView(const DataBufferView& view);
	DataBufferView(const DataBuffer& buffer);
	~DataBufferView() = default;
public:
	size_t Position(void) const { return m_Position; }
	void Position(size_t pos);
	size_t GetData(void* buffer, size_t size);
	int GetByte(void);
	bool EndOfBuffer(void) const;
private:
	const DataBuffer& m_DataBuffer;
	size_t m_Position;
	void* m_pBuffer;
};

class DataBufferIterator
{
public:
	DataBufferIterator(void) = delete;
	DataBufferIterator(const DataBufferIterator& it);
	DataBufferIterator(DataBufferIterator&& it);
	DataBufferIterator(const DataBuffer& buffer);
	~DataBufferIterator() = default;
public:
	bool Next(void);
	bool Prev(void);
public:
	const uint8_t* Data(void) const;
	const size_t Length(void) const;
private:
	void* m_pPrivateImpl;
};
