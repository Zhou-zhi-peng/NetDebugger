#pragma once
#include "IAsyncStream.h"
class UDPBasic :
	public CommunicationDevice,
	public IAsyncChannel,
	public std::enable_shared_from_this<UDPBasic>
{
	friend class UDPBasicChannel;
public:
	UDPBasic();
	virtual ~UDPBasic();
public:
	// 通过 CommunicationDevice 继承
	virtual bool IsServer(void) override;
	virtual bool IsSingleChannel(void) override;
	virtual PDTable EnumProperties() override;
	virtual void Start(void) override;
	virtual void Stop(void) override;
public:
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
	virtual void ChannelDisconnected(std::shared_ptr<IAsyncChannel> channel, const std::wstring& message) override;
protected:
	void CloseSocket(const boost::system::error_code& ecClose);
	void CloseSocket(bool notify, const boost::system::error_code& ecClose);
private:
	boost::asio::ip::udp::socket m_Socket;
	std::wstring m_LocalAddress;
	uint16_t m_LocalPort;
	std::wstring m_RemoteAddress;
	uint16_t m_RemotePort;
	bool m_ReuseAddress;
	std::mutex m_EndpointsMutex;
	std::map<std::wstring, std::shared_ptr<boost::asio::ip::udp::endpoint>> m_RemoteEndpoints;
};


class UDPBasicChannel :
	public IAsyncChannel,
	public std::enable_shared_from_this<UDPBasicChannel>
{
public:
	UDPBasicChannel(std::shared_ptr<UDPBasic> device, const boost::asio::ip::udp::endpoint& remote);
	virtual ~UDPBasicChannel(void);
public:
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
public:
	boost::asio::ip::udp::endpoint RemoteUDPEndPoint(void) const { return m_RemoteEP; }
private:
	std::shared_ptr<UDPBasic> m_Device;
	boost::asio::ip::udp::endpoint m_RemoteEP;
};


class UDPClient:
	public CommunicationDevice,
	public IAsyncChannel,
	public std::enable_shared_from_this<UDPClient>
{
public:
	UDPClient();
	virtual ~UDPClient();
public:
	// 通过 CommunicationDevice 继承
	virtual bool IsServer(void) override;
	virtual bool IsSingleChannel(void) override;
	virtual PDTable EnumProperties() override;
	virtual void Start(void) override;
	virtual void Stop(void) override;
public:
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
	void CloseSocket(const boost::system::error_code& ecClose);
	void CloseSocket(bool notify, const boost::system::error_code& ecClose);
private:
	boost::asio::ip::udp::socket m_Socket;
	std::wstring m_RemoteAddress;
	uint16_t m_RemotePort;
	bool m_ReuseAddress;
	bool m_Broadcast;
	boost::asio::ip::udp::endpoint m_RemoteEndpoint;
};