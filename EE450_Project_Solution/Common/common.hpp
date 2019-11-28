#pragma once

#include <map>
#include <vector>
#include <set>
#include <list>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cassert>
#include <memory>
#include <tuple>
#include <cfenv>

#include <sys/socket.h>
#include <unistd.h>

using std::map;
using std::string;
using std::vector;
using std::set;
using std::list;
using std::cout;
using std::left;
using std::setw;
using std::endl;

//===============================================//
//                   typedef                     //
//===============================================//
typedef long long Node_t;
typedef long long Distance_t; // km
typedef double PropagationSpeed_t; // km/sec
typedef double TransmissionSpeed_t; // byte/sec
typedef long long FileSize_t; // bit
typedef double Delay_t; // sec

//===============================================//
//                    Const                      //
//===============================================//
const int BYTE_SIZE = 8;
const int FLOAT_PRECISION = 2;
const int BUFFER_SIZE = 32768;
const int CONNECTION_LIMIT = 1;
const char* HOST = "127.0.0.1";
const char* SERVER_A_PORT = "21943";
const char* SERVER_B_PORT = "22943";
const char* SERVER_AWS_UDP_PORT = "23943";
const char* SERVER_AWS_TCP_PORT = "24943";

//===============================================//
//                     Tool                      //
//===============================================//

//===============================================//
//                    Class                      //
//===============================================//

//===================Exception====================

class EE450Exception : public std::runtime_error {
protected:
	explicit EE450Exception(const string& message) : std::runtime_error(message) {}
};

class UnsupportedOperationException : public EE450Exception {
public:
	explicit UnsupportedOperationException() : EE450Exception("Unsupported operation") {}
};

class ArgumentException : public EE450Exception {
public:
	explicit ArgumentException(const string& message) : EE450Exception(message) {}
};

class SocketException : public EE450Exception {
protected:
	explicit SocketException(const string& message) : EE450Exception(message) {}
};

class ResolveException : public SocketException {
public:
	explicit ResolveException(const char* host, const char* port) : SocketException("Cannot resolve address " + (host == nullptr ? string(HOST) : host) + ":" + port) {}
};

class MultipleRemoteHostException : public SocketException {
public:
	explicit MultipleRemoteHostException(const char* host, const char* port) : SocketException("Multiple remote address " + (host == nullptr ? string(HOST) : host) + ":" + port + " founded") {}
};

class ServerSetupException : public SocketException {
public:
	explicit ServerSetupException(const char* port, const bool udp) : SocketException("Cannot bind to / listen at port " + string(port) + ", please run command \"fuser -k " + string(port) + "/" + (udp ? "udp" : "tcp") + "\" then retry") {}
};

class ConnectException : public SocketException {
public:
	explicit ConnectException(const char* host, const char* port) : SocketException("Cannot connect to " + (host == nullptr ? string(HOST) : host) + ":" + port) {}
};

class TooLargePayloadException : public EE450Exception {
public:
	explicit TooLargePayloadException() : EE450Exception("The size of payload is larger than the max supported size " + std::to_string(BUFFER_SIZE)) {}
};

class PayloadSizeMismatchException : public EE450Exception {
public:
	explicit PayloadSizeMismatchException() : EE450Exception("Required payload size is larger than the received size") {}
};

class SendLengthMismatchException : public EE450Exception {
public:
	explicit SendLengthMismatchException() : EE450Exception("Payload is not sended entirelly") {}
};

class ResultMappingError : public EE450Exception {
public:
	explicit ResultMappingError() : EE450Exception("Shortest path result and delay result mismatch") {}
};

//===================Socket Wrapper====================

// encoder/decoder abstraction
class SocketHelper {
protected:
	virtual void Read(char* buffer, const int size) = 0;
	virtual void Write(const char* buffer, const int size) = 0;
public:
	
	template <typename T>
	void Read(const T& buffer) {
		Read((char*)&buffer, sizeof(buffer));
	}

	template <typename T>
	void Write(const T& buffer) {
		Write((char*)&buffer, sizeof(buffer));
	}

