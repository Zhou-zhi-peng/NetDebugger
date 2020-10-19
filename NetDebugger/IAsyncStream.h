#pragma once
#include <functional>
#include <string>
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <exception>

class IAsyncChannel;
class IDevice
{
public:
	enum class PropertyType : int
	{
		Int = 0x0000,
		Real = 0x0001,
		String = 0x0002,
		Boolean = 0x0003,
		Enum = 0x0100,
		Group = 0x0200
	};

	enum class PropertyChangeFlags : int
	{
		Readonly= 0x0000,
		CanChangeAfterStart = 0x0001,
		CanChangeBeforeStart = 0x0002,
		CanChangeAlways = 0x0003,
	};

	class PropertyDescription
	{
	public:
		using EnumOptions = std::vector<std::pair<std::wstring, std::wstring>>;
	public:
		virtual std::wstring Name() const = 0;
		virtual std::wstring Title() const = 0;
		virtual std::wstring DefaultValue() const = 0;
		virtual PropertyChangeFlags ChangeFlags() const = 0;
		virtual PropertyType Type() const = 0;
		virtual std::wstring MaxValue() const = 0;
		virtual std::wstring MinValue() const = 0;
		virtual EnumOptions Options() const = 0;
		virtual std::vector<std::shared_ptr<PropertyDescription>> Childs() const = 0;
		virtual std::wstring GetValue(void) = 0;
		virtual void SetValue(const std::wstring& value) = 0;
	};

	enum class DeviceStatus : int
	{
		Connecting,
		Connected,
		Disconnected,
		Disconnecting
	};

public:
	using PD = std::shared_ptr<PropertyDescription>;
	using PDTable = std::vector<PD>;
	using Channel = std::shared_ptr<IAsyncChannel>;
	using ChannelHandler = std::function<void(Channel channel,const std::wstring& message)>;
	using StatusHandler = std::function<void(DeviceStatus status, const std::wstring& message)>;
	using PropertyHandler = std::function<void(void)>;
public:
	virtual bool IsServer(void) = 0;
	virtual bool IsSingleChannel(void) = 0;
public:
	virtual PDTable EnumProperties() = 0;
public:
	virtual void OnChannelConnected(ChannelHandler handler) = 0;
	virtual void OnChannelDisconnected(ChannelHandler handler) = 0;
	virtual void OnStatusChanged(StatusHandler handler) = 0;
	virtual void OnPropertyChanged(PropertyHandler handler) = 0;
public:
	virtual bool Started(void) = 0;
	virtual void Start(void) = 0;
	virtual void Stop(void) = 0;
};

class IAsyncChannel
{
public:
	using IoCompletionHandler = std::function<void(bool ok, size_t io_bytes)>;
public:
	virtual std::wstring Id(void) const = 0;
	virtual std::wstring Description(void) const = 0;
	virtual std::wstring LocalEndPoint(void) const = 0;
	virtual std::wstring RemoteEndPoint(void) const = 0;
public:
	virtual void Read(void* buffer, size_t bufferSize, IoCompletionHandler handler) = 0;
	virtual void Write(const void* buffer, size_t bufferSize, IoCompletionHandler handler) = 0;
	virtual void ReadSome(void* buffer, size_t bufferSize, IoCompletionHandler handler) = 0;
	virtual void WriteSome(const void* buffer, size_t bufferSize, IoCompletionHandler handler) = 0;
	virtual void Cancel(void) = 0;
	virtual void Close(void) = 0;
};

class CommunicationDevice : public IDevice
{
public:
	CommunicationDevice() :
		m_OnConnected(nullptr),
		m_OnDisconnected(nullptr),
		m_OnStatusChanged(nullptr),
		m_OnPropertyChanged(nullptr),
		m_Status(DeviceStatus::Disconnected)
	{

	}

public:
	virtual void OnChannelConnected(ChannelHandler handler)
	{
		m_OnConnected = handler;
	}
	virtual void OnChannelDisconnected(ChannelHandler handler)
	{
		m_OnDisconnected = handler;
	}
	virtual void OnStatusChanged(StatusHandler handler)
	{
		m_OnStatusChanged = handler;
		if (m_OnStatusChanged != nullptr)
		{
			m_OnStatusChanged(m_Status, std::wstring());
		}
	}
	virtual void OnPropertyChanged(PropertyHandler handler)
	{
		m_OnPropertyChanged = handler;
	}

	virtual bool Started(void)
	{
		return m_Status != DeviceStatus::Disconnected;
	}
protected:
	virtual void ChannelConnected(Channel channel, const std::wstring& message)
	{
		if (m_OnConnected != nullptr)
		{
			m_OnConnected(channel, message);
		}
	}
	virtual void ChannelDisconnected(Channel channel, const std::wstring& message)
	{
		if (m_OnDisconnected != nullptr)
		{
			m_OnDisconnected(channel, message);
		}
	}
	virtual void StatusChanged(DeviceStatus status, const std::wstring& message)
	{
		if (m_Status != status)
		{
			m_Status = status;
			if (m_OnStatusChanged != nullptr)
				m_OnStatusChanged(m_Status, message);
		}
	}
	virtual void PropertyChanged(void)
	{
		if (m_OnPropertyChanged != nullptr)
		{
			m_OnPropertyChanged();
		}
	}

