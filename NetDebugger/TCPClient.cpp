#include "pch.h"
#include "TCPClient.h"
#include "NetDebugger.h"
#include "OEMStringHelper.hpp"

#include<vector>

static std::wstring ProtocolToWstring(const boost::asio::ip::tcp::endpoint::protocol_type& protocol)
{
	std::wstring result = L"TCP/IP";
	if (protocol.family() == AF_INET)
		result += L"V4";
	if (protocol.family() == AF_INET6)
		result += L"V6";
	return result;
}

TCPClientChannel::TCPClientChannel(void):
	m_Socket(theApp.GetIOContext()),
	m_Opened(false)
{

}

TCPClientChannel::~TCPClientChannel(void)
{
	CloseSocket();
}

void TCPClientChannel::Connect(const std::wstring& host, int port, bool keepAlive, std::function<void(const boost::system::error_code ec)> handler)
{
	bool state = false;
	if (m_Opened.compare_exchange_weak(state, true))
	{
		auto channel = shared_from_this();
		boost::system::error_code ec;
		auto rslv = std::make_shared<boost::asio::ip::tcp::resolver>(m_Socket.get_io_context());
		boost::asio::ip::tcp::resolver::query qry(WStringToString(host), std::to_string(port));
		rslv->async_resolve(qry, [rslv, channel, keepAlive, handler](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator iter)
		{
			if (ec)
			{
				handler(ec);
			}
			else
			{
				boost::asio::async_connect(
					channel->m_Socket,
					iter,
					[rslv, channel, keepAlive, handler](const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator iter)
				{
					if (ec)
					{
						channel->CloseSocket();
					}
					else
					{
						boost::system::error_code ecSet;
						channel->m_Socket.set_option(boost::asio::socket_base::keep_alive(keepAlive), ecSet);
					}
					handler(ec);
				});
			}
		});
	}
}

bool TCPClientChannel::CloseSocket(void)
{
	bool state = true;
	if (m_Opened.compare_exchange_weak(state, false))
	{
		boost::system::error_code ec;
		m_Socket.cancel(ec);
		m_Socket.shutdown(m_Socket.shutdown_both, ec);
		m_Socket.close(ec);
		return true;
	}
	return false;
}

void TCPClientChannel::CloseSocket(const boost::system::error_code& ecClose)
{
	if (CloseSocket())
	{
		auto dev = m_Device.lock();
		if (dev != nullptr)
		{
			dev->NotifyChannelClose(shared_from_this(), ecClose);
		}
	}
}

std::wstring TCPClientChannel::Id(void) const
{
	if (sizeof(size_t) == sizeof(void*))
		return std::to_wstring(reinterpret_cast<size_t>(this));
	else
		return std::to_wstring(reinterpret_cast<uint64_t>(this));
}

std::wstring TCPClientChannel::Description(void) const
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

std::wstring TCPClientChannel::LocalEndPoint(void) const
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

std::wstring TCPClientChannel::RemoteEndPoint(void) const
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

void TCPClientChannel::Read(void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	boost::asio::async_read(
		m_Socket,
		boost::asio::buffer(buffer, bufferSize),
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if (handler != nullptr)
			handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->CloseSocket(ec);
		}
	}
	);
}
void TCPClientChannel::Write(const void* buffer, size_t bufferSize, IoCompletionHandler handler)
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
				client->CloseSocket(ec);
		}
	});
}
void TCPClientChannel::ReadSome(void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Socket.async_read_some(
		boost::asio::buffer(buffer, bufferSize),
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->CloseSocket(ec);
		}
	}
	);
}
void TCPClientChannel::WriteSome(const void* buffer, size_t bufferSize, IoCompletionHandler handler)
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
				client->CloseSocket(ec);
		}
	});
}

void TCPClientChannel::Cancel(void)
{
	boost::system::error_code ec;
	m_Socket.cancel(ec);
}

void TCPClientChannel::Close(void)
{
	CloseSocket(boost::system::errc::make_error_code(boost::system::errc::connection_aborted));
}