	virtual void Flush() = 0;
};

class TcpSocketHelper : public SocketHelper {
private:
	char* receiveBuffer[BUFFER_SIZE];
	int receiveBufferLen = 0;
	int receiveBufferIndex = 0;
	
protected:
	int tcpSocket = -1;

	static void ReadStream(const int tcpSocket, char* buffer, const int size) {
		auto total = 0;
		while (total < size) {
			auto receivedLen = recv(tcpSocket, buffer + total, size - total, 0);
			assert(receivedLen >= 0);
			total += receivedLen;
		}
		assert(total == size);
	}

	static void WriteStream(const int tcpSocket, const char* buffer, const int size) {
		auto total = 0;
		while (total != size) {
			auto sendLen = send(tcpSocket, buffer + total, size - total, 0);
			total += sendLen;
		}
	}
public:
	~TcpSocketHelper() {
		if (tcpSocket >= 0) {
			close(tcpSocket);
		}
	}

	virtual void Read(char* buffer, const int size) {
		ReadStream(tcpSocket, buffer, size);
	}

	virtual void Write(const char* buffer, const int size) {
		WriteStream(tcpSocket, buffer, size);
	}

	virtual void Flush() { }
};

class TcpClientSocketHelper : public TcpSocketHelper {
private:
	
public:
	TcpClientSocketHelper(const char* _remoteHost, const char* _remotePort) {
		assert(_remoteHost != nullptr && _remotePort != nullptr);
		addrinfo hints = {};
		addrinfo* serverInfo;
		addrinfo* p;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		if (getaddrinfo(_remoteHost, _remotePort, &hints, &serverInfo) != 0) {
			throw ResolveException(_remoteHost, _remotePort);
		}
		for (p = serverInfo; p != nullptr; p = p->ai_next) {
			if ((tcpSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				continue;
			}
			if (connect(tcpSocket, p->ai_addr, p->ai_addrlen) != 0) {
				//close(tcpSocket);
				continue;
			}
			break;
		}
		if (p == nullptr) {
			throw ConnectException(_remoteHost, _remotePort);
		}
		freeaddrinfo(serverInfo);
	}
};

class TcpServerSocketBuilder;

// wrapper of TCP child socket
class TcpServerSocketHelper : public TcpSocketHelper {
private:
	sockaddr_storage their_addr = {};
	socklen_t addr_size = sizeof(their_addr);
	friend class TcpServerSocketBuilder;
protected:
	TcpServerSocketHelper() {};

public:

};

// wrapper of TCP parent socket
class TcpServerSocketBuilder {
private:
	int tcpSocket = -1;

public:
	TcpServerSocketBuilder(const char* _selfPort) {
		if (_selfPort == nullptr) {
			throw ArgumentException("Self port number is null");
		}
		addrinfo hints = {};
		addrinfo* serverInfo;
		addrinfo* p;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		if (getaddrinfo(nullptr, _selfPort, &hints, &serverInfo) != 0) {
			throw ResolveException(nullptr, _selfPort);
		}
		for (p = serverInfo; p != nullptr; p = p->ai_next) {
			if ((tcpSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				continue;
			}
			if (bind(tcpSocket, p->ai_addr, p->ai_addrlen) != 0) {
				close(tcpSocket);
				tcpSocket = -1;
				continue;
			}
			if (listen(tcpSocket, CONNECTION_LIMIT) != 0) {
				close(tcpSocket);
				continue;
			}
			break;
		}
		if (p == nullptr) {
			throw ServerSetupException(_selfPort, false);
		}
		freeaddrinfo(serverInfo);
	}

	~TcpServerSocketBuilder() {
		if (tcpSocket >= 0) {
			close(tcpSocket);
		}
	}

	// accept tcp connection
	std::unique_ptr<TcpServerSocketHelper> Accept() const {
		auto result = std::unique_ptr<TcpServerSocketHelper>(new TcpServerSocketHelper());
		result->tcpSocket = accept(tcpSocket, (sockaddr*)&(result->their_addr), &result->addr_size);
		return result;
	}
};

class UdpSocketHelper : public SocketHelper {
protected:
	char buffer[BUFFER_SIZE];
	int bufferLength = 0;
	int udpSocket = -1;
};

class UdpReceiveSocketHelper;

// wrapper of udp sender, can only be created by binded udp receiver
class UdpSendSocketHelper : public UdpSocketHelper {
private:
	const char* remoteHost;
	const char* remotePort;

