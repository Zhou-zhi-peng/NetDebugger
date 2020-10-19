#pragma once
#include "IAsyncStream.h"

class SerialPort :
	public CommunicationDevice,
	public IAsyncChannel,
	public std::enable_shared_from_this<SerialPort>
{
public:
	SerialPort();
	virtual ~SerialPort();
public:
	// 通过 IDevice 继承
	virtual bool IsSingleChannel(void) override;
	virtual bool IsServer(void) override;
	virtual PDTable EnumProperties() override;
	virtual void Start(void) override;
	virtual void Stop(void) override;

	// 通过 IAsyncChannel 继承
	virtual std::wstring Id(void) const override;
	virtual std::wstring Description(void) const override;
	virtual std::wstring LocalEndPoint(void) const override;
	virtual std::wstring RemoteEndPoint(void) const override;
	virtual void Read(void * buffer, size_t bufferSize, IoCompletionHandler handler) override;
	virtual void Write(const void * buffer, size_t bufferSize, IoCompletionHandler handler) override;
	virtual void ReadSome(void * buffer, size_t bufferSize, IoCompletionHandler handler) override;
	virtual void WriteSome(const void * buffer, size_t bufferSize, IoCompletionHandler handler) override;
	virtual void Cancel(void) override;
	virtual void Close(void) override;
protected:
	void CloseSerialPort(const boost::system::error_code& ecClose);
	void CloseSerialPort(bool notify, const boost::system::error_code& ecClose);
	bool ApplyConfig(boost::system::error_code& ec,std::wstring& name);
private:
	std::wstring m_PortName;
	boost::asio::serial_port::baud_rate m_BaudRate;
	boost::asio::serial_port::character_size m_CharacterSize;
	boost::asio::serial_port::stop_bits m_StopBits;
	boost::asio::serial_port::parity m_Parity;
	boost::asio::serial_port::flow_control m_FlowControl;
	bool m_DTRState;
	mutable std::atomic_flag m_Writing;
	mutable boost::asio::serial_port m_SerialPort;
};
