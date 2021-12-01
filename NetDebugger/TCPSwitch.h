#pragma once
#include "IAsyncStream.h"

class TcpForwardChannel;
class TCPForwardServer :
	public CommunicationDevice,
	public std::enable_shared_from_this<TCPForwardServer>
{
public:
	TCPForwardServer(void);
	virtual ~TCPForwardServer(void);

public:
	// 通过 IDevice 继承
	virtual bool IsSingleChannel(void) override;
	virtual bool IsServer(void) override;
	virtual PDTable EnumProperties() override;
	virtual void Start(void) override;
	virtual void Stop(void) override;
public:
	void NotifyChannelClose(std::shared_ptr<TcpForwardChannel> channel, const boost::system::error_code& ecClose);
private:
	void StartAcceptClient(void);
	static void ConnectServer(std::shared_ptr<TCPForwardServer> self, std::shared_ptr<TcpForwardChannel> channelClient, std::shared_ptr<TcpForwardChannel> channelServer);
	void Close(const boost::system::error_code& errorCode);
private:
	std::wstring m_ListenAddress;
	std::uint16_t m_ListenPort;
	std::wstring m_RemoteHost;
	std::uint16_t m_RemotePort;
	bool m_ReuseAddress;
	bool m_Keepalive;
	boost::asio::ip::tcp::acceptor m_Acceptor;
};

class TcpForwardChannel :
	public IAsyncChannel,
	public std::enable_shared_from_this<TcpForwardChannel>
{
	friend TCPForwardServer;
public:
	TcpForwardChannel(std::shared_ptr<TCPForwardServer> owner);
	virtual ~TcpForwardChannel(void);
public:
	// 通过 IAsyncChannel 继承
	virtual std::wstring Id(void) const override;
	virtual std::wstring Description(void) const override;
	virtual std::wstring LocalEndPoint(void) const override;
	virtual std::wstring RemoteEndPoint(void) const override;
	virtual void Read(OutputBuffer buffer, IoCompletionHandler handler) override;
	virtual void Write(InputBuffer buffer, IoCompletionHandler handler) override;
	virtual void ReadSome(OutputBuffer buffer, IoCompletionHandler handler) override;
	virtual void WriteSome(InputBuffer buffer, IoCompletionHandler handler) override;
	virtual void Cancel(void) override;
	virtual void Close(void) override;
protected:
	bool CloseChannel(void);
	void CloseChannel(const boost::system::error_code& ecClose);
	void TargetCloseChannel(const boost::system::error_code& ecClose);
private:
	std::weak_ptr<TCPForwardServer> m_Owner;
	std::weak_ptr<TcpForwardChannel> m_Target;
	boost::asio::ip::tcp::socket m_Socket;
	std::atomic<bool> m_Opened;
	std::wstring m_ChannelGroupName;
};