	DeviceStatus Status(void)const { return  m_Status; }
private:
	ChannelHandler m_OnConnected;
	ChannelHandler m_OnDisconnected;
	StatusHandler m_OnStatusChanged;
	PropertyHandler m_OnPropertyChanged;
	std::atomic<DeviceStatus> m_Status;
};

class PropertyException : public std::runtime_error
{
public:
	PropertyException(const std::string& message) :
		std::runtime_error(message)
	{

	}

	PropertyException(const char *message) :
		std::runtime_error(message)
	{

	}
};

namespace PropertyDescriptionHelper
{
	using IDevice = IDevice;
	using PD = IDevice::PD;
	using PropertySetMethod = std::function<void(const std::wstring& value)>;
	using PropertyGetMethod = std::function<std::wstring(void)>;
	class PropertyDescription : public IDevice::PropertyDescription
	{
	public:
		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const bool& defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = defValue?L"1":L"0";
			m_Type = static_cast<IDevice::PropertyType>(static_cast<int>(IDevice::PropertyType::Boolean) | static_cast<int>(IDevice::PropertyType::Enum));
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}

		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const std::wstring& defValue,
			IDevice::PropertyType type,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = defValue;
			m_Type = type;
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}

		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const std::wstring& defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = defValue;
			m_Type = IDevice::PropertyType::String;
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}

		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const wchar_t* defValue,
			IDevice::PropertyType type,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = defValue;
			m_Type = type;
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}

		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const wchar_t* defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = defValue;
			m_Type = IDevice::PropertyType::String;
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}

		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const double& defValue,
			const double& fMax,
			const double& fMnx,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = std::to_wstring(defValue);
			m_Type = IDevice::PropertyType::Real;
			m_MaxValue = std::to_wstring(fMax);
			m_MinValue = std::to_wstring(fMnx);
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}

		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const double& defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = std::to_wstring(defValue);
			m_Type = IDevice::PropertyType::Real;
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}

		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const uint8_t& defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = std::to_wstring(defValue);
			m_Type = IDevice::PropertyType::Int;
			m_MaxValue = L"255";
			m_MinValue = L"0";
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}
		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const int8_t& defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = std::to_wstring(defValue);
			m_Type = IDevice::PropertyType::Int;
			m_MaxValue = L"127";
			m_MinValue = L"-128";
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}

		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const uint16_t& defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = std::to_wstring(defValue);
			m_Type = IDevice::PropertyType::Int;
			m_MaxValue = L"65535";
			m_MinValue = L"0";
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}
		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const int16_t& defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = std::to_wstring(defValue);
			m_Type = IDevice::PropertyType::Int;
			m_MaxValue = L"32767";
			m_MinValue = L"-32768";
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}
		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const uint32_t& defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = std::to_wstring(defValue);
			m_Type = IDevice::PropertyType::Int;
			m_MaxValue = L"4294967295";
			m_MinValue = L"0";
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}
		PropertyDescription(
			const std::wstring& name,
			const std::wstring& desc,
			const int32_t& defValue,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_DefaultValue = std::to_wstring(defValue);
			m_Type = IDevice::PropertyType::Int;
			m_MaxValue = L"2147483647";
			m_MinValue = L"-2147483648";
			m_ChangeFlags = changeFlags;
			m_Getter = nullptr;
			m_Setter = nullptr;
		}
	public:
		// 通过 PropertyDescription 继承
		virtual std::wstring Name() const override
		{
			return m_Name;
		}
		virtual std::wstring Title() const override
		{
			return m_Title;
		}
		virtual std::wstring DefaultValue() const override
		{
			return m_DefaultValue;
		}
		virtual IDevice::PropertyChangeFlags ChangeFlags() const override
		{
			return m_ChangeFlags;
		}

		virtual IDevice::PropertyType Type() const override
		{
			return m_Type;
		}
		virtual std::wstring MaxValue() const override
		{
			return m_MaxValue;
		}
		virtual std::wstring MinValue() const override
		{
			return m_MinValue;
		}

		virtual std::vector<std::shared_ptr<IDevice::PropertyDescription>> Childs(void) const override
		{
			return std::vector<std::shared_ptr<IDevice::PropertyDescription>>();
		}

		virtual std::wstring GetValue(void) override
		{
			if (m_Getter)
				return m_Getter();
			return std::wstring();
		}
		virtual void SetValue(const std::wstring& value) override
		{
			if (m_Setter)
				m_Setter(value);
		}
	public:
		void BindMethod(PropertyGetMethod getter, PropertySetMethod setter)
		{
			m_Getter = getter;
			m_Setter = setter;
		}
	protected:
		std::wstring m_Name;
		std::wstring m_Title;
		std::wstring m_DefaultValue;
		IDevice::PropertyChangeFlags m_ChangeFlags;
		IDevice::PropertyType m_Type;
		std::wstring m_MaxValue;
		std::wstring m_MinValue;
		PropertySetMethod m_Setter;
		PropertyGetMethod m_Getter;
	};

	class StaticPropertyDescription : public PropertyDescription
	{
	public:
		using PropertyDescription::PropertyDescription;
	public:
		void AddEnumOptions(const std::wstring& value, const std::wstring& title, bool setEnumFlag)
		{
			if (setEnumFlag)
				m_Type = static_cast<IDevice::PropertyType>(static_cast<int>(m_Type) | static_cast<int>(IDevice::PropertyType::Enum));
			m_Options.push_back(std::make_pair(value, title));
		}
		void AddEnumOptions(const std::wstring& value, bool setEnumFlag)
		{
			if (setEnumFlag)
				m_Type = static_cast<IDevice::PropertyType>(static_cast<int>(m_Type) | static_cast<int>(IDevice::PropertyType::Enum));
			m_Options.push_back(std::make_pair(value, value));
		}
		void ClearEnumOptions()
		{
			m_Type = static_cast<IDevice::PropertyType>(static_cast<int>(m_Type) & (~static_cast<int>(IDevice::PropertyType::Enum)));
			m_Options.clear();
		}
	public:
		// 通过 PropertyDescription 继承
		virtual EnumOptions Options() const override
		{
			auto type = static_cast<IDevice::PropertyType>(static_cast<int>(IDevice::PropertyType::Boolean) | static_cast<int>(IDevice::PropertyType::Enum));
			if (m_Type == type && m_Options.empty())
			{
				EnumOptions opts;
				opts.push_back(std::make_pair(L"0", L"False"));
				opts.push_back(std::make_pair(L"1", L"True"));
				return opts;
			}
			return m_Options;
		}
		virtual std::vector<std::shared_ptr<IDevice::PropertyDescription>> Childs(void) const override
		{
			return std::vector<std::shared_ptr<IDevice::PropertyDescription>>();
		}
	private:
		EnumOptions m_Options;
	};

	class DynamicPropertyDescription : public PropertyDescription
	{
	public:
		using EnumOptionsFunction = std::function<EnumOptions()>;
	public:
		using PropertyDescription::PropertyDescription;
	public:
		void SetEnumHandler(EnumOptionsFunction handler, bool setEnumFlag)
		{
			m_OptionsEnumHandler = handler;
			if (m_OptionsEnumHandler == nullptr)
				m_Type = static_cast<IDevice::PropertyType>(static_cast<int>(m_Type) & (~static_cast<int>(IDevice::PropertyType::Enum)));
			else if( setEnumFlag )
				m_Type = static_cast<IDevice::PropertyType>(static_cast<int>(m_Type) | static_cast<int>(IDevice::PropertyType::Enum));
		}
	public:
		// 通过 PropertyDescription 继承
		virtual EnumOptions Options() const override
		{
			if (m_OptionsEnumHandler == nullptr)
				return EnumOptions();
			return m_OptionsEnumHandler();
		}

		virtual std::vector<std::shared_ptr<IDevice::PropertyDescription>> Childs(void) const override
		{
			return std::vector<std::shared_ptr<IDevice::PropertyDescription>>();
		}
	private:
		EnumOptionsFunction m_OptionsEnumHandler;
	};

	class PropertyGroupDescription : public IDevice::PropertyDescription
	{
	public:
		PropertyGroupDescription(
			const std::wstring& name,
			const std::wstring& desc,
			IDevice::PropertyChangeFlags changeFlags = IDevice::PropertyChangeFlags::CanChangeAlways
		)
		{
			m_Name = name;
			m_Title = desc;
			m_ChangeFlags = changeFlags;
		}
	public:
		// 通过 PropertyDescription 继承
		virtual std::wstring Name() const override
		{
			return m_Name;
		}
		virtual std::wstring Title() const override
		{
			return m_Title;
		}
		virtual std::wstring DefaultValue() const override
		{
			return std::wstring();
		}
		virtual IDevice::PropertyChangeFlags ChangeFlags() const override
		{
			return m_ChangeFlags;
		}

		virtual IDevice::PropertyType Type() const override
		{
			return IDevice::PropertyType::Group;
		}
		virtual std::wstring MaxValue() const override
		{
			return std::wstring();
		}
		virtual std::wstring MinValue() const override
		{
			return std::wstring();
		}

		virtual EnumOptions Options() const override
		{
			return EnumOptions();
		}

		virtual IDevice::PDTable Childs(void) const override
		{
			return m_Childs;
		}
		virtual std::wstring GetValue(void) override
		{
			return std::wstring();
		}
		virtual void SetValue(const std::wstring& value) override
		{
		}
	public:
		void AddChild(IDevice::PD pd)
		{
			m_Childs.push_back(pd);
		}

		void AddChildRange(IDevice::PDTable pds)
		{
			m_Childs.insert(m_Childs.end(), pds.begin(), pds.end());
		}
		
		void ClearChilds()
		{
			m_Childs.clear();
		}
	protected:
		std::wstring m_Name;
		std::wstring m_Title;
		std::wstring m_DefaultValue;
		IDevice::PropertyChangeFlags m_ChangeFlags;
		IDevice::PDTable m_Childs;
	};
}