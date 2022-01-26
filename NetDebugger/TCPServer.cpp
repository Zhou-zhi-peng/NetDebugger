#include "pch.h"
#include "TCPServer.h"
#include "NetDebugger.h"
#include "OEMStringHelper.hpp"

static std::wstring ProtocolToWstring(const boost::asio::ip::tcp::endpoint::protocol_type& protocol)
{
	std::wstring result = L"TCP/IP";
	if (protocol.family() == AF_INET)
		result += L"V4";
	if (protocol.family() == AF_INET6)
		result += L"V6";
	return result;
}

TCPServer::TCPServer():
	m_ListenAddress(L"127.0.0.1"),
	m_ListenPort(0),
	m_ReuseAddress(false),
	m_Keepalive(false),
	m_Acceptor(theApp.GetIOContext())
{

}

TCPServer::~TCPServer()
{
}

bool TCPServer::IsServer(void)
{
	return true;
}

bool TCPServer::IsSingleChannel(void)
{
	return false;
}

IDevice::PDTable TCPServer::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	PDTable results;
	auto self = shared_from_this();
	auto pd = std::make_shared<StaticProperty>(
		L"ListenAddress",
		L"DEVICE.TCPSERVER.PROP.LISTENADDRESS",
		L"127.0.0.1",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->AddEnumOptions(L"0.0.0.0", L"0.0.0.0", false);
	pd->AddEnumOptions(L"127.0.0.1", L"127.0.0.1", false);
	pd->AddEnumOptions(L"0:0:0:0:0:0:0:0", L"0:0:0:0:0:0:0:0", false);
	pd->AddEnumOptions(L"0:0:0:0:0:0:0:1", L"0:0:0:0:0:0:0:1", false);
	pd->BindMethod(
		[self]() { return self->m_ListenAddress; },
		[self](const std::wstring& value) { self->m_ListenAddress = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"ListenPort",
		L"DEVICE.TCPSERVER.PROP.LISTENPORT",
		uint16_t(0),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_ListenPort); },
		[self](const std::wstring& value) { self->m_ListenPort = static_cast<std::uint16_t>(std::wcstoul(value.c_str(), nullptr, 10)); }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"ReuseAddress",
		L"DEVICE.TCPSERVER.PROP.REUSEADDRESS",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_ReuseAddress ? 1 : 0); },
		[self](const std::wstring& value) { self->m_ReuseAddress = value == L"1"; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Keepalive",
		L"DEVICE.TCPSERVER.PROP.KEEPALIVE",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);

	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_Keepalive ? 1 : 0); },
		[self](const std::wstring& value) { self->m_Keepalive = value == L"1"; });
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Protocol",
		L"DEVICE.TCPSERVER.PROP.PROTOCOL",
		L"",
		IDevice::PropertyChangeFlags::Readonly
		);
	pd->BindMethod(
		[self]()
	{
		boost::system::error_code ec;
		auto local = self->m_Acceptor.local_endpoint(ec);
		if (ec)
			return StringToWString(ec.message());
		else
			return ProtocolToWstring(local.protocol());
	},
		nullptr
		);
	results.push_back(pd);

	return results;
}

void TCPServer::Start(void)
{
	if (Started())
		return;

	StatusChanged(DeviceStatus::Connecting, std::wstring());
	boost::system::error_code ec;
	auto address = boost::asio::ip::address::from_string(WStringToString(m_ListenAddress), ec);
	if (ec)
	{
		Close(ec);
		return;
	}

	boost::asio::ip::tcp::endpoint ep(address, m_ListenPort);
	m_Acceptor.open(ep.protocol(), ec);
	if (ec)
	{
		Close(ec);
		return;
	}

	m_Acceptor.bind(ep, ec);
	if (ec)
	{
		Close(ec);
		return;
	}
	m_Acceptor.listen(m_Acceptor.max_listen_connections, ec);
	if (ec)
	{
		Close(ec);
		return;
	}

	m_Acceptor.set_option(boost::asio::socket_base::reuse_address(m_ReuseAddress), ec);
	if (ec)
	{
		Close(ec);
		return;
	}

	StartAcceptClient();
	PropertyChanged();
	StatusChanged(DeviceStatus::Connected, StringToWString(ec.message()));
}

void TCPServer::Close(const boost::system::error_code& errorCode)
{
	boost::system::error_code ec;
	m_Acceptor.cancel(ec);
	m_Acceptor.close(ec);
	PropertyChanged();
	StatusChanged(DeviceStatus::Disconnected, StringToWString(errorCode.message()));
}

