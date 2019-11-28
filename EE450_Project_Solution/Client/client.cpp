#include <iostream>
#include <regex>

#include <sys/socket.h>
#include <netdb.h>

#include "common.hpp"

using std::cout;
using std::left;
using std::setw;
using std::endl;

//===============================================//
//                    Const                      //
//===============================================//

//===============================================//
//                     Tool                      //
//===============================================//
// parse command line arugments
ClientQuery Parse(int argc, char* argv[]) {
	if (argc != 4) {
		throw ArgumentException("Wrong number of argument");
	}
	auto name = string(argv[1]);
	if (!std::regex_match(name, std::regex("^[a-zA-z]$"))) {
		throw ArgumentException("Map ID should be exactly 1 alphabet");
	}
	auto nameId = name[0];
	Node_t source;
	try {
		source = std::stoll(argv[2]);
	} catch (...) {
		throw ArgumentException("Wrong source vertex id");
	}
	FileSize_t filesize;
	try {
		filesize = std::stoll(argv[3]);
	} catch (...) {
		throw ArgumentException("Wrong file size");
	}
	return ClientQuery(nameId, source, filesize);
}

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

	Response Process(const ClientQuery& query) {
		query.Encode(helper);
		cout << "The client has sent query to AWS using TCP: start vertex " << query.sourceNode << "; map " << query.mapName << "; file size " << query.fileSize << "." << endl;

		auto response = Response(helper);
		cout << "The client has received results from AWS:" << endl;
		response.Print();
		return response;
	}
};

int main(int argc, char* argv[]) {
	try {
		auto query = Parse(argc, argv);
		auto conn = Connection();
		auto response = conn.Process(query);
	} catch (const std::exception & ex) {
		std::cerr << ex.what() << endl;
	}
	return 0;
}