#include "pch.h"
#include "NetDebugger.h"
#include "TCPSwitch.h"
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

TCPForwardServer::TCPForwardServer() :
	m_ListenAddress(L"127.0.0.1"),
	m_ListenPort(0),
	m_RemoteHost(L"127.0.0.1"),
	m_RemotePort(0),
	m_ReuseAddress(false),
	m_Keepalive(false),
	m_Acceptor(theApp.GetIOContext())
{

}

TCPForwardServer::~TCPForwardServer()
{

}


bool TCPForwardServer::IsServer(void)
{
	return true;
}

bool TCPForwardServer::IsSingleChannel(void)
{
	return false;
}

IDevice::PDTable TCPForwardServer::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	using PropertyGroup = PropertyDescriptionHelper::PropertyGroupDescription;
	PDTable results;
	auto self = shared_from_this();
	auto localGroup = std::make_shared<PropertyGroup>(L"Local", L"DEVICE.TCPFORWARDSERVER.PROP.LOCAL");
	auto remoteGroup = std::make_shared<PropertyGroup>(L"Remote", L"DEVICE.TCPFORWARDSERVER.PROP.REMOTE");
	auto pd = std::make_shared<StaticProperty>(
		L"ListenAddress",
		L"DEVICE.TCPFORWARDSERVER.PROP.LISTENADDRESS",
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
	localGroup->AddChild(pd);

	pd = std::make_shared<StaticProperty>(
		L"ListenPort",
		L"DEVICE.TCPFORWARDSERVER.PROP.LISTENPORT",
		uint16_t(0),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_ListenPort); },
		[self](const std::wstring& value) { self->m_ListenPort = static_cast<std::uint16_t>(std::wcstoul(value.c_str(), nullptr, 10)); }
	);
	localGroup->AddChild(pd);



	pd = std::make_shared<StaticProperty>(
		L"RemoteHost",
		L"DEVICE.TCPFORWARDSERVER.PROP.REMOTEHOST",
		L"127.0.0.1",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_RemoteHost; },
		[self](const std::wstring& value) { self->m_RemoteHost = value; }
	);
	remoteGroup->AddChild(pd);

	pd = std::make_shared<StaticProperty>(
		L"RemotePort",
		L"DEVICE.TCPFORWARDSERVER.PROP.REMOTEPORT",
		uint16_t(0),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_RemotePort); },
		[self](const std::wstring& value) { self->m_RemotePort = static_cast<std::uint16_t>(std::wcstoul(value.c_str(), nullptr, 10)); }
	);
	remoteGroup->AddChild(pd);

	results.push_back(localGroup);
	results.push_back(remoteGroup);

	pd = std::make_shared<StaticProperty>(
		L"ReuseAddress",
		L"DEVICE.TCPFORWARDSERVER.PROP.REUSEADDRESS",
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
		L"DEVICE.TCPFORWARDSERVER.PROP.KEEPALIVE",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);

	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_Keepalive ? 1 : 0); },
		[self](const std::wstring& value) { self->m_Keepalive = value == L"1"; });
	results.push_back(pd);
	return results;
}

void TCPForwardServer::Start(void)
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

void TCPForwardServer::Stop(void)
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


void TCPForwardServer::Close(const boost::system::error_code& errorCode)
{
	boost::system::error_code ec;
	m_Acceptor.cancel(ec);
	m_Acceptor.close(ec);
	PropertyChanged();
	StatusChanged(DeviceStatus::Disconnected, StringToWString(errorCode.message()));
}

void TCPForwardServer::StartAcceptClient(void)
{
	auto server = this->shared_from_this();
	std::shared_ptr<TcpForwardChannel> channelClient(new TcpForwardChannel(server), [](TcpForwardChannel* ch)
	{
		ch->CloseChannel();
		delete ch;
	});

	m_Acceptor.async_accept(channelClient->m_Socket, [server, channelClient](const boost::system::error_code& ec)
	{
		if (!ec)
		{
			boost::system::error_code ecSet;
			channelClient->m_Opened = true;
			channelClient->m_Socket.set_option(boost::asio::socket_base::reuse_address(server->m_ReuseAddress), ecSet);
			channelClient->m_Socket.set_option(boost::asio::socket_base::keep_alive(server->m_Keepalive), ecSet);
			channelClient->m_ChannelGroupName = L"#";
			channelClient->m_ChannelGroupName += std::to_wstring((size_t)channelClient->m_Socket.native_handle());
			channelClient->m_ChannelGroupName += L"-Client : ";

			std::shared_ptr<TcpForwardChannel> channelServer(new TcpForwardChannel(server), [](TcpForwardChannel* ch)
			{
				ch->CloseChannel();
				delete ch;
			});
			channelServer->m_ChannelGroupName = L"#";
			channelServer->m_ChannelGroupName += std::to_wstring((size_t)channelClient->m_Socket.native_handle());
			channelServer->m_ChannelGroupName += L"-Server : ";

			ConnectServer(server, channelClient, channelServer);
			server->StartAcceptClient();
		}
	});
}

