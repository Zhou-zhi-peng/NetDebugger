#include "pch.h"
#include "DataBuffer.h"
#include "fast_memcpy.hpp"

constexpr size_t kBLOCK_DATA_SIZE = 8192;
class DataBufferPrivateImpl
{
public:
	struct BufferBlock
	{
		BufferBlock* prev;
		BufferBlock* next;
		size_t used;
		uint8_t data[kBLOCK_DATA_SIZE];
	};

	DataBufferPrivateImpl():
		m_BlockFirst(nullptr),
		m_BlockLast(nullptr),
		m_BlockCount(0),
		m_TotalUsed(0)
	{
		m_BlockFirst = new BufferBlock();
		m_BlockFirst->prev = nullptr;
		m_BlockFirst->next = nullptr;
		m_BlockFirst->used = 0;
		m_BlockLast = m_BlockFirst;
		m_BlockCount = 1;
	}
	~DataBufferPrivateImpl()
	{
		Clear(true);
	}

	void Clear(bool all)
	{
		if (m_BlockCount == 0)
			return;

		BufferBlock* next = m_BlockLast;
		for (BufferBlock* block = m_BlockFirst; block != m_BlockLast; block = next)
		{
			next = block->next;
			delete block;
		}
		m_BlockFirst = m_BlockLast;

		if (all)
		{
			delete m_BlockFirst;
			m_BlockFirst = nullptr;
			m_BlockLast = nullptr;
			m_TotalUsed = 0;
			m_BlockCount = 0;
		}
		else
		{
			m_BlockLast->used = 0;
			m_TotalUsed = 0;
			m_BlockCount = 1;
		}
	}

	BufferBlock* RemoveBlock(BufferBlock* atBlock)
	{
		if (atBlock == m_BlockFirst)
		{
			atBlock->used = 0;
			return atBlock->next;
		}
		--m_BlockCount;
		if (atBlock == m_BlockLast)
		{
			m_BlockLast = m_BlockLast->prev;
			m_BlockLast->next = nullptr;
			atBlock->used = 0;
			delete atBlock;
			return nullptr;
		}

		auto prev = atBlock->prev;
		auto next = atBlock->next;
		prev->next = next;
		next->prev = prev;
		atBlock->used = 0;
		delete atBlock;
		return next;
	}

	BufferBlock* AfterBlock(BufferBlock* atBlock)
	{
		BufferBlock* block = new BufferBlock();
		block->prev = atBlock;
		block->next = atBlock->next;
		block->used = 0;
		atBlock->next = block;
		if (block->next != nullptr)
			block->next->prev = block;
		if (atBlock == m_BlockLast)
			m_BlockLast = block;
		++m_BlockCount;
		return block;
	}

	BufferBlock* BeforeBlock(BufferBlock* atBlock)
	{
		BufferBlock* block = new BufferBlock();
		block->prev = atBlock->prev;
		block->next = atBlock;
		block->used = 0;
		atBlock->prev = block;
		if (block->prev != nullptr)
			block->prev->next = block;
		if (atBlock == m_BlockFirst)
			m_BlockFirst = block;
		++m_BlockCount;
		return block;
	}

	BufferBlock* PrependBlock()
	{
		BufferBlock* block = new BufferBlock();
		block->prev = nullptr;
		block->next = m_BlockFirst;
		block->used = 0;
		m_BlockFirst->prev = block;
		m_BlockFirst = block;
		++m_BlockCount;
		return block;
	}

	BufferBlock* AppendBlock()
	{
		BufferBlock* block = new BufferBlock();
		block->prev = m_BlockLast;
		block->next = nullptr;
		block->used = 0;
		m_BlockLast->next = block;
		m_BlockLast = block;
		++m_BlockCount;
		return block;
	}

	BufferBlock* FindBlockByOffset(size_t& offset)
	{
		for (BufferBlock* block = m_BlockFirst; block != nullptr; block = block->next)
		{
			if (offset < block->used)
				return block;
			offset -= block->used;
		}
		return nullptr;
	}

