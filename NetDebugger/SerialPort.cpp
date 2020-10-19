#include "pch.h"
#include "SerialPort.h"
#include "NetDebugger.h"
#include "OEMStringHelper.hpp"

SerialPort::SerialPort() :
	m_PortName(),
	m_BaudRate(19200),
	m_CharacterSize(8),
	m_StopBits(boost::asio::serial_port::stop_bits::type::two),
	m_Parity(boost::asio::serial_port::parity::none),
	m_FlowControl(boost::asio::serial_port::flow_control::none),
	m_DTRState(false),
	m_Writing(),
	m_SerialPort(theApp.GetIOContext())
{
	m_Writing.clear();
}

SerialPort::~SerialPort()
{
	auto ec = boost::system::errc::make_error_code(boost::system::errc::connection_aborted);
	CloseSerialPort(false, ec);
}


bool SerialPort::IsServer(void)
{
	return false;
}

bool SerialPort::IsSingleChannel(void)
{
	return true;
}

using PortsArray = std::vector<std::pair<std::wstring, std::wstring>>;
static PortsArray GetAllPorts(void)
{
	PortsArray ports;
	CRegKey RegKey;
	int nCount = 0;
	if (RegKey.Open(HKEY_LOCAL_MACHINE, L"Hardware\\DeviceMap\\SerialComm", KEY_READ) == ERROR_SUCCESS)
	{
		while (true)
		{
			TCHAR szName[MAX_PATH] = {0};
			TCHAR szPort[MAX_PATH] = {0};
			DWORD nValueSize = MAX_PATH-1;
			DWORD nDataSize = MAX_PATH-1;
			DWORD nType;

			if (::RegEnumValue(HKEY(RegKey), nCount, szName, &nValueSize, NULL, &nType, (LPBYTE)szPort, &nDataSize) == ERROR_NO_MORE_ITEMS)
			{
				break;
			}
			std::wstring name(szName);
			auto idx = name.find_last_of('\\');
			if (idx != name.npos)
			{
				name = name.substr(idx + 1);
			}
			name += L" (";
			name += szPort;
			name += L")";
			ports.push_back(std::make_pair(std::wstring(szPort), name));
			nCount++;
		}
	}
	if (ports.empty())
	{
		ports.push_back(std::make_pair(std::wstring(), L"DEVICE.SERIALPORT.PROP.PORTNAME.NOTFOUND"));
	}
	return ports;
}

static std::wstring GetPortDeviceName(const std::wstring& port)
{
	CRegKey RegKey;
	int nCount = 0;
	if (RegKey.Open(HKEY_LOCAL_MACHINE, L"Hardware\\DeviceMap\\SerialComm") == ERROR_SUCCESS)
	{
		while (true)
		{
			TCHAR szName[MAX_PATH] = { 0 };
			TCHAR szPort[MAX_PATH] = { 0 };
			DWORD nValueSize = MAX_PATH - 1;
			DWORD nDataSize = MAX_PATH - 1;
			DWORD nType;

			if (::RegEnumValue(HKEY(RegKey), nCount, szName, &nValueSize, NULL, &nType, (LPBYTE)szPort, &nDataSize) == ERROR_NO_MORE_ITEMS)
			{
				break;
			}
			if(port == std::wstring(szPort))
				return std::wstring(szName);
			nCount++;
		}
	}
	return port;
}

