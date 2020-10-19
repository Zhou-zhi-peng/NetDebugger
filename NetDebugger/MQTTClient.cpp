#include "pch.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include "MQTTClient.h"

namespace MQTT
{
	namespace URL
	{
		static bool parse(const std::string& url, std::string& host, int& port)
		{
			if (url.length() <= 0)
				return false;

			auto pos = url.find_first_of(':');
			if (pos == url.npos)
				return false;
			if (url.length() <= pos + 3)
				return false;
			auto address = url.substr(pos + 3);
			if (address.front() == '[')
			{
				pos = address.find_last_of(']');
				if (pos == address.npos)
				{
					host = address;
					port = -1;
				}
				else
				{
					host = address;
					host.erase(pos);
					host.erase(0, 1);
					auto sport = address.substr(pos + 1);
					pos = sport.find_first_of(':');
					if (pos != sport.npos)
					{
						sport = sport.erase(0, pos + 1);
						port = static_cast<int>(strtoul(sport.c_str(), nullptr, 10));
					}
				}
			}
			else
			{
				auto pos = address.find_last_of(':');
				if (pos != address.npos)
				{
					auto sport = address.substr(pos + 1);
					host = address.substr(0, pos);
					port = static_cast<int>(strtoul(sport.c_str(), nullptr, 10));
				}
				else
				{
					host = address;
					port = -1;
				}
			}
			return true;
		}
	}

	const size_t kMAX_MESSAGE_PAYLOAD = 1024 * 1024 * 8;
	enum class MQTTMessageType
	{
		CONNECT = 0x01,
		CONNACK,
		PUBLISH,
		PUBACK,
		PUBREC,
		PUBREL,
		PUBCOMP,
		SUBSCRIBE,
		SUBACK,
		UNSUBSCRIBE,
		UNSUBACK,
		PINGREQ,
		PINGRESP,
		DISCONNECT
	};

	typedef union
	{
		uint8_t value;
		struct
		{
			uint8_t retain : 1;
			uint8_t qos : 2;
			uint8_t dup : 1;
			uint8_t type : 4;
		}bits;
	}MessageHeader;

	typedef union
	{
		uint8_t value;
		struct
		{
			uint8_t reserved : 1;
			uint8_t cleanSession : 1;
			uint8_t lastWillFlag : 1;
			uint8_t lastWillQos : 2;
			uint8_t lastWillRetain : 1;
			uint8_t password : 1;
			uint8_t username : 1;
		}bits;
	}ConnectFlags;


	class MQTTPacket
	{
	public:
		MQTTPacket(MQTTMessageType type) :
			mHeader(),
			mBuffer()
		{
			mHeader.value = 0;
			mHeader.bits.type = static_cast<uint8_t>(type) & 0xF;
		}

		~MQTTPacket()
		{
		}
	public:
		MessageHeader& Header()
		{
			return *reinterpret_cast<MessageHeader*>(&mHeader);
		}

		uint32_t Length(void) const
		{
			return static_cast<uint32_t>(mBuffer.size());
		}
	public:
		void WriteMQTTString(void)
		{
			WriteUTF8String("MQTT");
		}
		void WriteMQTTVersion(MQTTVersion value)
		{
			WriteUint8(static_cast<uint8_t>(value));
		}
		void WriteUTF8String(const std::string& value)
		{
			WriteUint16(static_cast<uint16_t>(value.length()));
			WriteRawBytes(value.data(), value.length());
		}
		void WriteUint8(uint8_t value)
		{
			WriteRawBytes(&value, sizeof(value));
		}
		void WriteUint16(uint16_t value)
		{
			auto d = htons(value);
			WriteRawBytes(&d, sizeof(d));
		}

