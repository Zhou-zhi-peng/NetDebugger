#include "pch.h"
#include "Websocket.h"
#include "OEMStringHelper.hpp"
#include "NetDebugger.h"
#include <algorithm>
#include <regex>
#include <random>

#include "SHA1.h"
#include "Base64.h"

class WebSocketHeader
{
public:
	WebSocketHeader()
	{
	}

	~WebSocketHeader()
	{

	}
public:
	void ToBuffer(std::vector<uint8_t>& buffer) const
	{
		buffer.reserve(128 + static_cast<size_t>(payloadLength));
		buffer.resize(1);
		buffer[0] = ((uint8_t)(opcode));
		if(fin)
			buffer[0] |= 0x80;
		if (payloadLength < 126) {

			buffer.resize(2);
			buffer[1] = (payloadLength & 0xff);
		}
		else if (payloadLength < 65536)
		{
			buffer.resize(4);
			buffer[1] = 126;
			buffer[2] = (payloadLength >> 8) & 0xff;
			buffer[3] = (payloadLength >> 0) & 0xff;
		}
		else 
		{
			buffer.resize(10);
			buffer[1] = 127;
			buffer[2] = (payloadLength >> 56) & 0xff;
			buffer[3] = (payloadLength >> 48) & 0xff;
			buffer[4] = (payloadLength >> 40) & 0xff;
			buffer[5] = (payloadLength >> 32) & 0xff;
			buffer[6] = (payloadLength >> 24) & 0xff;
			buffer[7] = (payloadLength >> 16) & 0xff;
			buffer[8] = (payloadLength >> 8) & 0xff;
			buffer[9] = (payloadLength >> 0) & 0xff;
		}

		if (mask)
		{
			buffer[1] |= 0x80;
			buffer.push_back(masking_key[0]);
			buffer.push_back(masking_key[1]);
			buffer.push_back(masking_key[2]);
			buffer.push_back(masking_key[3]);
			auto data = reinterpret_cast<const uint8_t*>(payload);
			for (size_t i = 0; i != static_cast<size_t>(payloadLength); ++i)
			{
				buffer.push_back(data[i] ^ masking_key[i & 0x3]);
			}
		}
		else
		{
			auto data = reinterpret_cast<const uint8_t*>(payload);
			buffer.insert(buffer.end(), data, data + static_cast<size_t>(payloadLength));
		}
	}

	void ReadPacket(
		std::shared_ptr<WebSocketChannel> channel,
		boost::asio::ip::tcp::socket& sock, 
		void* buffer,
		size_t bufferSize, 
		WebSocketChannel::IoCompletionHandler handler)
	{
		auto packet = std::make_shared<std::vector<uint8_t>>();
		packet->reserve(bufferSize + 128);
		packet->resize(2);
		boost::asio::async_read(
			sock,
			boost::asio::buffer(packet->data(), packet->size()),
			[channel, &sock, buffer, bufferSize, packet, handler,this](const boost::system::error_code& ec, size_t bytestransfer)
		{

			if (ec || bytestransfer==0)
			{
				auto err = ec;
				if (bytestransfer == 0)
					err = boost::system::errc::make_error_code(boost::system::errc::connection_reset);

				if (handler != nullptr)
					handler(false, bytestransfer);

				channel->CloseSocket(err);
			}
			else
			{
				auto data = packet->data();
				fin = (data[0] & 0x80) == 0x80;
				opcode = (WebSocketHeader::opcode_type)(data[0] & 0x0f);
				mask = (data[1] & 0x80) == 0x80;
				payloadLength = (data[1] & 0x7f);

				if (payloadLength < 126)
				{
					ReadPacketData(channel, sock, buffer, bufferSize, packet, handler);
				}
				else
				{
					if (payloadLength == 126)
					{
						ReadPacketLength(channel, sock, buffer, bufferSize, packet, 2, handler);
					}
					else
					{
						ReadPacketLength(channel, sock, buffer, bufferSize, packet, 8, handler);
					}
				}
			}
		});
	}