void TCPForwardServer::ConnectServer(std::shared_ptr<TCPForwardServer> self, std::shared_ptr<TcpForwardChannel> channelClient, std::shared_ptr<TcpForwardChannel> channelServer)
{
	boost::system::error_code ec;
	auto rslv = std::make_shared<boost::asio::ip::tcp::resolver>(self->m_Acceptor.get_io_context());
	boost::asio::ip::tcp::resolver::query qry(WStringToString(self->m_RemoteHost), std::to_string(self->m_RemotePort));
	rslv->async_resolve(qry, [rslv, self, channelClient, channelServer](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator iter)
	{
		if (!ec)
		{
			boost::asio::async_connect(
				channelServer->m_Socket,
				iter,
				[rslv, self, channelClient, channelServer](const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator iter)
			{
				if (!ec)
				{
					boost::system::error_code ecSet;
					channelServer->m_Socket.set_option(boost::asio::socket_base::reuse_address(self->m_ReuseAddress), ecSet);
					channelServer->m_Socket.set_option(boost::asio::socket_base::keep_alive(self->m_Keepalive), ecSet);
					channelServer->m_Opened = true;

					channelClient->m_Target = channelServer;
					channelServer->m_Target = channelClient;
					self->ChannelConnected(channelClient, StringToWString(ec.message()));
					self->ChannelConnected(channelServer, StringToWString(ec.message()));
				}
			});
		}
	});
}

void TCPForwardServer::NotifyChannelClose(std::shared_ptr<TcpForwardChannel> channel, const boost::system::error_code& ecClose)
{
	ChannelDisconnected(channel, StringToWString(ecClose.message()));
}


TcpForwardChannel::TcpForwardChannel(std::shared_ptr<TCPForwardServer> owner):
	m_Owner(owner),
	m_Target(),
	m_Socket(theApp.GetIOContext()),
	m_Opened(false),
	m_ChannelGroupName()
{
}

TcpForwardChannel::~TcpForwardChannel(void)
{
	CloseChannel();
}

bool TcpForwardChannel::CloseChannel(void)
{
	bool state = true;
	if (m_Opened.compare_exchange_weak(state, false))
	{
		m_Target.reset();
		boost::system::error_code ec;
		m_Socket.cancel(ec);
		m_Socket.shutdown(m_Socket.shutdown_both, ec);
		m_Socket.close(ec);
		return true;
	}
	return false;
}

void TcpForwardChannel::TargetCloseChannel(const boost::system::error_code& ecClose)
{
	if (CloseChannel())
	{
		auto dev = m_Owner.lock();
		if (dev != nullptr)
		{
			dev->NotifyChannelClose(shared_from_this(), ecClose);
		}
	}
}

void TcpForwardChannel::CloseChannel(const boost::system::error_code& ecClose)
{
	auto target = m_Target.lock();
	if (CloseChannel())
	{
		if (target != nullptr)
		{
			target->TargetCloseChannel(ecClose);
		}

		auto dev = m_Owner.lock();
		if (dev != nullptr)
		{
			dev->NotifyChannelClose(shared_from_this(), ecClose);
		}
	}
}

std::wstring TcpForwardChannel::Id(void) const
{
	if (sizeof(size_t) == sizeof(void*))
		return std::to_wstring(reinterpret_cast<size_t>(this));
	else
		return std::to_wstring(reinterpret_cast<uint64_t>(this));
}

std::wstring TcpForwardChannel::Description(void) const
{
	std::wstring result = m_ChannelGroupName;
	boost::system::error_code ec;
	auto local = m_Socket.remote_endpoint(ec);
	if (ec)
	{
		result += L"none" + result;
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

std::wstring TcpForwardChannel::LocalEndPoint(void) const
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

std::wstring TcpForwardChannel::RemoteEndPoint(void) const
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

void TcpForwardChannel::Read(void * buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	boost::asio::async_read(
		m_Socket,
		boost::asio::buffer(buffer, bufferSize),
		[client, buffer, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if (!ec && bytestransfer > 0)
		{
			auto tsp = client->m_Target.lock();
			if (tsp != nullptr)
			{
				tsp->Write(buffer, bytestransfer, [client, bytestransfer, handler](bool ok, size_t byteswrite)
				{
					if (handler != nullptr)
						handler(true, bytestransfer);
				});
			}
			else
			{
				if (handler != nullptr)
					handler(true, bytestransfer);
			}
		}
		else
		{
			if (handler != nullptr)
				handler(false, bytestransfer);
			if (ec != boost::system::errc::operation_canceled)
				client->CloseChannel(ec);
		}
	});
}
void TcpForwardChannel::Write(const void * buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	boost::asio::async_write(
		m_Socket,
		boost::asio::const_buffer(buffer, bufferSize),
		[client, handler, this](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if (handler != nullptr)
			handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->CloseChannel(ec);
		}
	});
}

void TcpForwardChannel::ReadSome(void * buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Socket.async_read_some(
		boost::asio::buffer(buffer, bufferSize),
		[client, buffer, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if (!ec && bytestransfer > 0)
		{
			auto tsp = client->m_Target.lock();
			if (tsp != nullptr)
			{
				tsp->Write(buffer, bytestransfer, [client, bytestransfer, handler](bool ok, size_t byteswrite)
				{
					if (handler != nullptr)
						handler(true, bytestransfer);
				});
			}
			else
			{
				if (handler != nullptr)
					handler(true, bytestransfer);
			}
		}
		else
		{
			if (handler != nullptr)
				handler(false, bytestransfer);
			if (ec != boost::system::errc::operation_canceled)
				client->CloseChannel(ec);
		}
	});
}

void TcpForwardChannel::WriteSome(const void * buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Socket.async_write_some(
		boost::asio::const_buffer(buffer, bufferSize),
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->CloseChannel(ec);
		}
	});
}

void TcpForwardChannel::Cancel(void)
{
	boost::system::error_code ec;
	m_Socket.cancel(ec);
}

void TcpForwardChannel::Close(void)
{
	CloseChannel(boost::system::errc::make_error_code(boost::system::errc::connection_aborted));
}

REGISTER_CLASS_TITLE(TCPForwardServer, L"DEVICE.CLASS.TCPFORWARDSERVER.TITLE");