		void WriteConnectPayload(const void* value, size_t size)
		{
			WriteUint16(static_cast<uint16_t>(size));
			WriteRawBytes(value, size);
		}
		void WriteRawBytes(const void* value, size_t size)
		{
			auto offset = mBuffer.size();
			mBuffer.resize(mBuffer.size() + size);
			auto ptr = mBuffer.data() + offset;
			memcpy(ptr, value, size);
		}
	public:
		static std::string ReadUTF8String(const std::vector<uint8_t>& buffer, size_t& offset)
		{
			auto len = ReadUint16(buffer, offset);
			std::string value;
			if (buffer.size() >= offset + len)
			{
				value.assign(reinterpret_cast<const char*>(buffer.data() + offset), len);
				offset += len;
			}
			return value;
		}
		static uint16_t ReadUint16(const std::vector<uint8_t>& buffer, size_t& offset)
		{
			auto ptr = buffer.data() + offset;
			auto value = ntohs(*(reinterpret_cast<const uint16_t*>(ptr)));
			offset += sizeof(uint16_t);
			return value;
		}
	public:
		size_t ToBuffer(std::vector<uint8_t>& buffer)
		{
			uint8_t buf[5];
			buf[0] = mHeader.value;
			uint8_t count = 1;
			buffer.clear();
			auto length = mBuffer.size();
			do
			{
				uint8_t temp = length % 128;
				length /= 128;
				if (length > 0)
				{
					temp |= 0x80;
				}
				buf[count++] = temp;
			} while (length > 0);
			buffer.assign(buf, buf + count);
			buffer.insert(buffer.end(), mBuffer.begin(), mBuffer.end());
			return buffer.size();
		}
	private:
		MessageHeader mHeader;
		std::vector<uint8_t> mBuffer;
	};

	static void DefaultEventCallback(MQTTErrorCode errorCode)
	{
	}

	static void DefaultTransactionCallback(const std::vector<MQTTErrorCode>& errorCode)
	{
	}

	static void DefaultMessageCallback(const std::string& topic, const uint8_t* payload, size_t size)
	{

	}

	MQTTClient::MQTTClient(boost::asio::io_service& ios) :
		mSocket(ios),
		mConnectOptions(),
		mConnected(false),
		mPacketIdentifier(1),
		mTransactionMutex(),
		mTransactions(),
		mPingTimer(nullptr),
		mConnectedCallback(DefaultEventCallback),
		mDisconnectedCallback(DefaultEventCallback),
		mPingCallback(DefaultEventCallback),
		mMessageCallback(DefaultMessageCallback)
	{
	}

	MQTTClient::~MQTTClient()
	{
		Disconnect(true);
	}

	void MQTTClient::OnConnected(MQTTEventCallback handler)
	{
		if (handler == nullptr)
			handler = DefaultEventCallback;
		mConnectedCallback = handler;
	}
	void MQTTClient::OnDisconnected(MQTTEventCallback handler)
	{
		if (handler == nullptr)
			handler = DefaultEventCallback;
		mDisconnectedCallback = handler;
	}
	void MQTTClient::OnMessage(MQTTMessageCallback handler)
	{
		if (handler == nullptr)
			handler = DefaultMessageCallback;
		mMessageCallback = handler;
	}