IDevice::PDTable SerialPort::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	using DynamicProperty = PropertyDescriptionHelper::DynamicPropertyDescription;
	auto sp = shared_from_this();
	PDTable results;
	auto dpd = std::make_shared<DynamicProperty>(
		L"PortName",
		L"DEVICE.SERIALPORT.PROP.PORTNAME",
		L"127.0.0.1",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
	);
	dpd->SetEnumHandler(GetAllPorts, false);
	dpd->BindMethod(
		[sp]() { return sp->m_PortName; },
		[sp](const std::wstring& value) { sp->m_PortName = value; }
	);
	results.push_back(dpd);

	auto pd = std::make_shared<StaticProperty>(
		L"BaudRate",
		L"DEVICE.SERIALPORT.PROP.BAUDRATE",
		uint32_t(19200)
		);
	pd->AddEnumOptions(L"50", false);
	pd->AddEnumOptions(L"75", false);
	pd->AddEnumOptions(L"100", false);
	pd->AddEnumOptions(L"150", false);
	pd->AddEnumOptions(L"300", false);
	pd->AddEnumOptions(L"600", false);
	pd->AddEnumOptions(L"1200", false);
	pd->AddEnumOptions(L"1800", false);
	pd->AddEnumOptions(L"2400", false);
	pd->AddEnumOptions(L"3600", false);
	pd->AddEnumOptions(L"4800", false);
	pd->AddEnumOptions(L"7200", false);
	pd->AddEnumOptions(L"9600", false);
	pd->AddEnumOptions(L"14400", false);
	pd->AddEnumOptions(L"19200", false);
	pd->AddEnumOptions(L"28800", false);
	pd->AddEnumOptions(L"38400", false);
	pd->AddEnumOptions(L"43000", false);
	pd->AddEnumOptions(L"56000", false);
	pd->AddEnumOptions(L"57600", false);
	pd->AddEnumOptions(L"115200", false);
	pd->BindMethod(
		[sp]() { return std::to_wstring(sp->m_BaudRate.value()); },
		[sp](const std::wstring& value) 
	{
		sp->m_BaudRate = boost::asio::serial_port::baud_rate(static_cast<int>(std::wcstoul(value.c_str(), nullptr, 10)));
		if (sp->m_SerialPort.is_open())
			sp->m_SerialPort.set_option(sp->m_BaudRate);
	});
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"CharacterSize",
		L"DEVICE.SERIALPORT.PROP.CHARSIZE",
		uint8_t(8)
		);
	pd->BindMethod(
		[sp]() { return std::to_wstring(sp->m_CharacterSize.value()); },
		[sp](const std::wstring& value)
	{
		sp->m_CharacterSize = boost::asio::serial_port::character_size(static_cast<int>(std::wcstoul(value.c_str(), nullptr, 10)));
		if (sp->m_SerialPort.is_open())
			sp->m_SerialPort.set_option(sp->m_CharacterSize);
	});
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"StopBits",
		L"DEVICE.SERIALPORT.PROP.STOPBITS",
		uint8_t(boost::asio::serial_port::stop_bits::type::two)
		);
	pd->AddEnumOptions(L"0", L"DEVICE.SERIALPORT.PROP.STOPBITS.EM0", true);
	pd->AddEnumOptions(L"1", L"DEVICE.SERIALPORT.PROP.STOPBITS.EM1", true);
	pd->AddEnumOptions(L"2", L"DEVICE.SERIALPORT.PROP.STOPBITS.EM2", true);
	pd->BindMethod(
		[sp]() { return std::to_wstring(sp->m_StopBits.value()); },
		[sp](const std::wstring& value)
	{
		sp->m_StopBits = boost::asio::serial_port::stop_bits(static_cast<boost::asio::serial_port::stop_bits::type>(std::wcstoul(value.c_str(), nullptr, 10)));
		if (sp->m_SerialPort.is_open())
			sp->m_SerialPort.set_option(sp->m_StopBits);
	});
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Parity",
		L"DEVICE.SERIALPORT.PROP.PARITY",
		uint8_t(boost::asio::serial_port::parity::none)
		);
	pd->AddEnumOptions(L"0", L"DEVICE.SERIALPORT.PROP.PARITY.EM0", true);
	pd->AddEnumOptions(L"1", L"DEVICE.SERIALPORT.PROP.PARITY.EM1", true);
	pd->AddEnumOptions(L"2", L"DEVICE.SERIALPORT.PROP.PARITY.EM2", true);
	pd->BindMethod(
		[sp]() { return std::to_wstring(sp->m_Parity.value()); },
		[sp](const std::wstring& value)
	{
		sp->m_Parity = boost::asio::serial_port::parity(static_cast<boost::asio::serial_port::parity::type>(std::wcstoul(value.c_str(), nullptr, 10)));
		if (sp->m_SerialPort.is_open())
			sp->m_SerialPort.set_option(sp->m_Parity);
	});
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"FlowControl",
		L"DEVICE.SERIALPORT.PROP.FC",
		uint8_t(boost::asio::serial_port::flow_control::none)
		);

	pd->AddEnumOptions(L"0", L"DEVICE.SERIALPORT.PROP.FC.EM0", true);
	pd->AddEnumOptions(L"1", L"DEVICE.SERIALPORT.PROP.FC.EM1", true);
	pd->AddEnumOptions(L"2", L"DEVICE.SERIALPORT.PROP.FC.EM2", true);
	pd->BindMethod(
		[sp]() { return std::to_wstring(sp->m_FlowControl.value()); },
		[sp](const std::wstring& value)
	{
		sp->m_FlowControl = boost::asio::serial_port::flow_control(static_cast<boost::asio::serial_port::flow_control::type>(std::wcstoul(value.c_str(), nullptr, 10)));
		if (sp->m_SerialPort.is_open())
			sp->m_SerialPort.set_option(sp->m_FlowControl);
	});
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"DTR",
		L"DEVICE.SERIALPORT.PROP.DTR",
		bool(false)
		);
	pd->AddEnumOptions(L"0", L"DEVICE.SERIALPORT.PROP.DTR.EM0", true);
	pd->AddEnumOptions(L"1", L"DEVICE.SERIALPORT.PROP.DTR.EM1", true);
	pd->BindMethod(
		[sp]() { return std::to_wstring(sp->m_DTRState); },
		[sp](const std::wstring& value)
	{
		sp->m_DTRState = std::wcstoul(value.c_str(), nullptr, 10) != 0;
		if (sp->m_SerialPort.is_open())
		{
			if (!::EscapeCommFunction(sp->m_SerialPort.native_handle(), sp->m_DTRState ? SETDTR : CLRDTR))
			{
				DWORD dwErrorCode = ::GetLastError();
				LPSTR lpMsgBuf;
				::FormatMessageA(
					FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					dwErrorCode,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPSTR)&lpMsgBuf,
					0,
					NULL);

				CStringA message(lpMsgBuf);
				::LocalFree(lpMsgBuf);
				throw PropertyException(message);
			}
		}
	});
	results.push_back(pd);
	return results;
}

