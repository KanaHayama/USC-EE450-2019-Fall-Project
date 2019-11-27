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

class EE450Exception : std::exception {};

class InvalidInputException : EE450Exception {};

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
			exit(1);
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
			exit(1);
		}
		freeaddrinfo(serverInfo);
	}
};

class TcpServerSocketBuilder;

class TcpServerSocketHelper : public TcpSocketHelper {
private:
	sockaddr_storage their_addr = {};
	socklen_t addr_size = sizeof(their_addr);
	friend class TcpServerSocketBuilder;
protected:
	TcpServerSocketHelper() {};

public:

};

class TcpServerSocketBuilder {
private:
	int tcpSocket = -1;

public:
	TcpServerSocketBuilder(const char* _selfPort) {
		assert(_selfPort != nullptr);
		addrinfo hints = {};
		addrinfo* serverInfo;
		addrinfo* p;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		if (getaddrinfo(nullptr, _selfPort, &hints, &serverInfo) != 0) {
			exit(1);
		}
		for (p = serverInfo; p != nullptr; p = p->ai_next) {
			if ((tcpSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				continue;
			}
			if (bind(tcpSocket, p->ai_addr, p->ai_addrlen) != 0) {
				close(tcpSocket);
				continue;
			}
			if (listen(tcpSocket, CONNECTION_LIMIT) != 0) {
				close(tcpSocket);
				continue;
			}
			break;
		}
		if (p == nullptr) {
			exit(1);
		}
		freeaddrinfo(serverInfo);
	}

	~TcpServerSocketBuilder() {
		if (tcpSocket >= 0) {
			close(tcpSocket);
		}
	}

	std::unique_ptr<TcpServerSocketHelper> Accept() const {
		auto result = std::unique_ptr<TcpServerSocketHelper>(new TcpServerSocketHelper());
		result->tcpSocket = accept(tcpSocket, (sockaddr*)&(result->their_addr), &result->addr_size);
		return result;
	}
};

class UdpReceiveSocketHelper : public SocketHelper {
private:
	char readBuffer[BUFFER_SIZE];
	int readBufferLen = 0;
	int readIndex = 0;

	int udpSocket = 0;

	addrinfo* serverInfo = nullptr;
	addrinfo* p;

	void ReadDatagram(const int udpSocket, char* buffer, const int size) {
		assert(readIndex <= readBufferLen);
		if (readIndex == readBufferLen) {
			readBufferLen = recvfrom(udpSocket, readBuffer, BUFFER_SIZE, 0, p->ai_addr, &p->ai_addrlen);
			readIndex = 0;
		}
		assert(readIndex + size <= readBufferLen);
		memcpy(buffer, readBuffer + readIndex, size);
		readIndex += size;
	}
public:
	UdpReceiveSocketHelper(const char* _selfPort) {
		assert(_selfPort != nullptr);
		addrinfo hints = {};
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_flags = AI_PASSIVE;
		if (getaddrinfo(nullptr, _selfPort, &hints, &serverInfo) != 0) {
			exit(1);
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
			exit(1);
		}
	}

	~UdpReceiveSocketHelper() {
		freeaddrinfo(serverInfo);
	}

	virtual void Read(char* buffer, const int size) {
		ReadDatagram(udpSocket, buffer, size);
	}

	virtual void Write(const char* buffer, const int size) {
		assert(false);
	}

	virtual void Flush() {
		assert(false);
	}
};

class UdpSendSocketHelper : public SocketHelper {
private:
	char writeBuffer[BUFFER_SIZE];
	int writeBufferLen = 0;

	const char* remoteHost;
	const char* remotePort;

	void WriteDatagram(const char* buffer, const int size) {
		assert(writeBufferLen + size <= BUFFER_SIZE);
		memcpy(writeBuffer + writeBufferLen, buffer, size);
		writeBufferLen += size;
	}
public:
	UdpSendSocketHelper(const char* _remoteHost, const char* _remotePort) : remoteHost(_remoteHost), remotePort(_remotePort) {}

