#pragma once
#include "IAsyncStream.h"

class TCPClient;
class TCPClientChannel :
	public IAsyncChannel,
	public std::enable_shared_from_this<TCPClientChannel>
{
public:
	TCPClientChannel(void);
	virtual ~TCPClientChannel(void);
public:
	// 通过 IAsyncChannel 继承
	virtual std::wstring Id(void) const override;
	virtual std::wstring Description(void) const override;
	virtual std::wstring LocalEndPoint(void) const override;
	virtual std::wstring RemoteEndPoint(void) const override;
	virtual void Read(void* buffer, size_t bufferSize, IoCompletionHandler handler) override;
	virtual void Write(const void* buffer, size_t bufferSize, IoCompletionHandler handler) override;
	virtual void ReadSome(void* buffer, size_t bufferSize, IoCompletionHandler handler) override;
	virtual void WriteSome(const void* buffer, size_t bufferSize, IoCompletionHandler handler) override;
	virtual void Cancel(void) override;
	virtual void Close(void) override;
public:
	void SetOwner(std::shared_ptr<TCPClient> owner) { m_Device = owner; }
	std::wstring GetProtocol();
	bool CloseSocket(void);
	void Connect(const std::wstring& host, int port, bool keepAlive,std::function<void(const boost::system::error_code ec)> handler);
private:
	void CloseSocket(const boost::system::error_code& ecClose);
private:
	boost::asio::ip::tcp::socket m_Socket;
	std::atomic<bool> m_Opened;
	std::weak_ptr<TCPClient> m_Device;
};

class TCPClient :
	public CommunicationDevice
{
public:
	TCPClient(void);
	virtual ~TCPClient(void);
public:
	// 通过 IDevice 继承
	virtual bool IsServer(void) override;
	virtual PDTable EnumProperties() override;
public:
	virtual void NotifyChannelClose(std::shared_ptr<TCPClientChannel> channel, const boost::system::error_code& ecClose) = 0;
protected:
	virtual std::shared_ptr<TCPClient> GetSharedPtr() = 0;
	virtual std::wstring GetProtocol() = 0;
protected:
	std::wstring m_ServerURL;
	std::uint16_t m_RemotePort;
	bool m_Keepalive;
};

class TCPSingleClient :
	public TCPClient,
	public std::enable_shared_from_this<TCPSingleClient>
{
public:
	TCPSingleClient(void);
	virtual ~TCPSingleClient(void) ;
public:
	// 通过 IDevice 继承
	virtual bool IsSingleChannel(void) override { return true; }
	virtual void Start(void) override;
	virtual void Stop(void) override;
public:
	virtual void NotifyChannelClose(std::shared_ptr<TCPClientChannel> channel, const boost::system::error_code& ecClose) override;
	virtual std::wstring GetProtocol() override;
protected:
	virtual std::shared_ptr<TCPClient> GetSharedPtr() { return this->shared_from_this(); }
private:
	std::shared_ptr<TCPClientChannel> m_Channel;
};


class TCPMultipleClient :
	public TCPClient,
	public std::enable_shared_from_this<TCPMultipleClient>
{
public:
	TCPMultipleClient(void);
	virtual ~TCPMultipleClient(void);
public:
	// 通过 IDevice 继承
	virtual bool IsSingleChannel(void) override{ return false; }
	virtual PDTable EnumProperties() override;
	virtual void Start(void) override;
	virtual void Stop(void) override;
public:
	virtual void NotifyChannelClose(std::shared_ptr<TCPClientChannel> channel, const boost::system::error_code& ecClose) override;
	virtual std::wstring GetProtocol() override;
protected:
	virtual std::shared_ptr<TCPClient> GetSharedPtr() { return this->shared_from_this(); }
	void UpdateCanonsCount(size_t newCount);
	static void ConnectChannel(std::shared_ptr<TCPMultipleClient> client, std::shared_ptr<TCPClientChannel> channel);
private:
	std::vector<std::shared_ptr<TCPClientChannel>> m_Channels;
	std::atomic<size_t> m_ConnectConuter;
	std::atomic<size_t> m_FailedConuter;
};
