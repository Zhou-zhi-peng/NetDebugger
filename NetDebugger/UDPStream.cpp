#include "pch.h"
#include "UDPStream.h"
#include "NetDebugger.h"
#include "OEMStringHelper.hpp"

static std::wstring ProtocolToWstring(const boost::asio::ip::udp::endpoint::protocol_type& protocol)
{
	std::wstring result = L"TCP/IP";
	if (protocol.family() == AF_INET)
		result += L"V4";
	if (protocol.family() == AF_INET6)
		result += L"V6";
	return result;
}

static std::wstring to_wstring(const boost::asio::ip::udp::endpoint& ep)
{
	return std::wstring(L"[") + StringToWString(ep.address().to_string()) + L"]:" + std::to_wstring(ep.port());
}


UDPBasic::UDPBasic():
	m_Socket(theApp.GetIOContext()),
	m_LocalAddress(L"127.0.0.1"),
	m_LocalPort(0),
	m_RemoteAddress(L""),
	m_ReuseAddress(false),
	m_RemotePort(0)
{

}

UDPBasic::~UDPBasic()
{
	auto ec = boost::system::errc::make_error_code(boost::system::errc::connection_aborted);
	CloseSocket(false, ec);
}

bool UDPBasic::IsServer(void)
{
	return false;
}

bool UDPBasic::IsSingleChannel(void)
{
	return false;
}