	void MQTTClient::Connect(const ConnectionOptions& options)
	{
		Disconnect(false);

		MQTTPacket packet(MQTTMessageType::CONNECT);
		uint8_t connectFlags = 0;
		packet.WriteMQTTString();
		packet.WriteMQTTVersion(options.Version);
		if (options.CleanSession)
			connectFlags |= 0x2;
		if (!options.Will.Topic.empty())
		{
			connectFlags |= 0x04;
			connectFlags |= static_cast<uint8_t>(static_cast<int>(options.Will.Qos) << 3);
			if (options.Will.Retain)
				connectFlags |= 0x20;
		}

		if (!options.Password.empty())
			connectFlags |= 0x40;

		if (!options.UserName.empty())
			connectFlags |= 0x80;

		packet.WriteUint8(connectFlags);
		packet.WriteUint16(options.KeepAlive);
		packet.WriteUTF8String(options.ClientID);

		if (!options.Will.Topic.empty())
		{
			packet.WriteUTF8String(options.Will.Topic);
			packet.WriteConnectPayload(options.Will.Data.data(), options.Will.Data.size());
		}

		if (!options.UserName.empty())
			packet.WriteUTF8String(options.UserName);
		if (!options.Password.empty())
			packet.WriteUTF8String(options.Password);

		auto buffer = std::make_shared<std::vector<uint8_t>>();
		buffer->reserve(kMAX_MESSAGE_PAYLOAD + 2);
		packet.ToBuffer(*buffer);

		std::string host;
		int port = 0;
		URL::parse(options.serverURL, host, port);
		this->mConnectOptions = options;
		boost::system::error_code ec;
		boost::asio::ip::tcp::resolver rslv(mSocket.get_io_service());
		boost::asio::ip::tcp::resolver::query qry(host, std::to_string(port));
		boost::asio::ip::tcp::resolver::iterator iter = rslv.resolve(qry, ec);
		boost::asio::ip::tcp::resolver::iterator end;
		if (iter != end)
		{
			auto client = shared_from_this();
			boost::asio::async_connect(
				mSocket,
				iter,
				[client, iter, buffer](boost::system::error_code ec, boost::asio::ip::tcp::resolver::iterator iter)
			{
				if (!ec)
				{
					boost::asio::async_write(
						client->mSocket,
						boost::asio::const_buffer(buffer->data(), buffer->size()),
						[client, buffer](const boost::system::error_code& ec, size_t bytestransfer)
					{
						if (!ec)
						{
							if (client->mConnectOptions.KeepAlive > 0)
							{
								auto sec = client->mConnectOptions.KeepAlive / 2;
								if (sec == 0)
									sec = 1;

								client->mPingTimer = std::make_shared<boost::asio::deadline_timer>(
									client->mSocket.get_io_service(),
									boost::posix_time::seconds(sec)
									);
								client->onStartPingTimer(client);
							}
							client->onReadMQTTPacket(client, buffer);
						}
						else
						{
							client->Disconnect(true);
							client->mConnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
							client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
						}
					}
					);
				}
				else
				{
					client->mConnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
					client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
				}
			}
			);
		}
		else
		{
			mConnectedCallback(MQTTErrorCode::CONNECTION_TIMEOUT);
			mDisconnectedCallback(MQTTErrorCode::CONNECTION_TIMEOUT);
		}
	}

	void MQTTClient::onStartPingTimer(MQTTClientPtr client)
	{
		client->mPingTimer->async_wait(
			[client](const boost::system::error_code& ec)
		{
			if (!ec)
			{
				client->Ping(client);
				auto sec = client->mConnectOptions.KeepAlive / 2;
				if (sec == 0)
					sec = 1;
				client->mPingTimer->expires_from_now(boost::posix_time::seconds(sec));
				client->onStartPingTimer(client);
			}
		}
		);
	}

	void MQTTClient::Disconnect(bool forced)
	{
		if (forced || (!mConnected))
		{
			mConnected = false;
			boost::system::error_code ec;
			std::vector<MQTTTransactionCallback> temp;
			mSocket.close(ec);
			if (mPingTimer != nullptr)
			{
				mPingTimer->cancel(ec);
			}

			{
				std::unique_lock<std::mutex> lock(mTransactionMutex);
				for (auto& i : mTransactions)
				{
					temp.push_back(i.second);
				}
				mTransactions.clear();
			}
			std::vector< MQTTErrorCode> results;
			results.push_back(MQTTErrorCode::CONNECTION_DISCONN);
			for (const auto& handler : temp)
			{
				handler(results);
			}
		}
		else
		{
			MQTTPacket packet(MQTTMessageType::DISCONNECT);
			auto buffer = std::make_shared<std::vector<uint8_t>>();
			packet.ToBuffer(*buffer);
			auto client = shared_from_this();
			boost::asio::async_write(
				client->mSocket,
				boost::asio::const_buffer(buffer->data(), buffer->size()),
				[client, buffer](const boost::system::error_code& ec, size_t bytestransfer)
			{
				client->Disconnect(true);
			}
			);
		}
	}