	void setRandomMaskKey()
	{
		std::default_random_engine rg(static_cast<size_t>(std::time(nullptr)));
		std::uniform_int_distribution<int> n(1, 255);
		masking_key[0] = static_cast<uint8_t>(n(rg));
		masking_key[1] = static_cast<uint8_t>(n(rg));
		masking_key[2] = static_cast<uint8_t>(n(rg));
		masking_key[3] = static_cast<uint8_t>(n(rg));
	}
private:
	void ReadPacketLength(
		std::shared_ptr<WebSocketChannel> channel,
		boost::asio::ip::tcp::socket& sock,
		void* buffer,
		size_t bufferSize,
		std::shared_ptr<std::vector<uint8_t>> packet,
		size_t lengthSize,
		WebSocketChannel::IoCompletionHandler handler)
	{
		size_t offset = packet->size();
		packet->resize(offset + lengthSize);
		boost::asio::async_read(
			sock,
			boost::asio::buffer(packet->data() + offset, packet->size() - offset),
			[channel, &sock, buffer, bufferSize, packet, handler, this](const boost::system::error_code& ec, size_t bytestransfer)
		{

			if (ec || bytestransfer == 0)
			{
				auto err = ec;
				if (bytestransfer == 0)
					err = boost::system::errc::make_error_code(boost::system::errc::connection_reset);

				if (handler != nullptr)
					handler(false, bytestransfer);

				channel->CloseSocket(err);
			}
			else
			{
				auto data = packet->data();
				if (payloadLength == 126)
				{
					payloadLength = (data[2] << 8) & 0xff00;
					payloadLength |= (data[3] << 0) & 0x00ff;
				}
				else
				{
					payloadLength =  (static_cast<uint64_t>(data[9]) << 0);
					payloadLength |= (static_cast<uint64_t>(data[8]) << 8);
					payloadLength |= (static_cast<uint64_t>(data[7]) << 16);
					payloadLength |= (static_cast<uint64_t>(data[6]) << 24);
					payloadLength |= (static_cast<uint64_t>(data[5]) << 32);
					payloadLength |= (static_cast<uint64_t>(data[4]) << 40);
					payloadLength |= (static_cast<uint64_t>(data[3]) << 48);
					payloadLength |= (static_cast<uint64_t>(data[2]) << 56);
				}
				ReadPacketData(channel, sock, buffer, bufferSize, packet, handler);
			}
		});
	}

	void ReadPacketData(
		std::shared_ptr<WebSocketChannel> channel,
		boost::asio::ip::tcp::socket& sock,
		void* buffer,
		size_t bufferSize,
		std::shared_ptr<std::vector<uint8_t>> packet,
		WebSocketChannel::IoCompletionHandler handler)
	{
		size_t offset = packet->size();
		size_t maskKeyLen = mask ? 4 : 0;
		packet->resize(static_cast<size_t>(offset + payloadLength + maskKeyLen));
		payload = packet->data() + offset;
		headerSize = offset + maskKeyLen;
		boost::asio::async_read(
			sock,
			boost::asio::buffer(packet->data() + offset, packet->size() - offset),
			[channel, &sock, buffer, bufferSize, packet, handler, this](const boost::system::error_code& ec, size_t bytestransfer)
		{

			if (ec || bytestransfer == 0)
			{
				auto err = ec;
				if (bytestransfer == 0)
					err = boost::system::errc::make_error_code(boost::system::errc::connection_reset);

				if (handler != nullptr)
					handler(false, bytestransfer);

				channel->CloseSocket(err);
			}
			else
			{
				auto payloadBuffer = reinterpret_cast<uint8_t*>(buffer);
				if (mask) 
				{
					memcpy(masking_key, payload, sizeof(masking_key));
					payload += sizeof(masking_key);
					for (uint64_t i = 0; i < payloadLength; i++)
					{
						int j = i % 4;
						payloadBuffer[i] = payload[i] ^ masking_key[j];
					}
				}
				else
				{
					memcpy(payloadBuffer, payload, static_cast<size_t>(payloadLength));
				}
				if (handler != nullptr)
					handler(!ec, bytestransfer);
			}
		});
	}
public:
	unsigned headerSize;
	bool fin;
	bool mask;
	enum opcode_type {
		CONTINUATION = 0x0,
		TEXT_FRAME = 0x1,
		BINARY_FRAME = 0x2,
		CLOSE = 0x08,
		PING = 0x09,
		PONG = 0xa,
	} opcode;
	uint64_t payloadLength;
	uint8_t masking_key[4];
	const uint8_t* payload;
};


static bool URLParse(const std::string& url, std::string& host, int& port, std::string& query, std::string& path, bool& useWSS)
{
	if (url.length() <= 0)
		return false;

	auto pos = url.find_first_of(':');
	if (pos == url.npos)
		return false;
	if (url.length() <= pos + 3)
		return false;
	auto prot = url.substr(0, pos);
	std::transform(prot.begin(), prot.end(), prot.begin(), [](char ch) { return static_cast<char>(std::toupper(ch)); });
	useWSS = (prot == "wss");
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
				char* endptr = nullptr;
				port = static_cast<int>(std::strtoul(sport.c_str(), &endptr, 10));
				if (endptr != nullptr)
					query = endptr;
				else
					query.insert(query.begin(), '/');
			}
			else
			{
				port = -1;
				pos = sport.find_first_of('/');
				if (pos != sport.npos)
					query = sport.substr(pos);
				else
					query.insert(query.begin(), '/');
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
			char* endptr = nullptr;
			port = static_cast<int>(strtoul(sport.c_str(), &endptr, 10));
			if (endptr != nullptr)
				query = endptr;
			else
				query.insert(query.begin(), '/');
		}
		else
		{
			host = address;
			port = -1;
			pos = address.find_first_of('/');
			if (pos != address.npos)
				query = address.substr(pos);
			else
				query.insert(query.begin(), '/');
		}
	}
	if (!query.empty())
	{
		if(query.at(0)!='/')
			query.insert(path.begin(), '/');
		path = query;
		pos = path.find_first_of('&');
		if (pos != path.npos)
			path = path.substr(0, pos);

		pos = path.find_first_of('#');
		if (pos != path.npos)
			path = path.substr(0, pos);
	}
	return true;
}