	size_t CopyData(BufferBlock* start, size_t offset, size_t size, void* buffer, BufferBlock** next = nullptr)
	{
		auto p = reinterpret_cast<uint8_t*>(buffer);
		auto count = size;

		auto rlen = start->used - offset;
		size_t len = (size < rlen) ? size : rlen;
		if (len > 0)
		{
			fast_memcpy(p, start->data + offset, len);
			size -= len;
			p += len;
		}
		start = start->next;

		while (size > 0 && start != nullptr)
		{
			size_t len = (size < start->used) ? size : start->used;
			if (len > 0)
			{
				fast_memcpy(p, start->data, len);
				size -= len;
				p += len;
			}
			start = start->next;
		}
		if (*next != nullptr)
		{
			*next = start;
		}
		return count - size;
	}

	BufferBlock* m_BlockFirst;
	BufferBlock* m_BlockLast;
	size_t m_BlockCount;
	size_t m_TotalUsed;
};

DataBuffer::DataBuffer()
{
	m_Impl.reset(new DataBufferPrivateImpl());
}

DataBuffer::DataBuffer(DataBuffer&& value)
{
	m_Impl.swap(value.m_Impl);
}

DataBuffer::~DataBuffer(void)
{
	m_Impl->Clear(true);
}

size_t DataBuffer::Size() const 
{ 
	return m_Impl->m_TotalUsed;
}

size_t DataBuffer::GapSize(void) const
{
	auto total = m_Impl->m_BlockCount * kBLOCK_DATA_SIZE;
	return total - Size() - (kBLOCK_DATA_SIZE - m_Impl->m_BlockLast->used);
}

size_t DataBuffer::CopyData(size_t index, size_t size, void* buffer) const
{
	auto block = m_Impl->FindBlockByOffset(index);
	if (block == nullptr)
		return 0;
	
	return m_Impl->CopyData(block, index, size, buffer);
}

uint8_t DataBuffer::GetAt(size_t index) const
{
	auto block = m_Impl->FindBlockByOffset(index);
	if (block == nullptr)
		return 0;
	return block->data[index];
}

void DataBuffer::SetAt(size_t index, uint8_t value)
{
	auto block = m_Impl->FindBlockByOffset(index);
	if (block == nullptr)
		return;
	block->data[index] = value;
}

void DataBuffer::Append(const void* data, size_t size)
{
	auto addsize = size;
	auto block = m_Impl->m_BlockLast;
	auto p = reinterpret_cast<const uint8_t*>(data);
	auto freeSize = kBLOCK_DATA_SIZE - block->used;
	if (size > freeSize)
	{
		fast_memcpy(block->data + block->used, p, freeSize);
		size -= freeSize;
		block->used += freeSize;
		p += freeSize;
		while (size > 0)
		{
			block = m_Impl->AfterBlock(block);
			auto len = size < kBLOCK_DATA_SIZE ? size : kBLOCK_DATA_SIZE;
			fast_memcpy(block->data + block->used, p, len);
			size -= len;
			p += len;
			block->used += len;
		}
	}
	else
	{
		fast_memcpy(block->data + block->used, p, size);
		block->used += size;
	}
	m_Impl->m_TotalUsed += addsize;
}

void DataBuffer::AppendFill(uint8_t fill, size_t size)
{
	auto addsize = size;
	auto block = m_Impl->m_BlockLast;
	auto freeSize = kBLOCK_DATA_SIZE - block->used;
	if (size > freeSize)
	{
		memset(block->data + block->used, fill, freeSize);
		size -= freeSize;
		block->used += freeSize;
		while (size > 0)
		{
			block = m_Impl->AfterBlock(block);
			auto len = size < kBLOCK_DATA_SIZE ? size : kBLOCK_DATA_SIZE;
			memset(block->data + block->used, fill, len);
			size -= len;
			block->used += len;
		}
	}
	else
	{
		memset(block->data + block->used, fill, size);
		block->used += size;
	}
	m_Impl->m_TotalUsed += addsize;
}

void DataBuffer::Prepend(const void* data, size_t size)
{
	auto addsize = size;
	auto block = m_Impl->m_BlockFirst;
	auto p = reinterpret_cast<const uint8_t*>(data);
	auto freeSize = kBLOCK_DATA_SIZE - block->used;
	if (size > freeSize)
	{
		memmove(block->data + freeSize, block->data, block->used);
		fast_memcpy(block->data, p + (size - freeSize), freeSize);
		size -= freeSize;
		block->used += freeSize;
		block = m_Impl->BeforeBlock(block);
		while (true)
		{
			auto len = size < kBLOCK_DATA_SIZE ? size : kBLOCK_DATA_SIZE;
			fast_memcpy(block->data + block->used, p, len);
			size -= len;
			p += len;
			block->used += len;
			if(size>0)
				block = m_Impl->AfterBlock(block);
			else
				break;
		}
	}
	else
	{
		memmove(block->data + size, block->data, block->used);
		fast_memcpy(block->data, p, size);
		block->used += size;
	}
	m_Impl->m_TotalUsed += addsize;
}

