#pragma once
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace MQTT
{
	enum class MQTTVersion : uint8_t
	{
		VERSION_3_1 = 3,
		VERSION_3_1_1 = 4
	};

	enum class MessageQOS : uint8_t
	{
		MQTTQOS0 = 0,
		MQTTQOS1 = 1,
		MQTTQOS2 = 2
	};

	enum class MQTTErrorCode : uint8_t
	{
		SUCCESS = 0,
		UNACCEPTABLE_PROTOCOL_VERSION = 1,
		IDENTIFIER_REJECTED = 2,
		SERVER_UNAVAILABLE = 3,
		BAD_USER_NAME_OR_PASSWORD = 4,
		NOT_AUTHORIZED = 5,
		SUBSCRIBE_QOS0 = 0x00,
		SUBSCRIBE_QOS1 = 0x01,
		SUBSCRIBE_QOS2 = 0x02,
		SUBSCRIBE_FAILURE = 0x80,
		PAYLOAD_EXCEED = 252,
		CONNECTION_DISCONN = 253,
		CONNECTION_REFUSED = 254,
		CONNECTION_TIMEOUT = 255,
	};

	struct ConnectionOptions
	{
		MQTTVersion Version;
		bool CleanSession;
		struct
		{
			std::string Topic;
			MessageQOS Qos;
			bool Retain;
			std::vector<uint8_t> Data;
		}Will;
		std::string UserName;
		std::string Password;
		uint16_t KeepAlive;
		std::string ClientID;
		std::string serverURL;
	};


	class MQTTClient : public std::enable_shared_from_this<MQTTClient>
	{
	public:
		using tcp = boost::asio::ip::tcp;
		using MQTTEventCallback = std::function<void(MQTTErrorCode errorCode)>;
		using MQTTTransactionCallback = std::function<void(const std::vector<MQTTErrorCode>& errorCodes)>;
		using MQTTMessageCallback = std::function<void(const std::string& topic, const uint8_t* payload, size_t size)>;
		using MQTTClientPtr = std::shared_ptr<MQTTClient>;
		using IOBuffer = std::shared_ptr<std::vector<uint8_t>>;
	public:
		MQTTClient(boost::asio::io_service& ios);
		~MQTTClient();
	public:
		bool Connected(void)const { return mConnected; }
		void Connect(const ConnectionOptions& options);
		void Disconnect(bool forced);
		void Publish(const std::string& topic, const std::vector<uint8_t>& payload, MessageQOS qos, bool retain, MQTTTransactionCallback handler);
		void Publish(const std::string& topic, const std::string& payload, MessageQOS qos, bool retain, MQTTTransactionCallback handler);
		void Subscribe(const std::string& topic, MessageQOS qos, MQTTTransactionCallback handler);
		void Unsubscribe(const std::string& topic, MQTTTransactionCallback handler);
		void Subscribe(const std::vector<std::string>& topic, MessageQOS qos, MQTTTransactionCallback handler);
		void Unsubscribe(const std::vector<std::string>& topic, MQTTTransactionCallback handler);
	public:
		void OnConnected(MQTTEventCallback handler);
		void OnDisconnected(MQTTEventCallback handler);
		void OnMessage(MQTTMessageCallback handler);
	private:
		void Publish(const std::string& topic, const uint8_t* payload, size_t payloadSize, MessageQOS qos, bool retain, MQTTTransactionCallback handler);
		void Ping(MQTTClientPtr client);
	private:
		void onReadMQTTPacket(MQTTClientPtr client, IOBuffer buffer);
		void onReadMQTTPacketLength(MQTTClientPtr client, size_t offset, IOBuffer buffer);
		void onReadMQTTPacketLengthCompleted(MQTTClientPtr client, IOBuffer buffer);
		void onReadMQTTPacketCompleted(MQTTClientPtr client, IOBuffer buffer, size_t lengthOffset);
		void onStartReadMQTTPacket(MQTTClientPtr client);
		void onStartPingTimer(MQTTClientPtr client);
	private:
		uint16_t BeginTransaction(MQTTTransactionCallback handler);
		void EndTransaction(uint16_t transactionId, const std::vector<MQTTErrorCode>& results);
	private:
		tcp::socket mSocket;
		ConnectionOptions mConnectOptions;
		bool mConnected;
		uint16_t mPacketIdentifier;
		std::mutex mTransactionMutex;
		std::unordered_map<uint16_t, MQTTTransactionCallback> mTransactions;
		std::shared_ptr<boost::asio::deadline_timer> mPingTimer;
		MQTTEventCallback mConnectedCallback;
		MQTTEventCallback mDisconnectedCallback;
		MQTTEventCallback mPingCallback;
		MQTTMessageCallback mMessageCallback;
	};
}