	friend class UdpReceiveSocketHelper;

	// write local buffer
	void WriteDatagram(const char* buffer, const int size) {
		if (bufferLength + size > BUFFER_SIZE) {
			throw TooLargePayloadException();
		}
		memcpy(this->buffer + bufferLength, buffer, size);
		bufferLength += size;
	}

	// private constructor, can only called by UdpReceiveSocketHelper
	UdpSendSocketHelper(const int _udpSocket, const char* _remoteHost, const char* _remotePort) : remoteHost(_remoteHost), remotePort(_remotePort) {
		udpSocket = _udpSocket;

		if (remoteHost == nullptr || remotePort == nullptr) {
			throw ArgumentException("Remote host is null");
		}
		if (udpSocket < 0) {
			throw ArgumentException("Illegal socket handler");
		}
	}
public:
	virtual void Read(char* buffer, const int size) {
		throw UnsupportedOperationException();
	}

	virtual void Write(const char* buffer, const int size) {
		WriteDatagram(buffer, size);
	}

	virtual void Flush() {
		addrinfo hints = {};
		addrinfo* serverInfo = nullptr;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		if (getaddrinfo(remoteHost, remotePort, &hints, &serverInfo) != 0) {
			throw ResolveException(remoteHost, remotePort);
		}
		assert(serverInfo != nullptr);
		if (serverInfo->ai_next != nullptr) {
			throw MultipleRemoteHostException(remoteHost, remotePort);
		}
		auto sendLen = sendto(udpSocket, buffer, bufferLength, 0, serverInfo->ai_addr, serverInfo->ai_addrlen);
		if (sendLen != bufferLength) {
			throw SendLengthMismatchException();
		}
		freeaddrinfo(serverInfo);

		bufferLength = 0;
	}
};

// wrapper of binded UDP receiver
class UdpReceiveSocketHelper : public UdpSocketHelper {
private:
	int readIndex = 0;

	addrinfo* serverInfo = nullptr;
	addrinfo* p;

	void ReadDatagram(const int udpSocket, char* buffer, const int size) {
		assert(readIndex <= bufferLength);
		if (readIndex == bufferLength) { // buffered data run out, receive new data
			bufferLength = recvfrom(udpSocket, this->buffer, BUFFER_SIZE, 0, p->ai_addr, &p->ai_addrlen);
			readIndex = 0;
		}
		// consume buffered data
		if (readIndex + size > bufferLength) {
			throw PayloadSizeMismatchException();
		}
		memcpy(buffer, this->buffer + readIndex, size);
		readIndex += size;
	}
public:
	UdpReceiveSocketHelper(const char* _selfPort) {
		if (_selfPort == nullptr) {
			throw ArgumentException("Self port number is null");
		}
		addrinfo hints = {};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_PASSIVE;
		if (getaddrinfo(nullptr, _selfPort, &hints, &serverInfo) != 0) {
			throw ResolveException(nullptr, _selfPort);
		}
		for (p = serverInfo; p != nullptr; p = p->ai_next) {
			if ((udpSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				continue;
			}
			if (bind(udpSocket, p->ai_addr, p->ai_addrlen) == -1) {
				close(udpSocket);
				continue;
			}
			break;
		}
		if (p == nullptr) {
			throw ServerSetupException(_selfPort, true);
		}
	}

	~UdpReceiveSocketHelper() {
		freeaddrinfo(serverInfo);
	}

	// create a send helper using the same socket
	std::unique_ptr<UdpSendSocketHelper> SendHelper(const char* _remoteHost, const char* _remotePort) {
		return std::unique_ptr<UdpSendSocketHelper>(new UdpSendSocketHelper(udpSocket, _remoteHost, _remotePort));
	}

	virtual void Read(char* buffer, const int size) {
		ReadDatagram(udpSocket, buffer, size);
	}

	virtual void Write(const char* buffer, const int size) {
		throw UnsupportedOperationException();
	}

	virtual void Flush() {
		throw UnsupportedOperationException();
	}
};

//===================Container====================

class Serializable {
public:
	virtual void Encode(SocketHelper& socket) const = 0;

};

struct MapInfo {
	char name; // this field is unnecessary, but I want to keep it.
	PropagationSpeed_t propagationSpeed;
	TransmissionSpeed_t transmissionSpeed;