std::wstring TCPClientChannel::GetProtocol()
{
	boost::system::error_code ec;
	auto local = m_Socket.local_endpoint(ec);
	if (ec)
		return StringToWString(ec.message());
	else
		return ProtocolToWstring(local.protocol());
}



TCPClient::TCPClient(void):
	m_ServerURL(L"127.0.0.1"),
	m_RemotePort(0),
	m_Keepalive(false)
{

}

TCPClient::~TCPClient(void)
{
}

bool TCPClient::IsServer(void)
{
	return false;
}

IDevice::PDTable TCPClient::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	PDTable results;
	auto self = GetSharedPtr();
	auto pd = std::make_shared<StaticProperty>(
		L"Host",
		L"DEVICE.TCPCLIENT.PROP.HOST",
		L"127.0.0.1",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_ServerURL; },
		[self](const std::wstring& value) { self->m_ServerURL = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"RemotePort",
		L"DEVICE.TCPCLIENT.PROP.REMOTEPORT",
		uint16_t(0),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_RemotePort); },
		[self](const std::wstring& value) { self->m_RemotePort = static_cast<std::uint16_t>(std::wcstoul(value.c_str(), nullptr, 10)); }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Keepalive",
		L"DEVICE.TCPCLIENT.PROP.KEEPALIVE",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_Keepalive ? 1 : 0); },
		[self](const std::wstring& value) { self->m_Keepalive = value == L"1"; });
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Protocol",
		L"DEVICE.TCPCLIENT.PROP.PROTOCOL",
		L"",
		IDevice::PropertyChangeFlags::Readonly
		);
	pd->BindMethod(
		[self]() { return self->GetProtocol(); },
		nullptr
	);
	results.push_back(pd);
	return results;
}





TCPSingleClient::TCPSingleClient(void):
	m_Channel(std::make_shared<TCPClientChannel>())
{

}

TCPSingleClient::~TCPSingleClient(void)
{
	m_Channel.reset();
}


void TCPSingleClient::Start(void)
{
	if (Started())
		return;

	StatusChanged(DeviceStatus::Connecting, std::wstring());
	auto client = shared_from_this();
	m_Channel->Connect(m_ServerURL, m_RemotePort, m_Keepalive, [client](const boost::system::error_code& ec)
	{
		if (ec)
		{
			client->StatusChanged(DeviceStatus::Disconnected, StringToWString(ec.message()));
			client->PropertyChanged();
			client->ChannelConnected(client->m_Channel, StringToWString(ec.message()));
		}
		else
		{
			client->StatusChanged(DeviceStatus::Connected, StringToWString(ec.message()));
			client->PropertyChanged();
			client->ChannelConnected(client->m_Channel, StringToWString(ec.message()));
			client->m_Channel->SetOwner(client);
		}
	});
}

void TCPSingleClient::Stop(void)
{
	if (m_Channel->CloseSocket())
	{
		ChannelDisconnected(m_Channel, std::wstring());
		PropertyChanged();
		StatusChanged(DeviceStatus::Disconnected, std::wstring());
	}
}

std::wstring TCPSingleClient::GetProtocol()
{
	return m_Channel->GetProtocol();
}

void TCPSingleClient::NotifyChannelClose(std::shared_ptr<TCPClientChannel> channel, const boost::system::error_code& ecClose)
{
	ChannelDisconnected(m_Channel, std::wstring());
	PropertyChanged();
	StatusChanged(DeviceStatus::Disconnected, std::wstring());
}



TCPMultipleClient::TCPMultipleClient(void):
	m_Channels(),
	m_ConnectConuter(0),
	m_FailedConuter(0)
{

}

TCPMultipleClient::~TCPMultipleClient(void)
{

}