bool SerialPort::ApplyConfig(boost::system::error_code& ec, std::wstring& name)
{
	m_SerialPort.set_option(m_BaudRate, ec);
	if (ec)
	{
		name = L"BaudRate";
		return false;
	}
	m_SerialPort.set_option(m_CharacterSize, ec);
	if (ec)
	{
		name = L"CharacterSize";
		return false;
	}
	m_SerialPort.set_option(m_StopBits, ec);
	if (ec)
	{
		name = L"StopBits";
		return false;
	}
	m_SerialPort.set_option(m_Parity, ec);
	if (ec)
	{
		name = L"Parity";
		return false;
	}
	m_SerialPort.set_option(m_FlowControl, ec);
	if (ec)
	{
		name = L"FlowControl";
		return false;
	}

	if (!::EscapeCommFunction(m_SerialPort.native_handle(), m_DTRState ? SETDTR : CLRDTR))
	{
		name = L"DTRState";
		ec = boost::system::errc::make_error_code(boost::system::errc::function_not_supported);
		return false;
	}
	return true;
}

void SerialPort::Start(void)
{
	if (Started())
		return;
	StatusChanged(DeviceStatus::Connecting, std::wstring());
	boost::system::error_code ec;
	m_SerialPort.open(WStringToString(m_PortName), ec);
	if (ec)
	{
		StatusChanged(DeviceStatus::Disconnected, StringToWString(ec.message()));
	}
	else
	{
		std::wstring configName;
		if (ApplyConfig(ec, configName))
		{
			StatusChanged(DeviceStatus::Connected, StringToWString(ec.message()));
			ChannelConnected(shared_from_this(), StringToWString(ec.message()));
		}
		else
		{
			boost::system::error_code ecClose;
			m_SerialPort.close(ecClose);
			StatusChanged(DeviceStatus::Disconnected, StringToWString(ec.message()) + L" : " + configName);
		}
	}
}

void SerialPort::Stop(void)
{
	if (Started())
	{
		boost::system::error_code ec;
		m_SerialPort.cancel(ec);
		m_SerialPort.close(ec);
		ChannelDisconnected(shared_from_this(), std::wstring());
		PropertyChanged();
		StatusChanged(DeviceStatus::Disconnected, std::wstring());
	}
}

std::wstring SerialPort::Id(void) const
{
	if (sizeof(size_t) == sizeof(void*))
		return std::to_wstring(reinterpret_cast<size_t>(this));
	else
		return std::to_wstring(reinterpret_cast<uint64_t>(this));
}

