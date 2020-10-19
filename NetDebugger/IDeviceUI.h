#pragma once
#include <functional>
#include "DataBuffer.h"

class ISendEditor : public CMFCPropertyPage
{
public:
	virtual UINT GetDataType(void) = 0;
	virtual void SetDataType(UINT type) = 0;
	virtual void GetDataBuffer(std::function<bool(const uint8_t* buffer,size_t size)> handler) = 0;
	virtual void ClearContent(void) = 0;
	virtual void SetContent(const void* data, size_t size) = 0;
	virtual void Create(void) = 0;
	virtual void Destroy(void) = 0;
	virtual void UpdateLanguage(void) = 0;
};