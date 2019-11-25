#include <cassert>
#include <iostream>

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
	TcpClientSocketHelper helper;

public:
	Connection(): helper(TcpClientSocketHelper(HOST, SERVER_AWS_TCP_PORT)) {
		cout << "The client is up and running." << endl;
	}

	AllDelay Process(const ClientQuery& query) {
		query.Encode(helper);
		cout << "The client has sent query to AWS using TCP over port <port number>: start vertex " << query.sourceNode << "; map " << query.mapName << "; file size " << query.fileSize << "." << endl;

		auto delay = AllDelay(helper);
		cout << "The client has received results from AWS:" << endl;
		delay.Print();
		return delay;
	}
};

int main(int argc, char* argv[]) {
	assert(argc == 4);
	auto name = string(argv[1]);
	assert(name.size() == 1);
	auto query = ClientQuery(name[0], std::stoi(argv[2]), std::stoll(argv[3]));

	auto conn = Connection();
	auto delay = conn.Process(query);
	return 0;
}