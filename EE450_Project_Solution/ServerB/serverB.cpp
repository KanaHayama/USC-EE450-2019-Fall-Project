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
				
			}

			auto delay = AllDelay(query.fileSize, shortestPath);
			cout << "The Server B has finished the calculation of the delays:" << endl;
			delay.Print();

			auto sendHelper = UdpSendSocketHelper(HOST, SERVER_AWS_UDP_PORT);
			delay.Encode(sendHelper);
			cout << "The Server B has finished sending the output to AWS" << endl;
		}
	}
};

int main() {
	auto conn = Connection();

	conn.Process();
	return 0;
}