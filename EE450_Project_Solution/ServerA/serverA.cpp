#include <unordered_map>
#include <map>
#include <unordered_set>
#include <set>
#include <fstream>
#include <regex>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <cassert>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "common.hpp"

using std::unordered_map;
using std::map;
using std::unordered_set;
using std::set;
using std::cout;
using std::left;
using std::setw;
using std::endl;

//===============================================//
//                    Const                      //
//===============================================//

const string MAP_FILENAME = "map.txt";

//===============================================//
//                     Tool                      //
//===============================================//

vector<string> Split(const string& str, const string& regexStr) {
	auto regex = std::regex(regexStr);
	auto begin = std::sregex_token_iterator(str.begin(), str.end(), regex, -1);
	auto end = std::sregex_token_iterator();
	return { begin, end };
}

//===============================================//
//                    Class                      //
//===============================================//

class Map {
private:
	MapInfo mapInfo;
	unordered_map<Node_t, map<Node_t, Distance_t>> value;

	void AddDirectedEdge(const Node_t& src, const Node_t& dest, const Distance_t distance) {
		auto ret = value.emplace(src, map<Node_t, Distance_t>());
		auto& edges = ret.first->second;
		edges[dest] = distance;
	}
public:
	Map() {}
	Map(const MapInfo& _mapInfo) : mapInfo(_mapInfo) {}

	void AddUndirectedEdge(const Node_t& src, const Node_t& dest, const Distance_t distance) {
		AddDirectedEdge(src, dest, distance);
		AddDirectedEdge(dest, src, distance);
	}

	int VertexCount() const {
		return value.size();
	}

	int UndirectedEdgeCount() const {
		auto result = 0;
		for (const auto& edgeSet : value) {
			result += edgeSet.second.size();
		}
		return result / 2;
	}

	AllShortestPath CalcShortestPath(const Node_t& src) const {
		auto result = AllShortestPath(mapInfo, src);
		//Dijkstra
		auto included = unordered_set<Node_t>();
		auto& distance = result.distances;
		// init
		distance[src] = 0;
		// calc
		for (auto i = 0; i < VertexCount(); i++) {
			// find nearest
			auto newNode = Node_t();
			auto minDist = std::numeric_limits<Distance_t>::max();
			for (const auto& edgeSet : value) {
				auto& testNode = edgeSet.first;
				if (included.find(testNode) == included.end()) {// only not included nodes
					const auto& distElem = distance.find(testNode);
					const auto& testDist = distElem != distance.end() ? distElem->second : std::numeric_limits<Distance_t>::max();
					if (testDist < minDist) {// record nearest
						minDist = testDist;
						newNode = testNode;
					}
				}
			}
			// update
			included.insert(newNode);
			for (const auto& edge : value.at(newNode)) {
				const auto& updateNode = edge.first;
				const auto& distElem = distance.find(updateNode);
				const auto& currentDist = distElem != distance.end() ? distElem->second : std::numeric_limits<Distance_t>::max();
				auto newDist = minDist + edge.second;
				if (newDist < currentDist) {
					distance[updateNode] = newDist;
				}
			}
		}
		distance.erase(src);
		return result;
	}
};

enum class ReadLineState {
	Normal,
	PropagationSpeed,
	TransmissionSpeed,
};

class MapManager {
private:
	map<char, Map> maps;

	void BuildFromFile(const string& filename) {
		maps = map<char, Map>();
		auto file = std::ifstream(filename);
		auto state = ReadLineState::Normal;
		string name;
		PropagationSpeed_t pSpeed;
		TransmissionSpeed_t tSpeed;
		MapInfo info;
		Node_t src;
		Node_t dest;
		Distance_t dist;
		Map map;
		for (string line; getline(file, line);) {
			auto tokens = Split(line, R"(\s+)");
			switch (state) {
			case ReadLineState::Normal:
				switch (tokens.size()) {
				case 1:
					if (!name.empty()) {
						maps[info.name] = std::move(map);
					}
					name = tokens[0];
					state = ReadLineState::PropagationSpeed;
					break;
				case 3:
					src = std::stoi(tokens[0]);
					dest = std::stoi(tokens[1]);
					dist = std::stoi(tokens[2]);
					map.AddUndirectedEdge(src, dest, dist);
					break;
				default:
					throw InvalidInputException();
				}
				break;
			case ReadLineState::PropagationSpeed:
				pSpeed = std::stod(tokens[0]);
				state = ReadLineState::TransmissionSpeed;
				break;
			case ReadLineState::TransmissionSpeed:
				tSpeed = std::stod(tokens[0]);
				assert(name.size() == 1);
				info = MapInfo(name[0], pSpeed, tSpeed);
				map = Map(info);
				state = ReadLineState::Normal;
				break;
			default:
				break;
			}
		}
		maps[info.name] = std::move(map);
	}

	void Print() const {
		const int colWidth[] = { 8, 14, 11 };
		cout << left;
		cout << "The Server A has constructed a list of " << maps.size() << " maps:" << endl;
		cout << "-------------------------------------------" << endl;
		cout << setw(colWidth[0]) << "Map ID" << setw(colWidth[1]) << "Num Vertices" << setw(colWidth[2]) << "Num Edges" << endl;
		cout << "-------------------------------------------" << endl;
		for (const auto& m : maps) {
			cout << setw(colWidth[0]) << m.first << setw(colWidth[1]) << m.second.VertexCount() << setw(colWidth[2]) << m.second.UndirectedEdgeCount() << endl;
		}
		cout << "-------------------------------------------" << endl;
	}

public:
	MapManager() {
		BuildFromFile(MAP_FILENAME);
		Print();
	}

	AllShortestPath CalcShortestPath(const char map, const Node_t& src) const {
		return maps.at(map).CalcShortestPath(src);
	}
};

class Connection {
private:
	UdpReceiveSocketHelper receiveHelper;
public:
	Connection() : receiveHelper(UdpReceiveSocketHelper(SERVER_A_PORT)) {
		cout << "The Server A is up and running using UDP on port " << SERVER_A_PORT << "." << endl;
	}

	void Process(const MapManager& manager)  {
		while (true) {
			auto query = ClientQuery(receiveHelper);
			cout << "The Server A has received input for finding shortest paths: starting vertex " << query.sourceNode << " of map " << query.mapName << "." << endl;

			auto shortestPath = manager.CalcShortestPath(query.mapName, query.sourceNode);
			cout << "The Server A has identified the following shortest paths:" << endl;
			shortestPath.Print();

			auto sendHelper = UdpSendSocketHelper(HOST, SERVER_AWS_UDP_PORT);
			shortestPath.Encode(sendHelper);
			cout << "The Server A has sent shortest paths to AWS." << endl;
		}
	}
};

int main() {
	auto conn = Connection();

	auto manager = MapManager();
	
	conn.Process(manager);

	return 0;
}