static std::string GetRandomString()
{
	static const char chars_table[] =
	{
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
	};

	std::default_random_engine rg(static_cast<size_t>(std::time(nullptr)));
	std::uniform_int_distribution<int> n(0, sizeof(chars_table) - 1);
	char buffer[16];
	for (int i = 0; i < sizeof(buffer); ++i)
	{
		buffer[i] = chars_table[static_cast<size_t>(n(rg))];
	}
	return std::string(buffer, sizeof(buffer));
}

static std::string GetHttpHeaderValue(const std::map<std::string, std::string>& headers, const std::string& key, const std::string& defaultValue)
{
	auto it = headers.find(key);
	if (it == headers.end())
		return defaultValue;
	return it->second;
}

static std::string GetHttpHeaderValue(const std::map<std::string, std::string>& headers, const std::string& key)
{
	return GetHttpHeaderValue(headers, key, std::string());
}

static std::shared_ptr<std::string> HttpConnectString(
	const std::string& query, 
	const std::string& host,
	const std::string& id,
	int port, 
	const std::map<std::string, std::string>& headers)
{
	auto result = std::make_shared<std::string>();
	std::string& s = *result;
	s = "GET ";
	s += query.empty() ? std::string("/") : query;
	s += " HTTP/1.1\r\n";

	s += "Host: ";
	s += host;
	s += ":";
	s += std::to_string(port);
	s += "\r\n";

	s += "Upgrade: websocket\r\n";
	s += "Connection: Upgrade\r\n";

	s += "Sec-WebSocket-Key: ";
	s += Base64EncodeString(id);
	s += "\r\n";

	s += "Sec-WebSocket-Version: 13\r\n";

	for (auto h : headers)
	{
		s += h.first;
		s += " ";
		s += h.second;
		s += "\r\n";
	}
	s += "\r\n";
	return result;
}

static std::string GetHttpStatusMessage(int code)
{
	switch (code)
	{
	case 100: return "Continue";
	case 101: return "Switching Protocol";
	case 102: return "Processing WebDAV";
	case 103: return "Early Hints";
	case 200: return "OK";
	case 201: return "Created";
	case 202: return "Accepted";
	case 203: return "Non-Authoritative Information";
	case 204: return "No Content";
	case 205: return "Reset Content";
	case 206: return "Partial Content";
	case 207: return "Multi-Status WebDAV";
	case 208: return "Already Reported WebDAV";
	case 226: return "IM Used";
	case 300: return "Multiple Choice";
	case 301: return "Moved Permanently";
	case 302: return "Found";
	case 303: return "See Other";
	case 304: return "Not Modified";
	case 307: return "Temporary Redirect";
	case 308: return "Permanent Redirect";
	case 400: return "Bad Request";
	case 401: return "Unauthorized";
	case 402: return "Payment Required";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 406: return "Not Acceptable";
	case 407: return "Proxy Authentication Required";
	case 408: return "Request Timeout";
	case 409: return "Conflict";
	case 410: return "Gone";
	case 411: return "Length Required";
	case 412: return "Precondition Failed";
	case 413: return "Payload Too Large";
	case 414: return "URI Too Long";
	case 415: return "Unsupported Media Type";
	case 416: return "Range Not Satisfiable";
	case 417: return "Expectation Failed";
	case 418: return "I'm a teapot";
	case 421: return "Misdirected Request";
	case 422: return "Unprocessable Entity(WebDAV)";
	case 423: return "Locked(WebDAV)";
	case 424: return "Failed Dependency(WebDAV)";
	case 425: return "Too Early";
	case 426: return "Upgrade Required";
	case 428: return "Precondition Required";
	case 429: return "Too Many Requests";
	case 431: return "Request Header Fields Too Large";
	case 451: return "Unavailable For Legal Reasons";
	case 500: return "Internal Server Error";
	case 501: return "Not Implemented";
	case 502: return "Bad Gateway";
	case 503: return "Service Unavailable";
	case 504: return "Gateway Timeout";
	case 505: return "HTTP Version Not Supported";
	case 506: return "Variant Also Negotiates";
	case 507: return "Insufficient Storage(WebDAV)";
	case 508: return "Loop Detected(WebDAV)";
	case 510: return "Not Extended";
	case 511: return "Network Authentication Required";
	default:
		return "Unused";
	}
}