	void MQTTClient::Ping(MQTTClientPtr client)
	{
		MQTTPacket packet(MQTTMessageType::PINGREQ);
		auto buffer = std::make_shared<std::vector<uint8_t>>();
		packet.ToBuffer(*buffer);
		boost::asio::async_write(
			client->mSocket,
			boost::asio::const_buffer(buffer->data(), buffer->size()),
			[client, buffer](const boost::system::error_code& ec, size_t bytestransfer)
		{
			if (ec)
			{
				client->Disconnect(true);
				client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
			}
		}
		);
	}

	void MQTTClient::Publish(const std::string& topic, const uint8_t* payload, size_t payloadSize, MessageQOS qos, bool retain, MQTTTransactionCallback handler)
	{
		if (handler == nullptr)
			handler = DefaultTransactionCallback;

		MQTTPacket packet(MQTTMessageType::PUBLISH);
		auto& header = packet.Header();
		header.bits.dup = 0;
		header.bits.qos = static_cast<uint8_t>(qos) & 0x3;
		header.bits.retain = retain ? 1 : 0;

		packet.WriteUTF8String(topic);
		if (header.bits.qos > 0)
		{
			auto tid = BeginTransaction(handler);
			packet.WriteUint16(tid);
		}
		packet.WriteRawBytes(payload, payloadSize);
		auto buffer = std::make_shared<std::vector<uint8_t>>();
		packet.ToBuffer(*buffer);

		auto client = shared_from_this();
		boost::asio::async_write(
			client->mSocket,
			boost::asio::const_buffer(buffer->data(), buffer->size()),
			[client, buffer, handler, qos](boost::system::error_code ec, size_t bytestransfer)
		{
			if (ec)
			{
				client->Disconnect(true);
				if (qos == MessageQOS::MQTTQOS0)
				{
					std::vector< MQTTErrorCode> results;
					results.push_back(MQTTErrorCode::CONNECTION_REFUSED);
					handler(results);
				}
				client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
			}
			else
			{
				if (qos == MessageQOS::MQTTQOS0)
				{
					std::vector< MQTTErrorCode> results;
					results.push_back(MQTTErrorCode::CONNECTION_REFUSED);
					handler(results);
				}
			}
		}
		);
	}
	void MQTTClient::Publish(const std::string& topic, const std::vector<uint8_t>& payload, MessageQOS qos, bool retain, MQTTTransactionCallback handler)
	{
		Publish(topic, payload.data(), payload.size(), qos, retain, handler);
	}
	void MQTTClient::Publish(const std::string& topic, const std::string& payload, MessageQOS qos, bool retain, MQTTTransactionCallback handler)
	{
		Publish(topic, reinterpret_cast<const uint8_t*>(payload.data()), payload.size(), qos, retain, handler);
	}

	void MQTTClient::Subscribe(const std::string& topic, MessageQOS qos, MQTTTransactionCallback handler)
	{
		std::vector<std::string> topics;
		topics.push_back(topic);
		Subscribe(topics, qos, handler);
	}

	void MQTTClient::Unsubscribe(const std::string& topic, MQTTTransactionCallback handler)
	{
		std::vector<std::string> topics;
		topics.push_back(topic);
		Unsubscribe(topics, handler);
	}

	void MQTTClient::Subscribe(const std::vector<std::string>& topics, MessageQOS qos, MQTTTransactionCallback handler)
	{
		if (handler == nullptr)
			handler = DefaultTransactionCallback;

		MQTTPacket packet(MQTTMessageType::SUBSCRIBE);
		auto tid = BeginTransaction(handler);
		packet.WriteUint16(tid);
		for (const auto& topic : topics)
		{
			packet.WriteUTF8String(topic);
			packet.WriteUint8(static_cast<uint8_t>(qos));
		}
		auto buffer = std::make_shared<std::vector<uint8_t>>();
		packet.ToBuffer(*buffer);

		auto client = shared_from_this();
		boost::asio::async_write(
			client->mSocket,
			boost::asio::const_buffer(buffer->data(), buffer->size()),
			[client, buffer](boost::system::error_code ec, size_t bytestransfer)
		{
			if (ec)
			{
				client->Disconnect(true);
				client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
			}
		}
		);
	}

