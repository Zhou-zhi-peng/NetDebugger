#pragma once
#include "IAsyncStream.h"

class TcpChannel;
class TCPServer :
	public CommunicationDevice,
	public std::enable_shared_from_this<TCPServer>
{
	friend class TcpChannel;
public:
	TCPServer(void);
	virtual ~TCPServer(void);

public:
	// 通过 IDevice 继承
	virtual bool IsSingleChannel(void) override;
	virtual bool IsServer(void) override;
	virtual PDTable EnumProperties() override;
	virtual void Start(void) override;
	virtual void Stop(void) override;
private:
	void StartAcceptClient(void);
	void Close(const boost::system::error_code& errorCode);
private:
	std::wstring m_ListenAddress;
	std::uint16_t m_ListenPort;
	bool m_ReuseAddress;
	bool m_Keepalive;
	boost::asio::ip::tcp::acceptor m_Acceptor;
};

class TcpChannel :
	public IAsyncChannel,
	public std::enable_shared_from_this<TcpChannel>
{
	friend TCPServer;
public:
	TcpChannel(std::shared_ptr<TCPServer> owner);
	virtual ~TcpChannel(void);
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
	void CloseChannel(bool notify, const boost::system::error_code& ec);
private:
	std::weak_ptr<TCPServer> m_Owner;
	boost::asio::ip::tcp::socket m_Socket;
};