	MapInfo() {}
	MapInfo(const char _name, const PropagationSpeed_t& _propagationSpeed, const TransmissionSpeed_t& _transmissionSpeed) : name(_name), propagationSpeed(_propagationSpeed), transmissionSpeed(_transmissionSpeed) {}
};

// Response struct for server A response to main server and further be forward to server B
struct AllShortestPath : public Serializable {
	MapInfo mapInfo;
	Node_t sourceNode; // this field is unnecessary, but I want to keep it.

	map<Node_t, Distance_t> distances;

	AllShortestPath(const MapInfo& _mapInfo, const Node_t& _sourceNode) : mapInfo(_mapInfo), sourceNode(_sourceNode) {}

	AllShortestPath(SocketHelper& socket) {
		socket.Read(mapInfo);
		socket.Read(sourceNode);
		int size;
		socket.Read(size);
		for (auto i = 0; i < size; i++) {
			Node_t n;
			socket.Read(n);
			Distance_t d;
			socket.Read(d);
			AddDistance(n, d);
		}
	}

	virtual void Encode(SocketHelper& socket) const {
		socket.Write(mapInfo);
		socket.Write(sourceNode);
		int size = distances.size();
		socket.Write(size);
		for (const auto& p : distances) {
			socket.Write(p.first);
			socket.Write(p.second);
		}
		socket.Flush();
	}

	void AddDistance(const Node_t& dest, const Distance_t& distance) {
		distances[dest] = distance;
	}

	void Print() const {
		const int colWidth[] = { 13, 12};
		cout << left;
		cout << "-------------------------------------" << endl;
		cout << setw(colWidth[0]) << "Destination" << setw(colWidth[1]) << "Min Length" << endl;
		cout << "-------------------------------------" << endl;
		for (const auto& p : distances) {
			cout << setw(colWidth[0]) << p.first << setw(colWidth[1]) << p.second << endl;
		}
		cout << "-------------------------------------" << endl;
	}
};

struct Delay {
	Delay_t transmission = 0;
	Delay_t propagation = 0;

	Delay() {};
	Delay(const Delay_t& _transmission, const Delay_t& _propagation) : transmission(_transmission), propagation(_propagation) {}

	// end-to-end delay is returned in realtime
	Delay_t Total() const {
		return transmission + propagation;
	}
};

// Response struct for server B response to main server
struct AllDelay : public Serializable {
protected:
	AllDelay() {}

public:
	map<Node_t, Delay> delays;// since all transmission delays are identical, transmission delys can be further reduced to 1 copy.

	AllDelay(SocketHelper& socket) {
		int size;
		socket.Read(size);
		for (auto i = 0; i < size; i++) {
			Node_t n;
			socket.Read(n);
			Delay d;
			socket.Read(d);
			AddDelay(n, d);
		}
	}

	virtual void Encode(SocketHelper& socket) const {
		int size = delays.size();
		socket.Write(size);
		for (const auto& d : delays) {
			socket.Write(d.first);
			socket.Write(d.second);
		}
		socket.Flush();
	}