	virtual void Read(char* buffer, const int size) {
		assert(false);
	}

	virtual void Write(const char* buffer, const int size) {
		WriteDatagram(buffer, size);
	}

	virtual void Flush() {
		assert(remoteHost != nullptr);
		addrinfo hints = {};
		addrinfo* serverInfo;
		addrinfo* p;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		if (getaddrinfo(remoteHost, remotePort, &hints, &serverInfo) != 0) {
			exit(1);
		}
		int udpSocket;
		for (p = serverInfo; p != nullptr; p = p->ai_next) {
			if ((udpSocket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
				continue;
			}
			break;
		}
		if (p == nullptr) {
			exit(1);
		}
		auto sendLen = sendto(udpSocket, writeBuffer, writeBufferLen, 0, p->ai_addr, p->ai_addrlen);
		assert(sendLen == writeBufferLen);
		close(udpSocket);
		freeaddrinfo(serverInfo);

		writeBufferLen = 0;
	}
};

class Serializable {
public:
	virtual void Encode(SocketHelper& socket) const = 0;

};

struct MapInfo {

	char name;
	PropagationSpeed_t propagationSpeed;
	TransmissionSpeed_t transmissionSpeed;

	MapInfo() {}
	MapInfo(const char _name, const PropagationSpeed_t& _propagationSpeed, const TransmissionSpeed_t& _transmissionSpeed) : name(_name), propagationSpeed(_propagationSpeed), transmissionSpeed(_transmissionSpeed) {}
};

struct AllShortestPath : public Serializable {
	MapInfo mapInfo;
	Node_t sourceNode;

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
		cout << "-----------------------------" << endl;
		cout << setw(colWidth[0]) << "Destination" << setw(colWidth[1]) << "Min Length" << endl;
		cout << "-----------------------------" << endl;
		for (const auto& p : distances) {
			cout << setw(colWidth[0]) << p.first << setw(colWidth[1]) << p.second << endl;
		}
		cout << "-----------------------------" << endl;
	}
};

struct Delay {
	Delay_t transmission = 0;
	Delay_t propagation = 0;

	Delay() {};
	Delay(const FileSize_t& fileSize, const MapInfo& mapInfo, const Distance_t& distance) : transmission(mapInfo.transmissionSpeed * fileSize / BYTE_SIZE), propagation(mapInfo.propagationSpeed * distance) {}

	Delay_t Total() const {
		return transmission + propagation;
	}
};

struct AllDelay : public Serializable {

	map<Node_t, Delay> delays;

	AllDelay(const FileSize_t& fileSize, const AllShortestPath& allShortestPath) {
		for (const auto& record : allShortestPath.distances) {
			AddDelay(record.first, Delay(fileSize, allShortestPath.mapInfo, record.second));
		}
	}

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

	void AddDelay(const Node_t& dest, const Delay& delay) {
		delays[dest] = delay;
	}

	void Print() const {
		const int colWidth[] = { 13, 20, 20, 20};
		cout << left << std::fixed << std::showpoint << std::setprecision(FLOAT_PRECISION);
		cout << "-------------------------------------------------------" << endl;
		cout << setw(colWidth[0]) << "Destination" << setw(colWidth[1]) << "Tt" << setw(colWidth[2]) << "Tp" << setw(colWidth[3]) << "Delay" << endl;
		cout << "-------------------------------------------------------" << endl;
		for (const auto& d : delays) {
			cout << setw(colWidth[0]) << d.first << setw(colWidth[1]) << d.second.transmission << setw(colWidth[2]) << d.second.propagation << setw(colWidth[3]) << d.second.Total() << endl;
		}
		cout << "-------------------------------------------------------" << endl;
	}
};

struct ClientQuery : public Serializable {
	char mapName;
	Node_t sourceNode;
	FileSize_t fileSize;

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