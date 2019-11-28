#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "common.hpp"

using std::cout;
using std::endl;

//===============================================//
//                    Const                      //
//===============================================//

//===============================================//
//                     Tool                      //
//===============================================//

//===============================================//
//                    Class                      //
//===============================================//

// delay calculation logic
struct DefaultDelay : public AllDelay {
private:
	static Delay_t CalcTransmissionDelay(const FileSize_t& fileSize, const MapInfo& mapInfo, const Distance_t& distance) {
		return (double)fileSize / BYTE_SIZE / mapInfo.transmissionSpeed;
	}

	static Delay_t CalcPropagationDelay(const MapInfo& mapInfo, const Distance_t& distance) {
		return distance / mapInfo.propagationSpeed;
	}
public:
	DefaultDelay(const FileSize_t& fileSize, const AllShortestPath& allShortestPath) {
		for (const auto& record : allShortestPath.distances) {
			auto t = CalcTransmissionDelay(fileSize, allShortestPath.mapInfo, record.second);
			auto p = CalcPropagationDelay(allShortestPath.mapInfo, record.second);
			AddDelay(record.first, Delay(t, p));
		}
	}
};

class Connection {
private:
	UdpReceiveSocketHelper receiveHelper;
public:
	Connection() : receiveHelper(UdpReceiveSocketHelper(SERVER_B_PORT)) {
		std::cout << "The Server B is up and running using UDP on port " << SERVER_B_PORT << "." << std::endl;
	}

	void Process() {
		while (true) {
			auto query = ClientQuery(receiveHelper);
			auto shortestPath = AllShortestPath(receiveHelper);
			cout << std::left << std::fixed << std::setprecision(FLOAT_PRECISION);
			cout << "The Server B has received data for calculation:" << endl;
			cout << "* Propagation speed: " << shortestPath.mapInfo.propagationSpeed << " km/s;" << endl;
			cout << "* Transmission speed " << shortestPath.mapInfo.transmissionSpeed << " Bytes/s;" << endl;
			for (const auto& path : shortestPath.distances) {
				cout << "* Path length for destination " << path.first << ": " << path.second << ";" << endl;
			}

			auto delay = DefaultDelay(query.fileSize, shortestPath);
			cout << "The Server B has finished the calculation of the delays:" << endl;
			delay.Print();

			auto sendHelper = receiveHelper.SendHelper(HOST, SERVER_AWS_UDP_PORT);
			delay.Encode(*sendHelper);
			cout << "The Server B has finished sending the output to AWS" << endl;
		}
	}
};

int main() {
	try {
		auto conn = Connection();
		conn.Process();
	} catch (const std::exception & ex) {
		std::cerr << ex.what() << endl;
	}
	return 0;
}