IDevice::PDTable TCPMultipleClient::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	auto properties = TCPClient::EnumProperties();
	auto self = shared_from_this();
	auto pd = std::make_shared<StaticProperty>(
		L"CanonsCount",
		L"DEVICE.TCPMULTIPLECLIENT.PROP.CANONSCOUNT",
		uint16_t(10),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_Channels.size()); },
		[self](const std::wstring& value) { self->UpdateCanonsCount(static_cast<std::uint16_t>(std::wcstoul(value.c_str(), nullptr, 10))); }
	);
	properties.push_back(pd);
	pd = std::make_shared<StaticProperty>(
		L"ConnectedConuter",
		L"DEVICE.TCPMULTIPLECLIENT.PROP.CONNCOUNT",
		uint16_t(0),
		IDevice::PropertyChangeFlags::Readonly
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_ConnectConuter.load() - self->m_FailedConuter.load()); },
		nullptr
	);
	properties.push_back(pd);
	return properties;
}

void TCPMultipleClient::Start(void)
{
	if (Started())
		return;

	StatusChanged(DeviceStatus::Connecting, std::wstring());
	auto client = shared_from_this();
	m_ConnectConuter = 0;
	m_FailedConuter = 0;
	theApp.GetIOContext().post([client]()
	{
		for (size_t i = 0; i < client->m_Channels.size(); ++i)
		{
			auto& channel = client->m_Channels.at(i);
			if (channel == nullptr)
				channel = std::make_shared<TCPClientChannel>();
			ConnectChannel(client, channel);
		}
	});
}

void TCPMultipleClient::ConnectChannel(std::shared_ptr<TCPMultipleClient> client, std::shared_ptr<TCPClientChannel> channel)
{
	channel->Connect(client->m_ServerURL, client->m_RemotePort, client->m_Keepalive, [client, channel](const boost::system::error_code& ec)
	{
		auto count = client->m_Channels.size();
		auto complete = (++client->m_ConnectConuter >= count);
		

		if (ec)
		{
			++client->m_FailedConuter;
		}
		else
		{
			channel->SetOwner(client);
			client->ChannelConnected(channel, StringToWString(ec.message()));
		}
		if (complete)
		{
			if (client->m_FailedConuter == count)
			{
				client->PropertyChanged();
				client->StatusChanged(DeviceStatus::Disconnected, StringToWString(ec.message()));
			}
			else
			{
				client->StatusChanged(DeviceStatus::Connected, std::wstring(L""));
				client->PropertyChanged();
			}
		}
	});
}

void TCPMultipleClient::Stop(void)
{
	if (!Started())
		return;
	StatusChanged(DeviceStatus::Disconnecting, std::wstring());
	auto client = shared_from_this();
	theApp.GetIOContext().post([client]()
	{
		for (size_t i = 0; i < client->m_Channels.size(); ++i)
		{
			auto& channel = client->m_Channels.at(i);
			if (channel != nullptr)
			{
				channel->CloseSocket();
				client->ChannelDisconnected(channel, std::wstring());
				channel = nullptr;
			}
		}
		client->m_ConnectConuter = 0;
		client->m_FailedConuter = 0;
		client->PropertyChanged();
		client->StatusChanged(DeviceStatus::Disconnected, std::wstring());
	});
}

void TCPMultipleClient::NotifyChannelClose(std::shared_ptr<TCPClientChannel> channel, const boost::system::error_code& ecClose)
{
	ChannelDisconnected(channel, StringToWString(ecClose.message()));
}

std::wstring TCPMultipleClient::GetProtocol()
{
	if (m_Channels.empty())
		return L"";
	auto ch = m_Channels[0];
	if (ch != nullptr)
		return ch->GetProtocol();
	return L"";
}

void TCPMultipleClient::UpdateCanonsCount(size_t newCount)
{
	if (newCount >= 0xFFFF)
		newCount = 0xFFFF;
	m_Channels.resize(newCount);
}


REGISTER_CLASS_TITLE(TCPSingleClient, L"DEVICE.CLASS.TCPCLIENT.TITLE");
REGISTER_CLASS_TITLE(TCPMultipleClient, L"DEVICE.CLASS.TCPMULTIPLECLIENT.TITLE");