void DataBuffer::PrependFill(uint8_t fill, size_t size)
{
	auto addsize = size;
	auto block = m_Impl->m_BlockFirst;
	auto freeSize = kBLOCK_DATA_SIZE - block->used;
	if (size > freeSize)
	{
		memmove(block->data + freeSize, block->data, block->used);
		memset(block->data, fill, freeSize);
		size -= freeSize;
		block->used += freeSize;
		block = m_Impl->BeforeBlock(block);
		while (true)
		{
			auto len = size < kBLOCK_DATA_SIZE ? size : kBLOCK_DATA_SIZE;
			memset(block->data + block->used, fill, len);
			size -= len;
			block->used += len;
			if (size > 0)
				block = m_Impl->AfterBlock(block);
			else
				break;
		}
	}
	else
	{
		memmove(block->data + size, block->data, block->used);
		memset(block->data, fill, size);
		block->used += size;
	}
	m_Impl->m_TotalUsed += addsize;
}

void DataBuffer::Insert(const void* data, size_t index, size_t size)
{
	if (index >= Size())
	{
		Append(data, size);
		return;
	}

	if (index == 0)
	{
		Prepend(data, size);
		return;
	}

	auto addsize = size;
	auto block = m_Impl->FindBlockByOffset(index);
	if (block == nullptr)
		block = m_Impl->AppendBlock();
	auto p = reinterpret_cast<const uint8_t*>(data);
	auto freeSize = kBLOCK_DATA_SIZE - block->used;
	if (size > freeSize)
	{
		memmove(block->data + index + freeSize, block->data + index, block->used - index);
		fast_memcpy(block->data + index, p, freeSize);
		size -= freeSize;
		block->used += freeSize;
		p += freeSize;
		while (size > 0)
		{
			auto len = size < kBLOCK_DATA_SIZE ? size : kBLOCK_DATA_SIZE;
			if (block == nullptr || block->next == nullptr)
			{
				block = m_Impl->AfterBlock(block);
			}
			else
			{
				freeSize = kBLOCK_DATA_SIZE - block->next->used;
				if (freeSize >= len)
				{
					block = block->next;
					memmove(block->data + len, block->data, block->used);
				}
				else
				{
					block = m_Impl->AfterBlock(block);
				}
			}
			fast_memcpy(block->data, p, len);
			size -= len;
			p += len;
			block->used += len;
		}
	}
	else
	{
		memmove(block->data + index + size, block->data + index, block->used - index);
		fast_memcpy(block->data + index, p, size);
		block->used += size;
	}
	m_Impl->m_TotalUsed += addsize;
}

void DataBuffer::InsertFill(size_t index, size_t size, uint8_t fill)
{
	if (index >= Size())
	{
		AppendFill(fill, size);
		return;
	}

	if (index == 0)
	{
		PrependFill(fill, size);
		return;
	}

	auto addsize = size;
	auto block = m_Impl->FindBlockByOffset(index);
	if (block == nullptr)
		block = m_Impl->AppendBlock();
	auto freeSize = kBLOCK_DATA_SIZE - block->used;
	if (size > freeSize)
	{
		memmove(block->data + index + freeSize, block->data + index, block->used - index);
		memset(block->data + index, fill, freeSize);
		size -= freeSize;
		block->used += freeSize;
		while (size > 0)
		{
			auto len = size < kBLOCK_DATA_SIZE ? size : kBLOCK_DATA_SIZE;
			if (block == nullptr || block->next == nullptr)
			{
				block = m_Impl->AfterBlock(block);
			}
			else
			{
				freeSize = kBLOCK_DATA_SIZE - block->next->used;
				if (freeSize >= len)
				{
					block = block->next;
					memmove(block->data + len, block->data, block->used);
				}
				else
				{
					block = m_Impl->AfterBlock(block);
				}
			}
			memset(block->data, 0, len);
			size -= len;
			block->used += len;
		}
	}
	else
	{
		memmove(block->data + index + size, block->data + index, block->used - index);
		memset(block->data + index, fill, size);
		block->used += size;
	}
	m_Impl->m_TotalUsed += addsize;
}