static std::shared_ptr<std::string> HttpConnectResponseString(
	int httpStatus,
	const std::string& httpVersion,
	const std::map<std::string, std::string>& reqheaders,
	const std::map<std::string, std::string>& respheaders)
{
	auto result = std::make_shared<std::string>();
	std::string& s = *result;
	SHA1 sha1;

	s = "HTTP/";
	s += httpVersion;
	s += " ";
	s += std::to_string(httpStatus);
	s += " ";
	s += GetHttpStatusMessage(httpStatus);
	s += " \r\n";

	s += "Server: NetDebugger-WebSocket Server/1.0.0.1\r\n";

	s += "Upgrade: websocket\r\n";
	s += "Connection: Upgrade\r\n";

	sha1.Update(GetHttpHeaderValue(reqheaders, "Sec-WebSocket-Key"));
	sha1.Update(std::string("258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
	
	s += "Sec-WebSocket-Accept: ";
	s += Base64EncodeString(sha1.Final());
	s += "\r\n";

	s += "Sec-WebSocket-Version: 13\r\n";

	for (auto h : respheaders)
	{
		s += h.first;
		s += " ";
		s += h.second;
		s += "\r\n";
	}
	s += "\r\n";
	return result;
}

static bool GetHeaderLine(std::istream& input, std::string& line)
{
	line.clear();
	while (!input.eof())
	{
		int c = input.get();
		if (c == '\n') 
		{
			if (!line.empty() && line[line.length() - 1] == '\r')
			{
				line.pop_back();
				break;
			}
		}
		line.push_back((char)c);
	}
	return !line.empty();
}

static void ParseHttpHeaders(std::istream& input, std::map<std::string, std::string>& headers)
{
	std::string line;
	headers.clear();
	while (GetHeaderLine(input, line))
	{
		auto pos = line.find_first_of(": ");
		if (pos != line.npos)
			headers[line.substr(0, pos)] = line.substr(pos + 2);
		else
			headers[line] = std::string();
	}
}

static std::string GetHttpQueryStringPath(const std::string& query)
{
	auto pos = query.find_first_of("?#");
	if (pos != query.npos)
	{
		auto path = query.substr(0, pos);
		if (path.empty())
			path = "/";
		return path;
	}
	return query;
}

static bool GetHttpPathCompare(const std::string& p0, const std::string& p1)
{
	if (p0.empty() && p1.empty())
		return true;
	auto pt0 = p0.data();
	auto pt1 = p1.data();
	auto e0 = pt0 + p0.length();
	auto e1 = pt1 + p1.length();
	if (*pt0 == '/')
		++pt0;

	if (*pt1 == '/')
		++pt1;
	return std::strcmp(pt0, pt1) == 0;
}

WebSocketChannel::WebSocketChannel() :
	m_Socket(theApp.GetIOContext()),
	m_Opened(false)
{

}

WebSocketChannel::~WebSocketChannel(void)
{
	CloseSocket();
}

bool WebSocketChannel::CloseSocket(void)
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

void WebSocketChannel::CloseSocket(const boost::system::error_code& ecClose)
{
	if (CloseSocket())
	{
		OnNotifyClose(ecClose);
	}
}

std::wstring WebSocketChannel::Id(void) const
{
	if (sizeof(size_t) == sizeof(void*))
		return std::to_wstring(reinterpret_cast<size_t>(this));
	else
		return std::to_wstring(reinterpret_cast<uint64_t>(this));
}

std::wstring WebSocketChannel::Description(void) const
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

std::wstring WebSocketChannel::LocalEndPoint(void) const
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

std::wstring WebSocketChannel::RemoteEndPoint(void) const
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

void WebSocketChannel::Read(void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	auto header = std::make_shared<WebSocketHeader>();
	auto channel = shared_from_this();
	header->ReadPacket(channel, m_Socket, buffer, bufferSize, [header, channel, handler](bool ok, size_t bytestransfer)
	{
		if (handler != nullptr)
			handler(ok, bytestransfer);
		if (!ok)
		{
			channel->CloseSocket(boost::system::errc::make_error_code(boost::system::errc::connection_reset));
		}
	});
}

void WebSocketChannel::Write(const void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	WebSocketHeader header;
	header.fin = true;
	header.opcode = IsBinaryMode() ? WebSocketHeader::BINARY_FRAME : WebSocketHeader::TEXT_FRAME;
	header.mask = IsMessageMasked();
	header.payload = reinterpret_cast<const uint8_t*>(buffer);
	header.payloadLength = bufferSize;

	auto packet = std::make_shared<std::vector<uint8_t>>();
	auto client = shared_from_this();
	if (header.mask)
		header.setRandomMaskKey();
	header.ToBuffer(*packet);
	boost::asio::async_write(
		m_Socket,
		boost::asio::const_buffer(packet->data(), packet->size()),
		[client, handler, packet](const boost::system::error_code& ec, size_t bytestransfer)
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
void WebSocketChannel::ReadSome(void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	Read(buffer, bufferSize, handler);
}
void WebSocketChannel::WriteSome(const void* buffer, size_t bufferSize, IoCompletionHandler handler)
{
	Write(buffer, bufferSize, handler);
}

void WebSocketChannel::Cancel(void)
{
	boost::system::error_code ec;
	m_Socket.cancel(ec);
}

void WebSocketChannel::Close(void)
{
	CloseSocket(boost::system::errc::make_error_code(boost::system::errc::connection_aborted));
}

bool WebSocketChannel::TryOpen(void)
{
	bool state = false;
	if (m_Opened.compare_exchange_weak(state, true))
		return true;
	return false;
}


WebSocketClientChannel::WebSocketClientChannel()
{

}

WebSocketClientChannel::~WebSocketClientChannel(void)
{
}


void WebSocketClientChannel::Connect(
	const std::wstring& endpoint,
	const std::map<std::string, std::string>& headers,
	std::function<void(const boost::system::error_code ec)> handler)
{
	std::string host;
	std::string path;
	std::string query;
	int port = 0;
	bool useWSS = false;
	if (URLParse(WStringToString(endpoint), host, port, query, path, useWSS))
	{
		if (TryOpen())
		{
			m_RequestHeaders = headers;
			m_ConnectQueryString = query;
			auto channel = shared_from_this();
			boost::system::error_code ec;
			auto rslv = std::make_shared<boost::asio::ip::tcp::resolver>(GetSocket().get_io_context());
			boost::asio::ip::tcp::resolver::query qry(host, std::to_string(port));
			rslv->async_resolve(qry, [rslv, channel, query, host, port, handler](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator iter)
			{
				if (ec)
				{
					handler(ec);
				}
				else
				{
					boost::asio::async_connect(
						channel->GetSocket(),
						iter,
						[rslv, channel, query, host, port, handler](const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator iter)
					{
						if (ec)
						{
							channel->CloseSocket();
							handler(ec);
						}
						else
						{
							auto pChannel = dynamic_cast<WebSocketClientChannel*>(channel.get());
							pChannel->OnSocketConnected(channel, query, host, port, handler);
						}
					});
				}
			});
		}
	}
	else
	{
		handler(boost::system::errc::make_error_code(boost::system::errc::bad_address));
	}
}

void WebSocketClientChannel::OnSocketConnected(
	std::shared_ptr<WebSocketChannel> channel,
	std::string query,
	std::string host,
	int port,
	std::function<void(const boost::system::error_code ec)> handler)
{
	boost::system::error_code ecSet;
	channel->GetSocket().set_option(boost::asio::socket_base::keep_alive(true), ecSet);
	auto pChannel = dynamic_cast<WebSocketClientChannel*>(channel.get());
	auto data = HttpConnectString(query, host, GetRandomString(), port, pChannel->m_RequestHeaders);
	boost::asio::async_write(
		channel->GetSocket(),
		boost::asio::const_buffer(data->data(), data->length()),
		[channel, handler, data, this](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if (ec)
			handler(ec);
		else
		{
			auto headerBuf = std::make_shared<boost::asio::streambuf>(1024 * 64);
			boost::asio::async_read_until(
				channel->GetSocket(),
				*headerBuf,
				"\r\n\r\n",
				[channel, handler, headerBuf, this](const boost::system::error_code& ec, size_t bytestransfer)
			{
				if (ec)
					handler(ec);
				else
				{
					std::istream input(headerBuf.get());
					std::string line;
					if (!GetHeaderLine(input, line))
						handler(boost::system::errc::make_error_code(boost::system::errc::bad_message));
					else
					{
						std::regex re("HTTP/([0-9]+\\.[0-9]+)\\s+([0-9]+)\\s+(.*)", std::regex::icase | std::regex::ECMAScript);
						std::smatch sm;
						std::regex_search(line, sm, re);
						if (sm.empty())
							handler(boost::system::errc::make_error_code(boost::system::errc::bad_message));
						else
						{
							m_HttpVersion = sm[1].str();
							auto code = std::strtoul(sm[2].str().c_str(), nullptr, 10);
							if (code == 101)
							{
								ParseHttpHeaders(input, m_ResponseHeaders);
								handler(ec);
							}
							else
							{
								handler(boost::system::errc::make_error_code(boost::system::errc::connection_reset));
							}
						}
					}
				}
			});
		}
	});
}

void WebSocketClientChannel::OnNotifyClose(const boost::system::error_code& ecClose)
{
	auto dev = m_Device.lock();
	if (dev != nullptr)
	{
		dev->NotifyChannelClose(shared_from_this(), ecClose);
	}
}

bool WebSocketClientChannel::IsBinaryMode(void)
{
	auto dev = m_Device.lock();
	if (dev == nullptr)
		return true;
	return dev->BinaryMode();
}

bool WebSocketClientChannel::IsMessageMasked(void)
{
	auto dev = m_Device.lock();
	if (dev == nullptr)
		return false;
	return dev->MessageMasked();
}


WebSocketClient::WebSocketClient(void):
	m_bBinaryMode(false),
	m_bMessageMasked(false),
	m_ServerURL(L"ws://localhost:8080/test")
{
	m_Headers.resize(6);
}

WebSocketClient::~WebSocketClient(void)
{
}

bool WebSocketClient::IsServer(void)
{
	return false;
}

WebSocketClient::PDTable WebSocketClient::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	PDTable results;
	auto self = GetSharedPtr();
	auto pd = std::make_shared<StaticProperty>(
		L"Host",
		L"DEVICE.CLASS.WEBSOCKETCLIENT.PROP.REMOTEHOST",
		L"ws://localhost:8080/test",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_ServerURL; },
		[self](const std::wstring& value) { self->m_ServerURL = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Mode",
		L"DEVICE.WEBSOCKETCLIENT.PROP.DATAMODE",
		std::wstring(L"TEXT"),
		IDevice::PropertyChangeFlags::CanChangeAlways
		);
	pd->AddEnumOptions(L"BINARY",L"DEVICE.WEBSOCKET.PROP.DATAMODE.EM0",true);
	pd->AddEnumOptions(L"TEXT", L"DEVICE.WEBSOCKET.PROP.DATAMODE.EM1", false);
	pd->BindMethod(
		[self]() { return self->m_bBinaryMode ? L"BINARY": L"TEXT"; },
		[self](const std::wstring& value) { self->m_bBinaryMode = value == L"BINARY"; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Masked",
		L"DEVICE.WEBSOCKETCLIENT.PROP.MASKED",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeAlways
		);
	pd->BindMethod(
		[self]() { return self->m_bMessageMasked ? L"1" : L"0"; },
		[self](const std::wstring& value) { self->m_bMessageMasked = value == L"1"; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"AdditionalHeaders[0]", 
		L"DEVICE.WEBSOCKETCLIENT.PROP.AH0",
		L"",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_Headers[0]; },
		[self](const std::wstring& value) { self->m_Headers[0] = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"AdditionalHeaders[1]",
		L"DEVICE.WEBSOCKETCLIENT.PROP.AH1",
		L"",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_Headers[1]; },
		[self](const std::wstring& value) { self->m_Headers[1] = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"AdditionalHeaders[2]",
		L"DEVICE.WEBSOCKETCLIENT.PROP.AH2",
		L"",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_Headers[2]; },
		[self](const std::wstring& value) { self->m_Headers[2] = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"AdditionalHeaders[3]",
		L"DEVICE.WEBSOCKETCLIENT.PROP.AH3",
		L"",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_Headers[3]; },
		[self](const std::wstring& value) { self->m_Headers[3] = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"AdditionalHeaders[4]",
		L"DEVICE.WEBSOCKETCLIENT.PROP.AH4",
		L"",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_Headers[4]; },
		[self](const std::wstring& value) { self->m_Headers[4] = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"AdditionalHeaders[5]",
		L"DEVICE.WEBSOCKETCLIENT.PROP.AH5",
		L"",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_Headers[5]; },
		[self](const std::wstring& value) { self->m_Headers[5] = value; }
	);
	results.push_back(pd);
	return results;
}

std::map<std::string, std::string> WebSocketClient::GetHttpHeader()
{
	std::map<std::string, std::string> headers;
	for (auto line : m_Headers)
	{
		if (line.empty())
			continue;
		auto pos = line.find_first_of(L": ");
		if (pos != line.npos)
			headers[WStringToString(line.substr(0, pos))] = WStringToString(line.substr(pos + 2));
		else
			headers[WStringToString(line)] = std::string();
	}
	return headers;
}





WebSocketSingleClient::WebSocketSingleClient(void) :
	m_Channel(std::make_shared<WebSocketClientChannel>())
{

}

WebSocketSingleClient::~WebSocketSingleClient(void)
{
	m_Channel.reset();
}


void WebSocketSingleClient::Start(void)
{
	if (Started())
		return;

	StatusChanged(DeviceStatus::Connecting, std::wstring());
	auto client = shared_from_this();
	m_Channel->Connect(m_ServerURL, GetHttpHeader(),[client](const boost::system::error_code& ec)
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

void WebSocketSingleClient::Stop(void)
{
	if (m_Channel->CloseSocket())
	{
		ChannelDisconnected(m_Channel, std::wstring());
		PropertyChanged();
		StatusChanged(DeviceStatus::Disconnected, std::wstring());
	}
}

void WebSocketSingleClient::NotifyChannelClose(std::shared_ptr<WebSocketChannel> channel, const boost::system::error_code& ecClose)
{
	ChannelDisconnected(m_Channel, std::wstring());
	PropertyChanged();
	StatusChanged(DeviceStatus::Disconnected, std::wstring());
}




WebSocketMultipleClient::WebSocketMultipleClient(void) :
	m_Channels(),
	m_ConnectConuter(0),
	m_FailedConuter(0)
{

}

WebSocketMultipleClient::~WebSocketMultipleClient(void)
{

}

IDevice::PDTable WebSocketMultipleClient::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	auto properties = WebSocketClient::EnumProperties();
	auto self = shared_from_this();
	auto pd = std::make_shared<StaticProperty>(
		L"CanonsCount",
		L"DEVICE.WEBSOCKETMULTIPLECLIENT.PROP.CANONSCOUNT",
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
		L"DEVICE.WEBSOCKETMULTIPLECLIENT.PROP.CONNCOUNT",
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

void WebSocketMultipleClient::Start(void)
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
				channel = std::make_shared<WebSocketClientChannel>();
			ConnectChannel(client, channel);
		}
	});
}

void WebSocketMultipleClient::ConnectChannel(std::shared_ptr<WebSocketMultipleClient> client, std::shared_ptr<WebSocketClientChannel> channel)
{
	channel->Connect(client->m_ServerURL, client->GetHttpHeader(), [client, channel](const boost::system::error_code& ec)
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

void WebSocketMultipleClient::Stop(void)
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

void WebSocketMultipleClient::NotifyChannelClose(std::shared_ptr<WebSocketChannel> channel, const boost::system::error_code& ecClose)
{
	ChannelDisconnected(channel, StringToWString(ecClose.message()));
}

void WebSocketMultipleClient::UpdateCanonsCount(size_t newCount)
{
	if (newCount >= 0xFFFF)
		newCount = 0xFFFF;
	m_Channels.resize(newCount);
}







WebSocketServer::WebSocketServer() :
	m_ListenAddress(L"127.0.0.1"),
	m_ListenPort(0),
	m_URL(L"/"),
	m_Acceptor(theApp.GetIOContext())
{

}

WebSocketServer::~WebSocketServer()
{
}

bool WebSocketServer::IsServer(void)
{
	return true;
}

bool WebSocketServer::IsSingleChannel(void)
{
	return false;
}

IDevice::PDTable WebSocketServer::EnumProperties()
{
	using StaticProperty = PropertyDescriptionHelper::StaticPropertyDescription;
	PDTable results;
	auto self = shared_from_this();
	auto pd = std::make_shared<StaticProperty>(
		L"ListenAddress",
		L"DEVICE.WEBSOCKETSERVER.LISTENADDRESS",
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
		L"DEVICE.WEBSOCKETSERVER.PROP.LISTENPORT",
		uint16_t(0),
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return std::to_wstring(self->m_ListenPort); },
		[self](const std::wstring& value) { self->m_ListenPort = static_cast<std::uint16_t>(std::wcstoul(value.c_str(), nullptr, 10)); }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"URL",
		L"DEVICE.WEBSOCKETSERVER.PROP.URL",
		L"/",
		IDevice::PropertyChangeFlags::CanChangeBeforeStart
		);
	pd->BindMethod(
		[self]() { return self->m_URL; },
		[self](const std::wstring& value) { self->m_URL = value; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Mode",
		L"DEVICE.WEBSOCKETSERVER.PROP.DATAMODE",
		std::wstring(L"TEXT"),
		IDevice::PropertyChangeFlags::CanChangeAlways
		);
	pd->AddEnumOptions(L"BINARY", L"DEVICE.WEBSOCKET.PROP.DATAMODE.EM0", true);
	pd->AddEnumOptions(L"TEXT", L"DEVICE.WEBSOCKET.PROP.DATAMODE.EM1", false);
	pd->BindMethod(
		[self]() { return self->m_bBinaryMode ? L"BINARY" : L"TEXT"; },
		[self](const std::wstring& value) { self->m_bBinaryMode = value == L"BINARY"; }
	);
	results.push_back(pd);

	pd = std::make_shared<StaticProperty>(
		L"Masked",
		L"DEVICE.WEBSOCKETSERVER.PROP.MASKED",
		bool(false),
		IDevice::PropertyChangeFlags::CanChangeAlways
		);

	pd->BindMethod(
		[self]() { return self->m_bMessageMasked ? L"1" : L"0"; },
		[self](const std::wstring& value) { self->m_bMessageMasked = value == L"1"; }
	);
	results.push_back(pd);

	return results;
}

void WebSocketServer::Start(void)
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

	m_Acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
	if (ec)
	{
		Close(ec);
		return;
	}

	StartAcceptClient();
	PropertyChanged();
	StatusChanged(DeviceStatus::Connected, StringToWString(ec.message()));
}

void WebSocketServer::Close(const boost::system::error_code& errorCode)
{
	boost::system::error_code ec;
	m_Acceptor.cancel(ec);
	m_Acceptor.close(ec);
	PropertyChanged();
	StatusChanged(DeviceStatus::Disconnected, StringToWString(errorCode.message()));
}

void WebSocketServer::StartAcceptClient(void)
{
	auto server = this->shared_from_this();
	auto channel = std::make_shared<WebSocketServerChannel>();
	channel->SetOwner(server);
	m_Acceptor.async_accept(channel->GetSocket(), [server, channel](const boost::system::error_code& ec)
	{
		if (!ec)
		{
			boost::system::error_code ecSet;
			channel->GetSocket().set_option(boost::asio::socket_base::keep_alive(true), ecSet);
			channel->Start(WStringToString(server->m_URL),[ec, server, channel]()
			{
				server->ChannelConnected(channel, StringToWString(ec.message()));
			});
			server->StartAcceptClient();
		}
	});
}

void WebSocketServer::Stop(void)
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

void WebSocketServer::NotifyChannelClose(std::shared_ptr<WebSocketChannel> channel, const boost::system::error_code& ecClose)
{
	ChannelDisconnected(channel, StringToWString(ecClose.message()));
}





WebSocketServerChannel::WebSocketServerChannel(void)
{

}

WebSocketServerChannel::~WebSocketServerChannel(void)
{

}

void WebSocketServerChannel::Start(std::string url, std::function<void()> cb)
{
	if (!TryOpen())
		return;
	auto headerBuf = std::make_shared<boost::asio::streambuf>(1024 * 64);
	auto channel = shared_from_this();
	m_URLPath = GetHttpQueryStringPath(url);
	boost::asio::async_read_until(
		channel->GetSocket(),
		*headerBuf,
		"\r\n\r\n",
		[channel, cb, headerBuf, this](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if (!ec)
		{
			std::istream input(headerBuf.get());
			std::string line;
			if (GetHeaderLine(input, line))
			{
				std::regex re("(\\w+)\\s+([^\\s]+)\\s+HTTP/([0-9]+\\.[0-9]+)", std::regex::icase | std::regex::ECMAScript);
				std::smatch sm;
				std::regex_search(line, sm, re);
				if (!sm.empty())
				{
					m_ConnectQueryString = sm[2].str();
					m_HttpVersion = sm[3].str();
					ParseHttpHeaders(input, m_RequestHeaders);
					ResponseWebsocketRequest(cb);
				}
			}
		}
	});
}

void WebSocketServerChannel::ResponseWebsocketRequest(std::function<void()> cb)
{
	auto path = GetHttpQueryStringPath(m_ConnectQueryString);
	int httpStatus = 101;
	if (!m_URLPath.empty())
	{
		if (!GetHttpPathCompare(m_URLPath , path))
			httpStatus = 404;
	}

	auto channel = shared_from_this();
	auto data = HttpConnectResponseString(httpStatus, m_HttpVersion, m_RequestHeaders, m_ResponseHeaders);
	boost::asio::async_write(
		channel->GetSocket(),
		boost::asio::const_buffer(data->data(), data->length()),
		[httpStatus, channel, cb, data, this](const boost::system::error_code& ec, size_t bytestransfer)
	{
		if ((!ec) && (httpStatus==101))
			cb();
	});
}

void WebSocketServerChannel::OnNotifyClose(const boost::system::error_code& ecClose)
{
	auto dev = m_Device.lock();
	if (dev != nullptr)
	{
		dev->NotifyChannelClose(shared_from_this(), ecClose);
	}
}

bool WebSocketServerChannel::IsBinaryMode(void)
{
	auto dev = m_Device.lock();
	if (dev == nullptr)
		return true;
	return dev->BinaryMode();
}

bool WebSocketServerChannel::IsMessageMasked(void)
{
	auto dev = m_Device.lock();
	if (dev == nullptr)
		return false;
	return dev->MessageMasked();
}

REGISTER_CLASS_TITLE(WebSocketSingleClient, L"DEVICE.CLASS.WEBSOCKETSINGLECLIENT.TITLE");
REGISTER_CLASS_TITLE(WebSocketMultipleClient, L"DEVICE.CLASS.WEBSOCKETMULTIPLECLIENT.TITLE");
REGISTER_CLASS_TITLE(WebSocketServer, L"DEVICE.CLASS.WEBSOCKETSERVER.TITLE");