IDevice::PDTable UDPBasic::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	PDTable results;
	auto self = shared_from_this();
	auto pd = std::make_shared<StaticProperty>(
		L"LocalAddress",
		L"DEVICE.UDPBASIC.PROP.BINDADDRESS",
		L"127.0.0.1",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->AddEnumOptions(L"0.0.0.0", L"0.0.0.0", false);
	pd->AddEnumOptions(L"127.0.0.1", L"127.0.0.1", false);
	pd->AddEnumOptions(L"0:0:0:0:0:0:0:0", L"0:0:0:0:0:0:0:0", false);
	pd->AddEnumOptions(L"0:0:0:0:0:0:0:1", L"0:0:0:0:0:0:0:1", false);
	pd->BindMethod(
		[self]() { return self->m_LocalAddress; },
		[self](const std::wstring& value) { self->m_LocalAddress = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"LocalPort",
		L"DEVICE.UDPBASIC.PROP.BINDPORT",
		uint16_t(0),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_LocalPort); },
		[self](const std::wstring& value) { self->m_LocalPort = static_cast<std::uint16_t>(std::wcstoul(value.c_str(), nullptr, 10)); }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"RemoteAddress",
		L"DEVICE.UDPBASIC.PROP.REMOTEHOST",
		L"",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_RemoteAddress; },
		[self](const std::wstring& value) { self->m_RemoteAddress = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"RemotePort",
		L"DEVICE.UDPBASIC.PROP.REMOTEPORT",
		uint16_t(0),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_RemotePort); },
		[self](const std::wstring& value) { self->m_RemotePort = static_cast<std::uint16_t>(std::wcstoul(value.c_str(), nullptr, 10)); }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"ReuseAddress",
		L"DEVICE.UDPBASIC.PROP.REUSEADDRESS",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_ReuseAddress ? 1 : 0); },
		[self](const std::wstring& value) { self->m_ReuseAddress = value == L"1"; }
	);
	results.push_back(pd);

	/*pd = std::make_shared<StaticProperty>(
		L"Broadcast",
		L"广播模式\r\n设置 UDP 为广播模式",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_Broadcast ? 1 : 0); },
		[self](const std::wstring& value) { self->m_Broadcast = value == L"1";}
	);
	results.push_back(pd);*/

	pd = std::make_shared<StaticProperty>(
		L"Protocol",
		L"DEVICE.UDPBASIC.PROP.PROTOCOL",
		L"",
		IDevice::PropertyChangeFlags::Readonly
		);
	pd->BindMethod(
		[self]()
	{
		boost::system::error_code ec;
		auto local = self->m_Socket.local_endpoint(ec);
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

void UDPBasic::ChannelDisconnected(std::shared_ptr<IAsyncChannel> channel, const std::wstring& message)
{
	{
		std::unique_lock<std::mutex> clk(m_EndpointsMutex);
		auto udpChannel = std::dynamic_pointer_cast<UDPBasicChannel>(channel);
		auto name = to_wstring(udpChannel->RemoteUDPEndPoint());
		m_RemoteEndpoints.erase(name);
	}
	CommunicationDevice::ChannelDisconnected(channel, message);
}

void UDPBasic::Start(void)
{
	if (Started())
		return;

	StatusChanged(DeviceStatus::Connecting, std::wstring());

	auto client = shared_from_this();
	std::thread connectThread([client]()
	{
		boost::system::error_code ec;
		boost::asio::ip::udp::resolver rslv(theApp.GetIOContext());
		boost::asio::ip::udp::resolver::query qry(WStringToString(client->m_RemoteAddress), std::to_string(client->m_RemotePort));
		boost::asio::ip::udp::resolver::iterator iter = rslv.resolve(qry, ec);
		boost::asio::ip::udp::resolver::iterator end;

		if (iter != end)
		{
			auto address = boost::asio::ip::address::from_string(WStringToString(client->m_LocalAddress), ec);
			if (!ec)
			{
				boost::asio::ip::udp::endpoint ep(address, client->m_LocalPort);
				while (iter != end)
				{
					client->m_Socket.open(ep.protocol(), ec);
					if (ec)
						break;

					client->m_Socket.set_option(boost::asio::socket_base::reuse_address(client->m_ReuseAddress), ec);
					if (ec)
						break;

					client->m_Socket.bind(ep, ec);
					if (ec)
						break;

					client->m_Socket.connect(iter->endpoint(), ec);
					if (ec)
						break;

					if (!ec)
						break;
					boost::system::error_code ecClose;
					client->m_Socket.close(ecClose);
					++iter;
				}
			}
			if (ec)
			{
				client->CloseSocket(ec);
			}
			else
			{
				client->StatusChanged(DeviceStatus::Connected, StringToWString(ec.message()));
				client->PropertyChanged();
				client->ChannelConnected(client, StringToWString(ec.message()));
			}
		}
		else
		{
			auto ec = boost::system::errc::make_error_code(boost::system::errc::host_unreachable);
			client->CloseSocket(ec);
		}
	});
	connectThread.detach();
}

void UDPBasic::Stop(void)
{
	if (Started())
	{
		boost::system::error_code ec;
		m_Socket.cancel(ec);
		m_Socket.close(ec);
		CommunicationDevice::ChannelDisconnected(shared_from_this(), std::wstring());
		PropertyChanged();
		StatusChanged(DeviceStatus::Disconnected, std::wstring());
	}
}

void UDPBasic::CloseSocket(const boost::system::error_code& ecClose)
{
	CloseSocket(true, ecClose);
}

void UDPBasic::CloseSocket(bool notify, const boost::system::error_code& ecClose)
{
	boost::system::error_code ec;
	if (m_Socket.is_open())
	{
		m_Socket.cancel(ec);
		m_Socket.close(ec);
		if (notify)
		{
			auto message = StringToWString(ecClose.message());
			CommunicationDevice::ChannelDisconnected(shared_from_this(), message);
			PropertyChanged();
			StatusChanged(DeviceStatus::Disconnected, message);
		}
	}
}


std::wstring UDPBasic::Id(void) const
{
	if (sizeof(size_t) == sizeof(void*))
		return std::to_wstring(reinterpret_cast<size_t>(this));
	else
		return std::to_wstring(reinterpret_cast<uint64_t>(this));
}

std::wstring UDPBasic::Description(void) const
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


std::wstring UDPBasic::LocalEndPoint(void) const
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

std::wstring UDPBasic::RemoteEndPoint(void) const
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

void UDPBasic::Read(OutputBuffer buffer, IoCompletionHandler handler)
{
	ReadSome(buffer, handler);
}
void UDPBasic::Write(InputBuffer buffer, IoCompletionHandler handler)
{
	WriteSome(buffer, handler);
}
void UDPBasic::ReadSome(OutputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	auto remoteEP = std::make_shared<boost::asio::ip::udp::endpoint>();
	m_Socket.async_receive_from(
		boost::asio::buffer(buffer->data(), buffer->size()),
		*remoteEP,
		[client, remoteEP, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec || ec == boost::asio::error::message_size, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled && ec == boost::asio::error::message_size)
				client->CloseSocket(ec);
		}
		else
		{
			std::shared_ptr<UDPBasicChannel> channel;
			{
				std::unique_lock<std::mutex> clk(client->m_EndpointsMutex);
				auto epname = to_wstring(*remoteEP);
				if (client->m_RemoteEndpoints.find(epname) == client->m_RemoteEndpoints.end())
				{
					client->m_RemoteEndpoints.emplace(epname, remoteEP);
					channel = std::make_shared<UDPBasicChannel>(client, *remoteEP);
				}
			}
			if(channel)
				client->ChannelConnected(channel, std::wstring());
		}
	}
	);
}
void UDPBasic::WriteSome(InputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Socket.async_send(
		boost::asio::const_buffer(buffer.buffer, buffer.bufferSize),
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

void UDPBasic::Cancel(void)
{
	boost::system::error_code ec;
	m_Socket.cancel(ec);
}

void UDPBasic::Close(void)
{
	auto ec = boost::system::errc::make_error_code(boost::system::errc::operation_canceled);
	CloseSocket(ec);
}


UDPBasicChannel::UDPBasicChannel(
	std::shared_ptr<UDPBasic> device,
	const boost::asio::ip::udp::endpoint& remote
):
	m_Device(device),
	m_RemoteEP(remote)
{

}

UDPBasicChannel::~UDPBasicChannel(void)
{

}

// 通过 IAsyncChannel 继承
std::wstring UDPBasicChannel::Id(void) const
{
	if (sizeof(size_t) == sizeof(void*))
		return std::to_wstring(reinterpret_cast<size_t>(this));
	else
		return std::to_wstring(reinterpret_cast<uint64_t>(this));
}
std::wstring UDPBasicChannel::Description(void) const
{
	std::wstring result;
	boost::system::error_code ec;
	auto local = m_Device->m_Socket.local_endpoint(ec);
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

	if (ec)
	{
		result = L" <---> none";
	}
	else
	{
		result += L" <---> remote(";
		result += StringToWString(m_RemoteEP.address().to_string()) + L":" + std::to_wstring(m_RemoteEP.port());
		result += L")";
	}
	return result;
}

std::wstring UDPBasicChannel::LocalEndPoint(void) const
{
	std::wstring result;
	boost::system::error_code ec;
	auto local = m_Device->m_Socket.local_endpoint(ec);
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

std::wstring UDPBasicChannel::RemoteEndPoint(void) const
{
	return StringToWString(m_RemoteEP.address().to_string()) + L"#" + std::to_wstring(m_RemoteEP.port());
}

void UDPBasicChannel::Read(OutputBuffer buffer, IoCompletionHandler handler)
{
	ReadSome(buffer, handler);
}

void UDPBasicChannel::Write(InputBuffer buffer, IoCompletionHandler handler)
{
	WriteSome(buffer, handler);
}
void UDPBasicChannel::ReadSome(OutputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Device->m_Socket.async_receive_from(
		boost::asio::buffer(buffer->data(), buffer->size()),
		m_RemoteEP,
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec || ec == boost::asio::error::message_size, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled && ec == boost::asio::error::message_size)
				client->m_Device->ChannelDisconnected(client, StringToWString(ec.message()));
		}
	}
	);
}
void UDPBasicChannel::WriteSome(InputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Device->m_Socket.async_send_to(
		boost::asio::const_buffer(buffer.buffer,buffer.bufferSize),
		m_RemoteEP,
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->m_Device->ChannelDisconnected(client, StringToWString(ec.message()));
		}
	}
	);
}