void DataBuffer::Replace(const void* dest, size_t destSize, size_t srcIndex, size_t srcSize)
{
	if (destSize == srcSize && srcSize == 1)
	{
		SetAt(srcIndex, *reinterpret_cast<const uint8_t*>(dest));
		return;
	}

	if (srcIndex >= Size())
	{
		Append(dest, destSize);
		return;
	}

	if (srcIndex + srcSize > Size())
	{
		srcSize = Size() - srcIndex;
	}

	auto index = srcIndex;
	auto block = m_Impl->FindBlockByOffset(srcIndex);
	if (block == nullptr)
		return;
	auto p = reinterpret_cast<const uint8_t*>(dest);
	if (destSize > srcSize)
	{
		auto padsize = destSize - srcSize;
		InsertFill(index + srcSize, padsize, 0);
	}
	else if (destSize < srcSize)
	{
		auto delsize = srcSize - destSize;
		Remove(index + destSize, delsize);
	}

	if (srcIndex + destSize <= block->used)
	{
		fast_memcpy(block->data + srcIndex, p, destSize);
		destSize = 0;
	}
	else
	{
		auto len = block->used - srcIndex;
		fast_memcpy(block->data + srcIndex, p, len);
		p += len;
		destSize -= len;
	}

	while (destSize > 0)
	{
		block = block->next;
		if (block == nullptr)
			block = m_Impl->AppendBlock();
		auto len = destSize > block->used ? block->used : destSize;
		fast_memcpy(block->data, p, len);
		p += len;
		destSize -= len;
	}
}

void DataBuffer::Fill(uint8_t fill, size_t index, size_t size)
{
	if (size == 1)
	{
		SetAt(index, fill);
		return;
	}

	if (index >= Size())
	{
		return;
	}

	if (index + size > Size())
	{
		size = Size() - index;
	}

	auto block = m_Impl->FindBlockByOffset(index);
	if (block == nullptr)
		return;
	
	if (index + size <= block->used)
	{
		memset(block->data + index, fill, size);
		size = 0;
	}
	else
	{
		auto len = block->used - index;
		memset(block->data + index, fill, len);
		size -= len;
	}

	while (size > 0)
	{
		block = block->next;
		if (block == nullptr)
			break;
		auto len = size > block->used ? block->used : size;
		memset(block->data, fill, len);
		size -= len;
	}
}

void DataBuffer::Remove(size_t index, size_t size)
{
	auto block = m_Impl->FindBlockByOffset(index);
	if (block == nullptr)
		return;
	auto delsize = size;
	auto endIndex = index + size;
	if (endIndex > block->used)
	{
		delsize = block->used - index;
		size -= delsize;
		block->used -= delsize;
		if (block->used == 0)
			block = m_Impl->RemoveBlock(block);
		else
			block = block->next;
		while (size > 0 && block!=nullptr)
		{
			if (size >= block->used)
			{
				size -= block->used;
				block = m_Impl->RemoveBlock(block);
			}
			else
			{
				delsize = size;
				auto len = block->used - delsize;
				fast_memcpy(block->data, block->data + delsize, len);
				size = 0;
				block->used -= size;
				block = block->next;
			}
		}
	}
	else
	{
		auto len = block->used - endIndex;
		if (len > 0)
			memmove(block->data + index, block->data + endIndex, len);
		block->used -= size;
		size = 0;
		if (block->used == 0)
			m_Impl->RemoveBlock(block);
	}
	m_Impl->m_TotalUsed -= delsize - size;
}

void DataBuffer::Compress(void)
{
	auto block = m_Impl->m_BlockFirst;
	while (block != m_Impl->m_BlockLast)
	{
		auto freeSize = kBLOCK_DATA_SIZE - block->used;
		if (freeSize > 0)
		{
			auto next = block->next;
			while (freeSize > 0)
			{
				if (next != nullptr)
				{
					if (freeSize < next->used)
					{
						fast_memcpy(block->data + block->used, next->data, freeSize);
						memmove(next->data, next->data + freeSize, next->used - freeSize);
						block->used += freeSize;
						next->used -= freeSize;
						freeSize = 0;
						break;
					}
					else
					{
						fast_memcpy(block->data + block->used, next->data, next->used);
						block->used += next->used;
						freeSize -= next->used;
						next->used = 0;
						next = m_Impl->RemoveBlock(next);
					}
				}
				else
				{
					return;
				}
			}
		}
		block = block->next;
	}
}

