#pragma once
#include "IAsyncStream.h"

struct http_header_key_less
{
	struct nocase_compare
	{
		bool operator() (const unsigned char& c1, const unsigned char& c2) const {
			return tolower(c1) < tolower(c2);
		}
	};

	bool operator() (const std::string& s1, const std::string& s2) const {
		return std::lexicographical_compare
		(s1.begin(), s1.end(),
			s2.begin(), s2.end(),
			nocase_compare());
	}
};
using http_header_collections = std::map<std::string, std::string, http_header_key_less>;

class WebSocketClient;

class WebSocketChannel :
	public IAsyncChannel,
	public std::enable_shared_from_this<WebSocketChannel>
{
public:
	WebSocketChannel(void);
	virtual ~WebSocketChannel(void);
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
public:
	bool CloseSocket(void);
	void CloseSocket(const boost::system::error_code& ecClose);
	boost::asio::ip::tcp::socket& GetSocket(void) { return m_Socket; }
protected:
	virtual void OnNotifyClose(const boost::system::error_code& ecClose) = 0;
	virtual bool IsBinaryMode(void) = 0;
	virtual bool IsMessageMasked(void) = 0;
protected:
	bool TryOpen(void);
protected:
	std::string m_ConnectQueryString;
	std::string m_HttpVersion;
	http_header_collections m_RequestHeaders;
	http_header_collections m_ResponseHeaders;
private:
	boost::asio::ip::tcp::socket m_Socket;
	std::atomic<bool> m_Opened;
};

class WebSocketClientChannel :
	public WebSocketChannel
{
public:
	WebSocketClientChannel(void);
	virtual ~WebSocketClientChannel(void);
public:
	void SetOwner(std::shared_ptr<WebSocketClient> owner) { m_Device = owner; }
	void Connect(
		const std::wstring& endpoint,
		const http_header_collections& header, 
		std::function<void(const boost::system::error_code ec)> handler);
protected:
	virtual void OnNotifyClose(const boost::system::error_code& ecClose) override;
	virtual bool IsBinaryMode(void) override;
	virtual bool IsMessageMasked(void) override;
private:
	void OnSocketConnected(
		std::shared_ptr<WebSocketChannel> channel, 
		std::string query, 
		std::string host,
		int port,
		std::function<void(const boost::system::error_code ec)> handler);
private:
	std::weak_ptr<WebSocketClient> m_Device;
};

class WebSocketClient :
	public CommunicationDevice
{
public:
	WebSocketClient(void);
	virtual ~WebSocketClient(void);
public:
	// 通过 IDevice 继承
	virtual bool IsServer(void) override;
	virtual PDTable EnumProperties() override;
	bool BinaryMode(void)const { return m_bBinaryMode; }
	bool MessageMasked(void)const { return m_bMessageMasked; }
public:
	virtual void NotifyChannelClose(std::shared_ptr<WebSocketChannel> channel, const boost::system::error_code& ecClose) = 0;
protected:
	virtual http_header_collections GetHttpHeader();
	virtual std::shared_ptr<WebSocketClient> GetSharedPtr() = 0;
protected:
	bool m_bBinaryMode;
	bool m_bMessageMasked;
	std::wstring m_ServerURL;
	std::vector<std::wstring> m_Headers;
};

class WebSocketSingleClient :
	public WebSocketClient,
	public std::enable_shared_from_this<WebSocketSingleClient>
{
public:
	WebSocketSingleClient(void);
	virtual ~WebSocketSingleClient(void);
public:
	// 通过 IDevice 继承
	virtual bool IsSingleChannel(void) override { return true; }
	virtual void Start(void) override;
	virtual void Stop(void) override;
public:
	virtual void NotifyChannelClose(std::shared_ptr<WebSocketChannel> channel, const boost::system::error_code& ecClose) override;
protected:
	virtual std::shared_ptr<WebSocketClient> GetSharedPtr() { return this->shared_from_this(); }
private:
	std::shared_ptr<WebSocketClientChannel> m_Channel;
};

class WebSocketMultipleClient :
	public WebSocketClient,
	public std::enable_shared_from_this<WebSocketMultipleClient>
{
public:
	WebSocketMultipleClient(void);
	virtual ~WebSocketMultipleClient(void);
public:
	// 通过 IDevice 继承
	virtual bool IsSingleChannel(void) override { return false; }
	virtual PDTable EnumProperties() override;
	virtual void Start(void) override;
	virtual void Stop(void) override;
public:
	virtual void NotifyChannelClose(std::shared_ptr<WebSocketChannel> channel, const boost::system::error_code& ecClose) override;
protected:
	virtual std::shared_ptr<WebSocketClient> GetSharedPtr() { return this->shared_from_this(); }
	void UpdateCanonsCount(size_t newCount);
	static void ConnectChannel(std::shared_ptr<WebSocketMultipleClient> client, std::shared_ptr<WebSocketClientChannel> channel);
private:
	std::vector<std::shared_ptr<WebSocketClientChannel>> m_Channels;
	std::atomic<size_t> m_ConnectConuter;
	std::atomic<size_t> m_FailedConuter;
};


class WebSocketServer :
	public CommunicationDevice,
	public std::enable_shared_from_this<WebSocketServer>
{
	friend class TcpChannel;
public:
	WebSocketServer(void);
	virtual ~WebSocketServer(void);

public:
	// 通过 IDevice 继承
	virtual bool IsSingleChannel(void) override;
	virtual bool IsServer(void) override;
	virtual PDTable EnumProperties() override;
	virtual void Start(void) override;
	virtual void Stop(void) override;
public:
	bool BinaryMode(void)const { return m_bBinaryMode; }
	bool MessageMasked(void)const { return m_bMessageMasked; }
	void NotifyChannelClose(std::shared_ptr<WebSocketChannel> channel, const boost::system::error_code& ecClose);
private:
	void StartAcceptClient(void);
	void Close(const boost::system::error_code& errorCode);
private:
	std::wstring m_ListenAddress;
	std::uint16_t m_ListenPort;
	std::wstring m_URL;
	bool m_bBinaryMode;
	bool m_bMessageMasked;
	boost::asio::ip::tcp::acceptor m_Acceptor;
};

class WebSocketServerChannel :
	public WebSocketChannel
{
public:
	WebSocketServerChannel(void);
	virtual ~WebSocketServerChannel(void);
public:
	void SetOwner(std::shared_ptr<WebSocketServer> owner) { m_Device = owner; }
	void Start(std::string url, std::function<void()> cb);
protected:
	virtual void OnNotifyClose(const boost::system::error_code& ecClose) override;
	virtual bool IsBinaryMode(void) override;
	virtual bool IsMessageMasked(void) override;
private:
	void ResponseWebsocketRequest(std::function<void()> cb);
private:
	std::weak_ptr<WebSocketServer> m_Device;
	std::string m_URLPath;
};