	void MQTTClient::Unsubscribe(const std::vector<std::string>& topics, MQTTTransactionCallback handler)
	{
		if (handler == nullptr)
			handler = DefaultTransactionCallback;

		MQTTPacket packet(MQTTMessageType::UNSUBSCRIBE);
		auto tid = BeginTransaction(handler);
		for (const auto& topic : topics)
		{
			packet.WriteUint16(tid);
			packet.WriteUTF8String(topic);
		}
		auto buffer = std::make_shared<std::vector<uint8_t>>();
		packet.ToBuffer(*buffer);

		auto client = shared_from_this();
		boost::asio::async_write(
			client->mSocket,
			boost::asio::const_buffer(buffer->data(), buffer->size()),
			[client, buffer](boost::system::error_code ec, size_t bytestransfer)
		{
			if (ec)
			{
				client->Disconnect(true);
				client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
			}
		}
		);
	}

	void MQTTClient::onReadMQTTPacket(MQTTClientPtr client, IOBuffer buffer)
	{
		buffer->resize(2);
		boost::asio::async_read(
			client->mSocket,
			boost::asio::buffer(buffer->data(), buffer->size()),
			[client, buffer](boost::system::error_code ec, size_t bytestransfer)
		{
			if (!ec)
			{
				auto l = buffer->at(1);
				if ((l & 0x80) == 0x80)
				{
					client->onReadMQTTPacketLength(client, 2, buffer);
				}
				else
				{
					client->onReadMQTTPacketLengthCompleted(client, buffer);
				}
			}
			else
			{
				client->Disconnect(true);
				client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_DISCONN);
			}
		}
		);
	}

	void MQTTClient::onReadMQTTPacketLength(MQTTClientPtr client, size_t offset, IOBuffer buffer)
	{
		buffer->resize(offset + 1);
		boost::asio::async_read(
			client->mSocket,
			boost::asio::buffer(buffer->data() + offset, buffer->size() - offset),
			[client, offset, buffer](boost::system::error_code ec, size_t bytestransfer)
		{
			if (!ec)
			{
				auto l = buffer->at(offset);
				if ((l & 0x80) == 0x80)
				{
					client->onReadMQTTPacketLength(client, offset + 1, buffer);
				}
				else
				{
					client->onReadMQTTPacketLengthCompleted(client, buffer);
				}
			}
			else
			{
				client->Disconnect(true);
				client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_DISCONN);
			}
		}
		);
	}

	void MQTTClient::onReadMQTTPacketLengthCompleted(MQTTClientPtr client, IOBuffer buffer)
	{
		size_t length = 0;
		size_t lengthOffset;
		size_t i;
		for (i = 1; i < 5 && i < buffer->size(); ++i)
		{
			auto l = buffer->at(i);
			if (i == 1)
			{
				length = l & 0x7F;
			}
			else
			{
				length |= (l & 0x7F) << (7 * (i - 1));
			}
		}
		lengthOffset = i;
		if (length > kMAX_MESSAGE_PAYLOAD)
		{
			client->Disconnect(true);
			client->mDisconnectedCallback(MQTTErrorCode::PAYLOAD_EXCEED);
		}
		else
		{
			if (length > 0)
			{
				auto offset = buffer->size();
				buffer->resize(offset + length);
				boost::asio::async_read(
					client->mSocket,
					boost::asio::buffer(buffer->data() + offset, buffer->size() - offset),
					[client, offset, buffer, lengthOffset](boost::system::error_code ec, size_t bytestransfer)
				{
					if (!ec)
					{
						client->onReadMQTTPacketCompleted(client, buffer, lengthOffset);
					}
					else
					{
						client->Disconnect(true);
						client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_DISCONN);
					}
				}
				);
			}
			else
			{
				client->onReadMQTTPacketCompleted(client, buffer, lengthOffset);
			}
		}
	}

	void MQTTClient::onStartReadMQTTPacket(MQTTClientPtr client)
	{
		auto buffer = std::make_shared<std::vector<uint8_t>>();
		buffer->reserve(kMAX_MESSAGE_PAYLOAD + 2);
		client->onReadMQTTPacket(client, buffer);
	}

	void MQTTClient::onReadMQTTPacketCompleted(MQTTClientPtr client, IOBuffer buffer, size_t lengthOffset)
	{
		auto data = buffer->data();
		auto header = reinterpret_cast<MessageHeader*>(data);
		auto messageType = static_cast<MQTTMessageType>(header->bits.type);
		switch (messageType)
		{
		case MQTTMessageType::CONNACK:
		{
			//bool bPresent = (data[lengthOffset++] == 0x01);
			MQTTErrorCode connectReturnCode = static_cast<MQTTErrorCode>(data[lengthOffset++]);
			if (connectReturnCode == MQTTErrorCode::SUCCESS)
			{
				mConnected = true;
				client->onReadMQTTPacket(client, buffer);
			}

			mConnectedCallback(connectReturnCode);
			if (connectReturnCode != MQTTErrorCode::SUCCESS)
			{
				client->Disconnect(true);
				mDisconnectedCallback(connectReturnCode);
			}
			break;
		}
		case MQTTMessageType::PUBLISH:
		{
			client->onStartReadMQTTPacket(client);
			auto offset = lengthOffset;
			std::string topicName = MQTTPacket::ReadUTF8String(*buffer, offset);
			uint16_t packetIdentifier = 0;
			if (header->bits.qos > 0)
			{
				packetIdentifier = MQTTPacket::ReadUint16(*buffer, offset);
			}
			mMessageCallback(topicName, data + offset, buffer->size() - offset);

			if (header->bits.qos == static_cast<uint8_t>(MessageQOS::MQTTQOS1))
			{
				MQTTPacket packet(MQTTMessageType::PUBACK);
				packet.WriteUint16(packetIdentifier);
				packet.ToBuffer(*buffer);

				boost::asio::async_write(
					client->mSocket,
					boost::asio::const_buffer(buffer->data(), buffer->size()),
					[client, buffer](boost::system::error_code ec, size_t bytestransfer)
				{
					if (ec)
					{
						client->Disconnect(true);
						client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
					}
				}
				);
			}
			else if (header->bits.qos == static_cast<uint8_t>(MessageQOS::MQTTQOS2))
			{
				MQTTPacket packet(MQTTMessageType::PUBREC);
				packet.WriteUint16(packetIdentifier);
				packet.ToBuffer(*buffer);

				boost::asio::async_write(
					client->mSocket,
					boost::asio::const_buffer(buffer->data(), buffer->size()),
					[client, buffer](boost::system::error_code ec, size_t bytestransfer)
				{
					if (ec)
					{
						client->Disconnect(true);
						client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
					}
				}
				);
			}
			break;
		}
		case MQTTMessageType::PUBACK:
		{
			auto packetIdentifier = MQTTPacket::ReadUint16(*buffer, lengthOffset);
			client->onReadMQTTPacket(client, buffer);
			std::vector<MQTTErrorCode> results;
			results.push_back(MQTTErrorCode::SUCCESS);
			client->EndTransaction(packetIdentifier, results);
			break;
		}
		case MQTTMessageType::PUBREC:
		{
			client->onStartReadMQTTPacket(client);
			MQTTPacket packet(MQTTMessageType::PUBREL);
			auto packetIdentifier = MQTTPacket::ReadUint16(*buffer, lengthOffset);
			packet.WriteUint16(packetIdentifier);
			packet.ToBuffer(*buffer);
			boost::asio::async_write(
				client->mSocket,
				boost::asio::const_buffer(buffer->data(), buffer->size()),
				[client, buffer](boost::system::error_code ec, size_t bytestransfer)
			{
				if (ec)
				{
					client->Disconnect(true);
					client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
				}
			}
			);
			break;
		}
		case MQTTMessageType::PUBREL:
		{
			client->onStartReadMQTTPacket(client);
			MQTTPacket packet(MQTTMessageType::PUBCOMP);
			auto packetIdentifier = MQTTPacket::ReadUint16(*buffer, lengthOffset);
			packet.WriteUint16(packetIdentifier);
			packet.ToBuffer(*buffer);

			boost::asio::async_write(
				client->mSocket,
				boost::asio::const_buffer(buffer->data(), buffer->size()),
				[client, buffer](boost::system::error_code ec, size_t bytestransfer)
			{
				if (ec)
				{
					client->Disconnect(true);
					client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
				}
			}
			);
			break;
		}
		case MQTTMessageType::PUBCOMP:
		{
			auto packetIdentifier = MQTTPacket::ReadUint16(*buffer, lengthOffset);
			client->onReadMQTTPacket(client, buffer);
			std::vector<MQTTErrorCode> results;
			results.push_back(MQTTErrorCode::SUCCESS);
			client->EndTransaction(packetIdentifier, results);
			break;
		}
		case MQTTMessageType::SUBACK:
		{
			auto offset = lengthOffset;
			auto packetIdentifier = MQTTPacket::ReadUint16(*buffer, offset);
			std::vector<MQTTErrorCode> results;
			while (offset < buffer->size())
			{
				results.push_back(static_cast<MQTTErrorCode>(data[offset++]));
			}

			client->onReadMQTTPacket(client, buffer);
			client->EndTransaction(packetIdentifier, results);
			break;
		}
		case MQTTMessageType::UNSUBACK:
		{
			auto packetIdentifier = MQTTPacket::ReadUint16(*buffer, lengthOffset);
			client->onReadMQTTPacket(client, buffer);
			std::vector<MQTTErrorCode> results;
			results.push_back(MQTTErrorCode::SUCCESS);
			client->EndTransaction(packetIdentifier, results);
			break;
		}
		case MQTTMessageType::PINGREQ:
		{
			client->onStartReadMQTTPacket(client);
			MQTTPacket packet(MQTTMessageType::PINGREQ);
			packet.ToBuffer(*buffer);
			boost::asio::async_write(
				client->mSocket,
				boost::asio::const_buffer(buffer->data(), buffer->size()),
				[client, buffer](boost::system::error_code ec, size_t bytestransfer)
			{
				if (ec)
				{
					client->Disconnect(true);
					client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_REFUSED);
				}
				else
				{
					client->mPingCallback(MQTTErrorCode::SUCCESS);
				}
			}
			);
			break;
		}
		case MQTTMessageType::PINGRESP:
		{
			client->onReadMQTTPacket(client, buffer);
			client->mPingCallback(MQTTErrorCode::SUCCESS);
			break;
		}
		default:
		{
			client->Disconnect(true);
			client->mDisconnectedCallback(MQTTErrorCode::CONNECTION_DISCONN);
			break;
		}
		}
	}

	uint16_t MQTTClient::BeginTransaction(MQTTTransactionCallback handler)
	{
		std::unique_lock<std::mutex> lock(mTransactionMutex);
		uint16_t tId = 0;
		auto start = mPacketIdentifier;
		uint16_t temp = uint16_t(start + uint16_t(1));
		while (true)
		{
			if (mTransactions.find(temp) == mTransactions.end())
			{
				tId = temp;
				break;
			}
			if (temp == start)
				break;
			if (++temp == 0)
				temp = 1;
		}

		if (tId == 0)
			return 0;
		mPacketIdentifier = uint16_t(tId + uint16_t(1));
		mTransactions.insert(std::make_pair(tId, handler));
		return tId;
	}
	void MQTTClient::EndTransaction(uint16_t transactionId, const std::vector<MQTTErrorCode>& results)
	{
		MQTTTransactionCallback handler;
		{
			std::unique_lock<std::mutex> lock(mTransactionMutex);
			auto found = mTransactions.find(transactionId);
			if (found == mTransactions.end())
				return;
			handler = found->second;
			mTransactions.erase(found);
		}
		if (handler != nullptr)
		{
			handler(results);
		}
	}
}