void DataBuffer::Resize(size_t size)
{
	if (size > Size())
	{
		auto padsize = size - Size();
		AppendFill(0, padsize);
	}
	else if(size < Size())
	{
		auto delsize = Size() - size;
		Remove(Size() - delsize, delsize);
	}
}

void DataBuffer::Clear(void)
{
	m_Impl->Clear(false);
}


DataBufferView::DataBufferView(const DataBufferView& view) :
	m_DataBuffer(view.m_DataBuffer),
	m_Position(view.m_Position),
	m_pBuffer(view.m_pBuffer)
{
}
DataBufferView::DataBufferView(const DataBuffer& buffer) :
	m_DataBuffer(buffer),
	m_Position(0),
	m_pBuffer(buffer.m_Impl->m_BlockFirst)
{
}

void DataBufferView::Position(size_t pos) 
{
	if (pos != m_Position)
	{
		auto block = m_DataBuffer.m_Impl->FindBlockByOffset(pos);
		if (block != nullptr)
		{
			m_Position = pos;
			m_pBuffer = block;
		}
	}
}
size_t DataBufferView::GetData(void* buffer, size_t size)
{
	DataBufferPrivateImpl::BufferBlock* block = reinterpret_cast<DataBufferPrivateImpl::BufferBlock*>(m_pBuffer);
	auto len = m_DataBuffer.m_Impl->CopyData(
		block,
		m_Position % kBLOCK_DATA_SIZE,
		size,
		buffer,
		&block);
	m_Position += len;
	if (block != nullptr)
		m_pBuffer = block;
	else
		m_pBuffer = m_DataBuffer.m_Impl->m_BlockLast;
	return len;
}

int DataBufferView::GetByte(void)
{
	uint8_t val;
	DataBufferPrivateImpl::BufferBlock* block = reinterpret_cast<DataBufferPrivateImpl::BufferBlock*>(m_pBuffer);
	auto len = m_DataBuffer.m_Impl->CopyData(
		block,
		m_Position % kBLOCK_DATA_SIZE,
		1,
		&val,
		&block);
	m_Position += len;
	if (block != nullptr)
		m_pBuffer = block;
	else
		m_pBuffer = m_DataBuffer.m_Impl->m_BlockLast;
	if (len == sizeof(val))
		return val;
	return -1;
}
bool DataBufferView::EndOfBuffer(void) const
{
	return m_Position >= m_DataBuffer.Size();
}

DataBufferIterator::DataBufferIterator(const DataBufferIterator& it):
	m_pPrivateImpl(it.m_pPrivateImpl)
{
}

DataBufferIterator::DataBufferIterator(DataBufferIterator&& it) :
	m_pPrivateImpl(it.m_pPrivateImpl)
{
	it.m_pPrivateImpl = nullptr;
}

DataBufferIterator::DataBufferIterator(const DataBuffer& buffer) :
	m_pPrivateImpl(buffer.m_Impl->m_BlockFirst)
{
}

bool DataBufferIterator::Next(void)
{
	if (m_pPrivateImpl == nullptr)
		return false;
	DataBufferPrivateImpl::BufferBlock* block = reinterpret_cast<DataBufferPrivateImpl::BufferBlock*>(m_pPrivateImpl);
	m_pPrivateImpl = block->next;
	return m_pPrivateImpl!=nullptr;
}

bool DataBufferIterator::Prev(void)
{
	if (m_pPrivateImpl == nullptr)
		return false;
	DataBufferPrivateImpl::BufferBlock* block = reinterpret_cast<DataBufferPrivateImpl::BufferBlock*>(m_pPrivateImpl);
	m_pPrivateImpl = block->prev;
	return m_pPrivateImpl != nullptr;
}

const uint8_t* DataBufferIterator::Data(void) const
{
	if (m_pPrivateImpl == nullptr)
		return nullptr;
	DataBufferPrivateImpl::BufferBlock* block = reinterpret_cast<DataBufferPrivateImpl::BufferBlock*>(m_pPrivateImpl);
	return block->data;
}
const size_t DataBufferIterator::Length(void) const
{
	if (m_pPrivateImpl == nullptr)
		return 0;
	DataBufferPrivateImpl::BufferBlock* block = reinterpret_cast<DataBufferPrivateImpl::BufferBlock*>(m_pPrivateImpl);
	return block->used;
}