void TCPServer::StartAcceptClient(void)
{
	auto server = this->shared_from_this();
	std::shared_ptr<TcpChannel> channel(new TcpChannel(server), [](TcpChannel* ch)
	{
		ch->Close();
		delete ch;
	});

	m_Acceptor.async_accept(channel->m_Socket, [server, channel](const boost::system::error_code& ec)
	{
		if (!ec)
		{
			boost::system::error_code ecSet;
			channel->m_Socket.set_option(boost::asio::socket_base::reuse_address(server->m_ReuseAddress), ecSet);
			channel->m_Socket.set_option(boost::asio::socket_base::keep_alive(server->m_Keepalive), ecSet);
			server->ChannelConnected(channel,StringToWString(ec.message()));
			server->StartAcceptClient();
		}
	});
}

void TCPServer::Stop(void)
{
	if (Started())
	{
		StatusChanged(DeviceStatus::Disconnecting, std::wstring());
		boost::system::error_code ec;
		m_Acceptor.cancel(ec);
		m_Acceptor.close(ec);
		PropertyChanged();
		StatusChanged(DeviceStatus::Disconnected, std::wstring());
	}
}

TcpChannel::TcpChannel(std::shared_ptr<TCPServer> owner) :
	m_Owner(owner),
	m_Socket(theApp.GetIOContext())
{

}

TcpChannel::~TcpChannel(void)
{
	CloseChannel(false, boost::system::errc::make_error_code(boost::system::errc::success));
}

std::wstring TcpChannel::Id(void) const
{
	if (sizeof(size_t) == sizeof(void*))
		return std::to_wstring(reinterpret_cast<size_t>(this));
	else
		return std::to_wstring(reinterpret_cast<uint64_t>(this));
}

std::wstring TcpChannel::Description(void) const
{
	std::wstring result;
	boost::system::error_code ec;
	auto local = m_Socket.local_endpoint(ec);
	if (ec)
	{
		result = L"none" + result;
	}
	else
	{
		result += L"local(";
		result += StringToWString(local.address().to_string()) + L":" + std::to_wstring(local.port());
		result += L")";
	}

	auto remote = m_Socket.remote_endpoint(ec);
	if (ec)
	{
		result += L" <---> none";
	}
	else
	{
		result += L" <---> remote(";
		result += StringToWString(remote.address().to_string()) + L":" + std::to_wstring(remote.port());
		result += L")";
	}
	return result;
}


std::wstring TcpChannel::LocalEndPoint(void) const
{
	std::wstring result;
	boost::system::error_code ec;
	auto local = m_Socket.local_endpoint(ec);
	if (ec)
	{
		result = L"none";
	}
	else
	{
		result = StringToWString(local.address().to_string()) + L"#" + std::to_wstring(local.port());
	}
	return result;
}

std::wstring TcpChannel::RemoteEndPoint(void) const
{
	std::wstring result;
	boost::system::error_code ec;
	auto local = m_Socket.remote_endpoint(ec);
	if (ec)
	{
		result = L"none";
	}
	else
	{
		result = StringToWString(local.address().to_string()) + L"#" + std::to_wstring(local.port());
	}
	return result;
}

void TcpChannel::Read(OutputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	boost::asio::async_read(
		m_Socket,
		boost::asio::buffer(buffer->data(), buffer->size()),
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if (handler != nullptr)
			handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->CloseChannel(true, ec);
		}
	}
	);
}
void TcpChannel::Write(InputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	boost::asio::async_write(
		m_Socket,
		boost::asio::const_buffer(buffer.buffer, buffer.bufferSize),
		[client, handler, this](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if (handler != nullptr)
			handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->CloseChannel(true, ec);
		}
	}
	);
}
void TcpChannel::ReadSome(OutputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Socket.async_read_some(
		boost::asio::buffer(buffer->data(), buffer->size()),
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->CloseChannel(true, ec);
		}
	}
	);
}
void TcpChannel::WriteSome(InputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Socket.async_write_some(
		boost::asio::const_buffer(buffer.buffer, buffer.bufferSize),
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->CloseChannel(true, ec);
		}
	}
	);
}

void TcpChannel::Cancel(void)
{
	boost::system::error_code ec;
	m_Socket.cancel(ec);
}

void TcpChannel::Close(void)
{
	CloseChannel(true, boost::system::errc::make_error_code(boost::system::errc::operation_canceled));
}

void TcpChannel::CloseChannel(bool notify, const boost::system::error_code& error)
{
	boost::system::error_code ec;
	if (m_Socket.is_open())
	{
		m_Socket.cancel(ec);
		m_Socket.shutdown(m_Socket.shutdown_both, ec);
		m_Socket.close(ec);
		if (notify)
		{
			auto owner = m_Owner.lock();
			if(owner!=nullptr)
				owner->ChannelDisconnected(shared_from_this(), StringToWString(error.message()));
		}
	}
}


REGISTER_CLASS_TITLE(TCPServer, L"DEVICE.CLASS.TCPSERVER.TITLE");