std::wstring SerialPort::Description(void) const
{
	return GetPortDeviceName(m_PortName) + L" (" + m_PortName + L")";
}

std::wstring SerialPort::LocalEndPoint(void) const
{
	return m_PortName;
}
std::wstring SerialPort::RemoteEndPoint(void) const
{
	return m_PortName;
}

void SerialPort::Read(void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	boost::asio::async_read(
		m_SerialPort,
		boost::asio::buffer(buffer, bufferSize),
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if (handler != nullptr)
			handler(!ec, bytestransfer);
		if (ec)
		{
			client->CloseSerialPort(ec);
		}
	}
	);
}
void SerialPort::Write(const void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	int count = 1024 * 8;
	while (m_Writing.test_and_set())
	{
		if (--count <= 0)
		{
			if (handler != nullptr)
				handler(true, 0);
			return;
		}
		std::this_thread::yield();
	}
	auto client = shared_from_this();
	boost::asio::async_write(
		m_SerialPort,
		boost::asio::const_buffer(buffer, bufferSize),
		[client, handler, this](const boost::system::error_code& ec, size_t bytestransfer)
	{
		m_Writing.clear();
		if (handler != nullptr)
			handler(!ec, bytestransfer);
		if (ec)
		{
			client->CloseSerialPort(ec);
		}
	});
}
void SerialPort::ReadSome(void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_SerialPort.async_read_some(
		boost::asio::buffer(buffer, bufferSize),
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec, bytestransfer);
		if (ec)
		{
			if (ec != boost::system::errc::operation_canceled)
				client->CloseSerialPort(ec);
		}
	});
}
void SerialPort::WriteSome(const void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto client = shared_from_this();
	m_SerialPort.async_write_some(
		boost::asio::const_buffer(buffer, bufferSize),
		[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
	{
		handler(!ec, bytestransfer);
		if (ec)
		{
			client->CloseSerialPort(ec);
		}
	}
	);
}

void SerialPort::Cancel(void)
{
	boost::system::error_code ec;
	m_SerialPort.cancel(ec);
}

void SerialPort::Close(void)
{
	CloseSerialPort(boost::system::errc::make_error_code(boost::system::errc::connection_aborted));
}


void SerialPort::CloseSerialPort(const boost::system::error_code& ecClose)
{
	CloseSerialPort(true, ecClose);
}

void SerialPort::CloseSerialPort(bool notify, const boost::system::error_code& ecClose)
{
	boost::system::error_code ec;
	if (m_SerialPort.is_open())
	{
		m_SerialPort.cancel(ec);
		m_SerialPort.close(ec);
		if (notify)
		{
			auto message = StringToWString(ecClose.message());
			ChannelDisconnected(shared_from_this(), message);
			PropertyChanged();
			StatusChanged(DeviceStatus::Disconnected, message);
		}
	}
}



class SerialPortSwitch;
class SerialPortStream :
	public IAsyncChannel,
	public std::enable_shared_from_this<SerialPortStream>
{
	friend class SerialPortSwitch;
public:
	SerialPortStream() :
		m_PortName(),
		m_BaudRate(19200),
		m_CharacterSize(8),
		m_StopBits(boost::asio::serial_port::stop_bits::type::two),
		m_Parity(boost::asio::serial_port::parity::none),
		m_FlowControl(boost::asio::serial_port::flow_control::none),
		m_DTRState(false),
		m_Writing(),
		m_Opened(false),
		m_SerialPort(theApp.GetIOContext())
	{
		m_Writing.clear();
	}

	~SerialPortStream()
	{
		auto ec = boost::system::errc::make_error_code(boost::system::errc::connection_aborted);
		CloseSerialPort(false, ec);
	}
public:
	virtual std::wstring Id(void) const override
	{
		if (sizeof(size_t) == sizeof(void*))
			return std::to_wstring(reinterpret_cast<size_t>(this));
		else
			return std::to_wstring(reinterpret_cast<uint64_t>(this));
	}
	virtual std::wstring Description(void) const override
	{
		return GetPortDeviceName(m_PortName) + L" (" + m_PortName + L")";
	}

	virtual std::wstring LocalEndPoint(void) const override
	{
		return m_PortName;
	}
	virtual std::wstring RemoteEndPoint(void) const override
	{
		return m_PortName;
	}

	virtual void Read(void * buffer, size_t bufferSize, IoCompletionHandler handler) override
	{
		auto client = shared_from_this();
		boost::asio::async_read(
			m_SerialPort,
			boost::asio::buffer(buffer, bufferSize),
			[client, buffer, handler](const boost::system::error_code& ec, size_t bytestransfer)
		{
			if (!ec && bytestransfer>0)
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
					client->CloseSerialPort(ec);
			}
		});
	}
	virtual void Write(const void * buffer, size_t bufferSize, IoCompletionHandler handler) override
	{
		int count = 1024 * 8;
		while (m_Writing.test_and_set())
		{
			if (--count <= 0)
			{
				if (handler != nullptr)
					handler(true, 0);
				return;
			}
			std::this_thread::yield();
		}
		auto client = shared_from_this();
		boost::asio::async_write(
			m_SerialPort,
			boost::asio::const_buffer(buffer, bufferSize),
			[client, handler, this](const boost::system::error_code& ec, size_t bytestransfer)
		{
			m_Writing.clear();
			if (handler != nullptr)
				handler(!ec, bytestransfer);
			if (ec)
			{
				client->CloseSerialPort(ec);
			}
		});
	}
	virtual void ReadSome(void * buffer, size_t bufferSize, IoCompletionHandler handler) override
	{
		auto client = shared_from_this();
		m_SerialPort.async_read_some(
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
					client->CloseSerialPort(ec);
			}
		});
	}
	virtual void WriteSome(const void * buffer, size_t bufferSize, IoCompletionHandler handler) override
	{
		auto client = shared_from_this();
		m_SerialPort.async_write_some(
			boost::asio::const_buffer(buffer, bufferSize),
			[client, handler](const boost::system::error_code& ec, size_t bytestransfer)
		{
			handler(!ec, bytestransfer);
			if (ec)
			{
				client->CloseSerialPort(ec);
			}
		});
	}
	virtual void Cancel(void) override
	{
		boost::system::error_code ec;
		m_SerialPort.cancel(ec);
	}
	virtual void Close(void) override
	{
		CloseSerialPort(boost::system::errc::make_error_code(boost::system::errc::connection_aborted));
	}