void UDPBasicChannel::Cancel(void)
{
	m_Device->m_Socket.cancel();
}

void UDPBasicChannel::Close(void)
{
	m_Device->ChannelDisconnected(shared_from_this(), std::wstring());
}





UDPClient::UDPClient() :
	m_Socket(theApp.GetIOContext()),
	m_RemoteAddress(L"127.0.0.1"),
	m_RemotePort(0),
	m_ReuseAddress(false),
	m_Broadcast(false),
	m_RemoteEndpoint()
{

}

UDPClient::~UDPClient()
{
	auto ec = boost::system::errc::make_error_code(boost::system::errc::connection_aborted);
	CloseSocket(false, ec);
}

bool UDPClient::IsServer(void)
{
	return false;
}

bool UDPClient::IsSingleChannel(void)
{
	return true;
}

IDevice::PDTable UDPClient::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	PDTable results;
	auto self = shared_from_this();

	auto pd = std::make_shared<StaticProperty>(
		L"RemoteAddress",
		L"DEVICE.UDPCLIENT.PROP.REMOTEHOST",
		L"",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_RemoteAddress; },
		[self](const std::wstring& value) { self->m_RemoteAddress = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"RemotePort",
		L"DEVICE.UDPCLIENT.PROP.REMOTEPORT",
		uint16_t(0),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_RemotePort); },
		[self](const std::wstring& value) { self->m_RemotePort = static_cast<std::uint16_t>(std::wcstoul(value.c_str(), nullptr, 10)); }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Broadcast",
		L"DEVICE.UDPCLIENT.PROP.BROADCAST",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_Broadcast ? 1 : 0); },
		[self](const std::wstring& value) { self->m_Broadcast = value == L"1"; }
	);
	results.push_back(pd);
	
	pd = std::make_shared<StaticProperty>(
		L"ReuseAddress",
		L"DEVICE.UDPCLIENT.PROP.REUSEADDRESS",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_ReuseAddress ? 1 : 0); },
		[self](const std::wstring& value) { self->m_ReuseAddress = value == L"1";}
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Protocol",
		L"DEVICE.UDPCLIENT.PROP.PROTOCOL",
		L"",
		IDevice::PropertyChangeFlags::Readonly
		);
	pd->BindMethod(
		[self]()
	{
		boost::system::error_code ec;
		auto local = self->m_Socket.local_endpoint(ec);
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

void UDPClient::Start(void)
{
	if (Started())
		return;

	StatusChanged(DeviceStatus::Connecting, std::wstring());

	auto client = shared_from_this();
	std::thread connectThread([client]()
	{
		boost::system::error_code ec;
		boost::asio::ip::udp::resolver rslv(theApp.GetIOContext());
		boost::asio::ip::udp::resolver::query qry(WStringToString(client->m_RemoteAddress), std::to_string(client->m_RemotePort));
		boost::asio::ip::udp::resolver::iterator iter = rslv.resolve(qry, ec);
		boost::asio::ip::udp::resolver::iterator end;

		if (iter != end)
		{
			while (iter != end)
			{
				boost::asio::ip::udp::endpoint ep = iter->endpoint();
				client->m_Socket.open(ep.protocol(), ec);
				if (ec)
				{
					client->m_Socket.close(ec);
					++iter;
					continue;
				}

				client->m_Socket.set_option(boost::asio::socket_base::reuse_address(client->m_ReuseAddress), ec);
				if (ec)
					break;

				client->m_Socket.set_option(boost::asio::socket_base::broadcast(client->m_Broadcast), ec);
				if (ec)
					break;

				if(!client->m_Broadcast)
					client->m_Socket.connect(ep, ec);
				else
				{
					boost::asio::ip::udp::endpoint lep;
					client->m_Socket.bind(lep, ec);
				}
				if (!ec)
					break;
				boost::system::error_code ecClose;
				client->m_Socket.close(ecClose);
				++iter;
			}

			if (ec)
			{
				client->CloseSocket(ec);
			}
			else
			{
				client->m_RemoteEndpoint = iter->endpoint();
				client->StatusChanged(DeviceStatus::Connected, StringToWString(ec.message()));
				client->PropertyChanged();
				client->ChannelConnected(client, StringToWString(ec.message()));
			}
		}
		else
		{
			auto ec = boost::system::errc::make_error_code(boost::system::errc::host_unreachable);
			client->CloseSocket(ec);
		}
	});
	connectThread.detach();
}

void UDPClient::Stop(void)
{
	if (Started())
	{
		boost::system::error_code ec;
		m_Socket.cancel(ec);
		m_Socket.close(ec);
		ChannelDisconnected(shared_from_this(), std::wstring());
		PropertyChanged();
		StatusChanged(DeviceStatus::Disconnected, std::wstring());
	}
}

void UDPClient::CloseSocket(const boost::system::error_code& ecClose)
{
	CloseSocket(true, ecClose);
}

void UDPClient::CloseSocket(bool notify, const boost::system::error_code& ecClose)
{
	boost::system::error_code ec;
	if (m_Socket.is_open())
	{
		m_Socket.cancel(ec);
		m_Socket.close(ec);
		if (notify)
		{
			auto message = StringToWString(ecClose.message());
			CommunicationDevice::ChannelDisconnected(shared_from_this(), message);
			PropertyChanged();
			StatusChanged(DeviceStatus::Disconnected, message);
		}
	}
}


std::wstring UDPClient::Id(void) const
{
	if (sizeof(size_t) == sizeof(void*))
		return std::to_wstring(reinterpret_cast<size_t>(this));
	else
		return std::to_wstring(reinterpret_cast<uint64_t>(this));
}

std::wstring UDPClient::Description(void) const
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


std::wstring UDPClient::LocalEndPoint(void) const
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

std::wstring UDPClient::RemoteEndPoint(void) const
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

void UDPClient::Read(OutputBuffer buffer, IoCompletionHandler handler)
{
	ReadSome(buffer, handler);
}
void UDPClient::Write(InputBuffer buffer, IoCompletionHandler handler)
{
	WriteSome(buffer, handler);
}
void UDPClient::ReadSome(OutputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Socket.async_receive_from(
		boost::asio::buffer(buffer->data(),buffer->size()),
		m_RemoteEndpoint,
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec || ec == boost::asio::error::message_size, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled && ec == boost::asio::error::message_size)
				client->CloseSocket(ec);
		}
	}
	);
}
void UDPClient::WriteSome(InputBuffer buffer, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_Socket.async_send_to(
		boost::asio::const_buffer(buffer.buffer, buffer.bufferSize),
		m_RemoteEndpoint,
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

void UDPClient::Cancel(void)
{
	boost::system::error_code ec;
	m_Socket.cancel(ec);
}

void UDPClient::Close(void)
{
	auto ec = boost::system::errc::make_error_code(boost::system::errc::operation_canceled);
	CloseSocket(ec);
}

REGISTER_CLASS_TITLE(UDPBasic, L"DEVICE.CLASS.UDPBASIC.TITLE");
REGISTER_CLASS_TITLE(UDPClient, L"DEVICE.CLASS.UDPCLIENT.TITLE");