	void Print() const {
		const int colWidth[] = { 13, 20, 20, 20};
		std::fesetround(FE_TONEAREST);
		cout << left << std::fixed << std::showpoint << std::setprecision(FLOAT_PRECISION);
		cout << "---------------------------------------------------------------------" << endl;
		cout << setw(colWidth[0]) << "Destination" << setw(colWidth[1]) << "Tt" << setw(colWidth[2]) << "Tp" << setw(colWidth[3]) << "Delay" << endl;
		cout << "---------------------------------------------------------------------" << endl;
		for (const auto& d : delays) {
			cout << setw(colWidth[0]) << d.first << setw(colWidth[1]) << d.second.transmission << setw(colWidth[2]) << d.second.propagation << setw(colWidth[3]) << d.second.Total() << endl;
		}
		cout << "---------------------------------------------------------------------" << endl;
	}

protected:
	void AddDelay(const Node_t& dest, const Delay& delay) {
		delays[dest] = delay;
	}
};

// Query struct for client query main server and further be forward to server A & B
struct ClientQuery : public Serializable {
	char mapName; // this field is unnecessary for server B, but I will not define a new class for simplicity.
	Node_t sourceNode; // this field is unnecessary for server B, but I will not define a new class for simplicity.
	FileSize_t fileSize; // this field is unnecessary for server A, but I will not define a new class for simplicity.

	ClientQuery(const char _mapName, const Node_t& _sourceNode, const FileSize_t& _fileSize) : mapName(_mapName), sourceNode(_sourceNode), fileSize(_fileSize) {}

	ClientQuery(SocketHelper& socket) {
		socket.Read(mapName);
		socket.Read(sourceNode);
		socket.Read(fileSize);
	}

	virtual void Encode(SocketHelper& socket) const {
		socket.Write(mapName);
		socket.Write(sourceNode);
		socket.Write(fileSize);
		socket.Flush();
	}
};

// Response struct for main server response to clinet
struct Response : public Serializable {
private:
	void Add(const std::tuple<Node_t, Distance_t, Delay>& value) {
		values.push_back(value);
	}

public:
	std::vector<std::tuple<Node_t, Distance_t, Delay>> values; // since all transmission delays are identical, they can be further reduced to 1 copy.

	Response(const AllShortestPath& allShortestPath, const AllDelay& allDelay) {
		if (allShortestPath.distances.size() != allDelay.delays.size()) {
			throw ResultMappingError();
		}
		for (const auto& d : allShortestPath.distances) {
			auto it = allDelay.delays.find(d.first);
			if (it == allDelay.delays.end()) {
				throw ResultMappingError();
			}
			Add(std::make_tuple(d.first, d.second, it->second));
		}
	}

	Response(SocketHelper& socket) {
		int size;
		socket.Read(size);
		for (auto i = 0; i < size; i++) {
			std::tuple<Node_t, Distance_t, Delay> t;
			socket.Read(t);
			Add(t);
		}
	}

	virtual void Encode(SocketHelper& socket) const {
		int size = values.size();
		socket.Write(size);
		for (const auto& v : values) {
			socket.Write(v);
		}
		socket.Flush();
	}

	void Print() const {
		const int colWidth[] = { 13, 13, 20, 20, 20 };
		std::fesetround(FE_TONEAREST);
		cout << left << std::fixed << std::showpoint << std::setprecision(FLOAT_PRECISION);
		cout << "--------------------------------------------------------------------------------" << endl;
		cout << setw(colWidth[0]) << "Destination" << setw(colWidth[1]) << "Min Length" << setw(colWidth[2]) << "Tt" << setw(colWidth[3]) << "Tp" << setw(colWidth[4]) << "Delay" << endl;
		cout << "--------------------------------------------------------------------------------" << endl;
		for (const auto& t : values) {
			cout << setw(colWidth[0]) << std::get<0>(t) << setw(colWidth[1]) << std::get<1>(t) << setw(colWidth[2]) << std::get<2>(t).transmission << setw(colWidth[3]) << std::get<2>(t).propagation << setw(colWidth[4]) << std::get<2>(t).Total() << endl;
		}
		cout << "--------------------------------------------------------------------------------" << endl;
	}
};