public:
	void SetTarget(std::shared_ptr<SerialPortSwitch> device, std::shared_ptr<SerialPortStream> stream)
	{
		m_Device = device;
		m_Target = stream;
	}
	IDevice::PDTable EnumProperties(void)
	{
		using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
		using DynamicProperty = PropertyDescriptionHelper::DynamicPropertyDescription;
		auto sp = shared_from_this();
		IDevice::PDTable results;
		auto dpd = std::make_shared<DynamicProperty>(
			L"PortName",
			L"DEVICE.SERIALPORT.PROP.PORTNAME",
			L"127.0.0.1",
			IDevice::PropertyChangeFlags::CanChangeBeforeStart
			);
		dpd->SetEnumHandler(GetAllPorts, false);
		dpd->BindMethod(
			[sp]() { return sp->m_PortName; },
			[sp](const std::wstring& value) { sp->m_PortName = value; }
		);
		results.push_back(dpd);

		auto pd = std::make_shared<StaticProperty>(
			L"BaudRate",
			L"DEVICE.SERIALPORT.PROP.BAUDRATE",
			uint32_t(19200)
			);
		pd->AddEnumOptions(L"50", false);
		pd->AddEnumOptions(L"75", false);
		pd->AddEnumOptions(L"100", false);
		pd->AddEnumOptions(L"150", false);
		pd->AddEnumOptions(L"300", false);
		pd->AddEnumOptions(L"600", false);
		pd->AddEnumOptions(L"1200", false);
		pd->AddEnumOptions(L"1800", false);
		pd->AddEnumOptions(L"2400", false);
		pd->AddEnumOptions(L"3600", false);
		pd->AddEnumOptions(L"4800", false);
		pd->AddEnumOptions(L"7200", false);
		pd->AddEnumOptions(L"9600", false);
		pd->AddEnumOptions(L"14400", false);
		pd->AddEnumOptions(L"19200", false);
		pd->AddEnumOptions(L"28800", false);
		pd->AddEnumOptions(L"38400", false);
		pd->AddEnumOptions(L"43000", false);
		pd->AddEnumOptions(L"56000", false);
		pd->AddEnumOptions(L"57600", false);
		pd->AddEnumOptions(L"115200", false);
		pd->BindMethod(
			[sp]() { return std::to_wstring(sp->m_BaudRate.value()); },
			[sp](const std::wstring& value)
		{
			sp->m_BaudRate = boost::asio::serial_port::baud_rate(static_cast<int>(std::wcstoul(value.c_str(), nullptr, 10)));
			if (sp->m_SerialPort.is_open())
				sp->m_SerialPort.set_option(sp->m_BaudRate);
		});
		results.push_back(pd);

		pd = std::make_shared<StaticProperty>(
			L"CharacterSize",
			L"DEVICE.SERIALPORT.PROP.CHARSIZE",
			uint8_t(8)
			);
		pd->BindMethod(
			[sp]() { return std::to_wstring(sp->m_CharacterSize.value()); },
			[sp](const std::wstring& value)
		{
			sp->m_CharacterSize = boost::asio::serial_port::character_size(static_cast<int>(std::wcstoul(value.c_str(), nullptr, 10)));
			if (sp->m_SerialPort.is_open())
				sp->m_SerialPort.set_option(sp->m_CharacterSize);
		});
		results.push_back(pd);

		pd = std::make_shared<StaticProperty>(
			L"StopBits",
			L"DEVICE.SERIALPORT.PROP.STOPBITS",
			uint8_t(boost::asio::serial_port::stop_bits::type::two)
			);
		pd->AddEnumOptions(L"0", L"DEVICE.SERIALPORT.PROP.STOPBITS.EM0", true);
		pd->AddEnumOptions(L"1", L"DEVICE.SERIALPORT.PROP.STOPBITS.EM1", true);
		pd->AddEnumOptions(L"2", L"DEVICE.SERIALPORT.PROP.STOPBITS.EM2", true);
		pd->BindMethod(
			[sp]() { return std::to_wstring(sp->m_StopBits.value()); },
			[sp](const std::wstring& value)
		{
			sp->m_StopBits = boost::asio::serial_port::stop_bits(static_cast<boost::asio::serial_port::stop_bits::type>(std::wcstoul(value.c_str(), nullptr, 10)));
			if (sp->m_SerialPort.is_open())
				sp->m_SerialPort.set_option(sp->m_StopBits);
		});
		results.push_back(pd);

		pd = std::make_shared<StaticProperty>(
			L"Parity",
			L"DEVICE.SERIALPORT.PROP.PARITY",
			uint8_t(boost::asio::serial_port::parity::none)
			);
		pd->AddEnumOptions(L"0", L"DEVICE.SERIALPORT.PROP.PARITY.EM0", true);
		pd->AddEnumOptions(L"1", L"DEVICE.SERIALPORT.PROP.PARITY.EM1", true);
		pd->AddEnumOptions(L"2", L"DEVICE.SERIALPORT.PROP.PARITY.EM2", true);
		pd->BindMethod(
			[sp]() { return std::to_wstring(sp->m_Parity.value()); },
			[sp](const std::wstring& value)
		{
			sp->m_Parity = boost::asio::serial_port::parity(static_cast<boost::asio::serial_port::parity::type>(std::wcstoul(value.c_str(), nullptr, 10)));
			if (sp->m_SerialPort.is_open())
				sp->m_SerialPort.set_option(sp->m_Parity);
		});
		results.push_back(pd);

		pd = std::make_shared<StaticProperty>(
			L"FlowControl",
			L"DEVICE.SERIALPORT.PROP.FC",
			uint8_t(boost::asio::serial_port::flow_control::none)
			);

		pd->AddEnumOptions(L"0", L"DEVICE.SERIALPORT.PROP.FC.EM0", true);
		pd->AddEnumOptions(L"1", L"DEVICE.SERIALPORT.PROP.FC.EM1", true);
		pd->AddEnumOptions(L"2", L"DEVICE.SERIALPORT.PROP.FC.EM2", true);
		pd->BindMethod(
			[sp]() { return std::to_wstring(sp->m_FlowControl.value()); },
			[sp](const std::wstring& value)
		{
			sp->m_FlowControl = boost::asio::serial_port::flow_control(static_cast<boost::asio::serial_port::flow_control::type>(std::wcstoul(value.c_str(), nullptr, 10)));
			if (sp->m_SerialPort.is_open())
				sp->m_SerialPort.set_option(sp->m_FlowControl);
		});
		results.push_back(pd);

		pd = std::make_shared<StaticProperty>(
			L"DTR",
			L"DEVICE.SERIALPORT.PROP.DTR",
			bool(false)
			);
		pd->AddEnumOptions(L"0", L"DEVICE.SERIALPORT.PROP.DTR.EM0", true);
		pd->AddEnumOptions(L"1", L"DEVICE.SERIALPORT.PROP.DTR.EM1", true);
		pd->BindMethod(
			[sp]() { return std::to_wstring(sp->m_DTRState); },
			[sp](const std::wstring& value)
		{
			sp->m_DTRState = std::wcstoul(value.c_str(), nullptr, 10) != 0;
			if (sp->m_SerialPort.is_open())
			{
				if (!::EscapeCommFunction(sp->m_SerialPort.native_handle(), sp->m_DTRState ? SETDTR : CLRDTR))
				{
					DWORD dwErrorCode = ::GetLastError();
					LPSTR lpMsgBuf;
					::FormatMessageA(
						FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL,
						dwErrorCode,
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						(LPSTR)&lpMsgBuf,
						0,
						NULL);

					CStringA message(lpMsgBuf);
					::LocalFree(lpMsgBuf);
					throw PropertyException(message);
				}
			}
		});
		results.push_back(pd);
		return results;
	}
	boost::system::error_code Open(void)
	{
		boost::system::error_code ec;
		bool state = false;
		if (m_Opened.compare_exchange_weak(state, true))
		{
			m_SerialPort.open(WStringToString(m_PortName), ec);
			if (ec)
				m_Opened = false;
		}
		return ec;
	}

	boost::system::error_code ApplyConfig(std::wstring& name)
	{
		boost::system::error_code ec;
		m_SerialPort.set_option(m_BaudRate, ec);
		if (ec)
		{
			name = L"BaudRate";
			return ec;
		}
		m_SerialPort.set_option(m_CharacterSize, ec);
		if (ec)
		{
			name = L"CharacterSize";
			return ec;
		}
		m_SerialPort.set_option(m_StopBits, ec);
		if (ec)
		{
			name = L"StopBits";
			return ec;
		}
		m_SerialPort.set_option(m_Parity, ec);
		if (ec)
		{
			name = L"Parity";
			return ec;
		}
		m_SerialPort.set_option(m_FlowControl, ec);
		if (ec)
		{
			name = L"FlowControl";
			return ec;
		}

		if (!::EscapeCommFunction(m_SerialPort.native_handle(), m_DTRState ? SETDTR : CLRDTR))
		{
			name = L"DTRState";
			ec = boost::system::errc::make_error_code(boost::system::errc::function_not_supported);
			return ec;
		}
		return ec;
	}
