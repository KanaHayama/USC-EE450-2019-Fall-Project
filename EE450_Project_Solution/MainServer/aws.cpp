#include <iostream>
#include <memory>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "common.hpp"

using std::cout;
using std::endl;

//===============================================//
//                   typedef                     //
//===============================================//

typedef AllShortestPath (*ServerA)(const ClientQuery& query);
typedef AllDelay(*ServerB)(const FileSize_t& fileSize, const AllShortestPath& allShortestPath);

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
	TcpServerSocketBuilder builder;
	UdpReceiveSocketHelper udpReceiveHelper;
	
public:
	Connection() : builder(TcpServerSocketBuilder(SERVER_AWS_TCP_PORT)), udpReceiveHelper(UdpReceiveSocketHelper(SERVER_AWS_UDP_PORT)){
		cout << "The AWS is up and running." << endl;
	}

	void Process() {
		while (auto child = builder.Accept()) {
			//receive from client
			auto query = ClientQuery(*child);
			cout << "The AWS has received map ID " << query.mapName << ", start vertex " << query.sourceNode << " and file size " << query.fileSize << " from the client using TCP over port " << SERVER_AWS_TCP_PORT << endl;

			//forward to A
			auto sendA = udpReceiveHelper.SendHelper(HOST, SERVER_A_PORT);
			query.Encode(*sendA);
			cout << "The AWS has sent map ID and starting vertex to server A using UDP over port " << SERVER_AWS_UDP_PORT << "." << endl;
			auto shortestPath = AllShortestPath(udpReceiveHelper);
			cout << "The AWS has received shortest path from server A:" << endl;
			shortestPath.Print();

			//forward to B
			auto sendB = udpReceiveHelper.SendHelper(HOST, SERVER_B_PORT);
			query.Encode(*sendB);
			shortestPath.Encode(*sendB);
			cout << "The AWS has sent path length, propagation speed and transmission speed to server B using UDP over port " << SERVER_AWS_UDP_PORT <<"." << endl;
			auto delay = AllDelay(udpReceiveHelper);
			cout << "The AWS has received delays from server B:" << endl;
			delay.Print();

			//send to client
			delay.Encode(*child);
			cout << "The AWS has sent calculated delay to client using TCP over port " << SERVER_AWS_TCP_PORT << "." << endl;
		}
	}
};

int main() {
	try {
		auto client = Connection();
		client.Process();
	} catch (const std::exception & ex) {
		std::cerr << ex.what() << endl;
	}
	return 0;
}