private:
	void CloseSerialPort(const boost::system::error_code& ecClose)
	{
		CloseSerialPort(true, ecClose);
	}

	void CloseSerialPort(bool notify, const boost::system::error_code& ecClose);
private:
	std::wstring m_PortName;
	boost::asio::serial_port::baud_rate m_BaudRate;
	boost::asio::serial_port::character_size m_CharacterSize;
	boost::asio::serial_port::stop_bits m_StopBits;
	boost::asio::serial_port::parity m_Parity;
	boost::asio::serial_port::flow_control m_FlowControl;
	bool m_DTRState;
	std::atomic_flag m_Writing;
	std::atomic<bool> m_Opened;
	boost::asio::serial_port m_SerialPort;
	std::weak_ptr<SerialPortSwitch> m_Device;
	std::weak_ptr<SerialPortStream> m_Target;
};

class SerialPortSwitch :
	public CommunicationDevice,
	public std::enable_shared_from_this<SerialPortSwitch>
{
	friend class SerialPortStream;
public:
	SerialPortSwitch():
		m_SerialPortA(std::make_shared<SerialPortStream>()),
		m_SerialPortB(std::make_shared<SerialPortStream>())
	{
	}
	virtual ~SerialPortSwitch()
	{
		m_SerialPortA->Close();
		m_SerialPortB->Close();
	}
public:
	// Í¨¹ý IDevice ¼Ì³Ð
	virtual bool IsSingleChannel(void) override
	{
		return false;
	}

	virtual bool IsServer(void) override
	{
		return false;
	}

	virtual PDTable EnumProperties() override
	{
		using PropertyGroup = PropertyDescriptionHelper::PropertyGroupDescription;

		PDTable t;
		auto pdA = std::make_shared<PropertyGroup>(
			L"PortA",
			L"DEVICE.SERIALPORTSWITCH.PROP.PORT1"
			);

		auto pdB = std::make_shared<PropertyGroup>(
			L"PortB",
			L"DEVICE.SERIALPORTSWITCH.PROP.PORT2"
			);

		pdA->AddChildRange(m_SerialPortA->EnumProperties());
		pdB->AddChildRange(m_SerialPortB->EnumProperties());
		t.push_back(pdA);
		t.push_back(pdB);
		return t;
	}

	virtual void Start(void) override
	{
		if (Started())
			return;
		boost::system::error_code ec;
		auto dev = shared_from_this();
		StatusChanged(DeviceStatus::Connecting, std::wstring());
		ec = m_SerialPortA->Open();
		if (ec)
		{
			StatusChanged(DeviceStatus::Disconnected, L"#1:" + StringToWString(ec.message()));
			return;
		}
		std::wstring configName;
		ec = m_SerialPortA->ApplyConfig(configName);
		if (ec)
		{
			StatusChanged(DeviceStatus::Disconnected, L"#1:" + StringToWString(ec.message()) + L" : " + configName);
			return;
		}
		

		ec = m_SerialPortB->Open();
		if (ec)
		{
			m_SerialPortA->Close();
			StatusChanged(DeviceStatus::Disconnected, L"#2:" + StringToWString(ec.message()));
			return;
		}
		configName.clear();
		ec = m_SerialPortB->ApplyConfig(configName);
		if (ec)
		{
			m_SerialPortA->Close();
			m_SerialPortB->Close();
			StatusChanged(DeviceStatus::Disconnected, L"#2:" + StringToWString(ec.message()) + L" : " + configName);
			return;
		}
		m_SerialPortA->SetTarget(dev, m_SerialPortB);
		m_SerialPortB->SetTarget(dev, m_SerialPortA);
		StatusChanged(DeviceStatus::Connected, StringToWString(ec.message()));
		ChannelConnected(m_SerialPortA, StringToWString(ec.message()));
		ChannelConnected(m_SerialPortB, StringToWString(ec.message()));
		PropertyChanged();
	}

	virtual void Stop(void) override
	{
		if (Started())
		{
			boost::system::error_code ec;
			m_SerialPortA->CloseSerialPort(false, ec);
			m_SerialPortB->CloseSerialPort(false, ec);
			ChannelDisconnected(m_SerialPortA, std::wstring());
			ChannelDisconnected(m_SerialPortB, std::wstring());
			PropertyChanged();
			StatusChanged(DeviceStatus::Disconnected, std::wstring());
		}
	}
private:
	void ChannelDisconnectedNotify(std::shared_ptr<SerialPortStream> stream,const std::wstring& message)
	{
		boost::system::error_code ec;
		m_SerialPortA->CloseSerialPort(false, ec);
		m_SerialPortB->CloseSerialPort(false, ec);
		ChannelDisconnected(m_SerialPortA, std::wstring());
		ChannelDisconnected(m_SerialPortB, std::wstring());
		PropertyChanged();
		StatusChanged(DeviceStatus::Disconnected, message);
	}
private:
	std::shared_ptr<SerialPortStream> m_SerialPortA;
	std::shared_ptr<SerialPortStream> m_SerialPortB;
};

void SerialPortStream::CloseSerialPort(bool notify, const boost::system::error_code& ecClose)
{
	bool state = true;
	if (m_Opened.compare_exchange_weak(state, false))
	{
		boost::system::error_code ec;
		if (m_SerialPort.is_open())
		{
			m_SerialPort.cancel(ec);
			m_SerialPort.close(ec);
		}
		if (notify)
		{
			auto dev = m_Device.lock();
			if (dev != nullptr)
			{
				auto message = StringToWString(ecClose.message());
				dev->ChannelDisconnectedNotify(shared_from_this(), message);
			}
		}
	}
}


REGISTER_CLASS_TITLE(SerialPort, L"DEVICE.CLASS.SERIALPORT.TITLE");
REGISTER_CLASS_TITLE(SerialPortSwitch, L"DEVICE.CLASS.SERIALPORTSWITCH.TITLE");
