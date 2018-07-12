// Company: FETCH.ai
// Author: Jerome Maloberti
// Creation: 09/01/18
//
#include <iostream>
#include "clara.hpp"
#include "multiclient.h"
#include <google/protobuf/text_format.h>
#include <future>
#include "grid.h"
#include "maze.pb.h"
#include "schema.h"
#include "clientmsg.h"
#include <stack>

using fetch::oef::MultiClient;
using fetch::oef::Conversation;

enum class ExplorerState {OEF_WAITING_FOR_MAZE = 1,
                          OEF_WAITING_FOR_REGISTER,
                          MAZE_WAITING_FOR_REGISTER,
                          OEF_WAITING_FOR_MOVE_DELIVERED,
                          MAZE_WAITING_FOR_MOVE};

enum class SellerState {WAITING_FOR_CFP = 1,
                        OEF_WAITING_FOR_PROPOSE,
                        WAITING_FOR_AGREEMENT,
                        OEF_WAITING_FOR_TRANSACTION,
                        OEF_WAITING_FOR_RESOURCES};

enum class BuyerState {OEF_WAITING_FOR_AGENTS = 1,
                       OEF_WAITING_FOR_CFP,
                       WAITING_FOR_PROPOSE,
                       OEF_WAITING_FOR_ACCEPT,
                       OEF_WAITING_FOR_REFUSE,
                       WAITING_FOR_TRANSACTION,
                       WAITING_FOR_RESOURCES};
                        

enum class GridState : uint8_t { UNKNOWN, WALL, ROOM, VISITED_ROOM };

class Explorer : public MultiClient<ExplorerState,Explorer>
{
private:
  uint64_t _account;
  uint32_t _steps = 0;
  std::unique_ptr<Query> _mazeQuery;
  std::unique_ptr<Grid<GridState>> _grid;
  Position _current;
  fetch::oef::pb::Explorer_Direction _dir;
  std::string _maze;
  std::random_device _rd;
  std::mt19937 _gen;
  std::stack<fetch::oef::pb::Explorer_Direction> _path;

  void processOEFStatus(const fetch::oef::pb::Server_AgentMessage &msg) {
    // it is getting complicated with sellers and buyers, then oef messages for maze and trades are mixed.
    // not clear how to disembiguate. Maybe just ignore for now.
    std::shared_ptr<Conversation<ExplorerState>> conv;
    if(msg.status().has_cid()) {
      auto iter = _conversations.find(msg.status().cid());
      assert(iter != _conversations.end());
      conv = iter->second;
    } else {
      conv = _conversations[""];
    }
    switch(conv->getState()) {
    case ExplorerState::OEF_WAITING_FOR_REGISTER:
      assert(conv->msgId() == 0);
      conv->setState(ExplorerState::MAZE_WAITING_FOR_REGISTER);
      break;
    case ExplorerState::OEF_WAITING_FOR_MOVE_DELIVERED:
      conv->setState(ExplorerState::MAZE_WAITING_FOR_MOVE);
      break;
    default:
      std::cerr << "Error processOEFStatus " << static_cast<int>(conv->getState()) << " msgId " << conv->msgId() << std::endl;
    }
  }
  std::vector<fetch::oef::pb::Explorer_Direction> filterMove(const Position &pos, GridState val) const {
    std::vector<fetch::oef::pb::Explorer_Direction> res;
    if((pos.first > 0) && (_grid->get(pos.first - 1, pos.second) == val)) {
      res.push_back(fetch::oef::pb::Explorer_Direction_N);
    }
    if((pos.first < (_grid->rows() - 1)) && (_grid->get(pos.first + 1, pos.second) == val)) {
      res.push_back(fetch::oef::pb::Explorer_Direction_S);
    }
    if((pos.second > 0) && (_grid->get(pos.first, pos.second - 1) == val)) {
      res.push_back(fetch::oef::pb::Explorer_Direction_W);
    }
    if((pos.second < (_grid->cols() - 1)) && (_grid->get(pos.first, pos.second + 1) == val)) {
      res.push_back(fetch::oef::pb::Explorer_Direction_E);
    }
    return res;
  }
  fetch::oef::pb::Explorer_Direction choose(const std::vector<fetch::oef::pb::Explorer_Direction> &vals) {
    if(vals.size() == 1)
      return vals.front();
    std::uniform_int_distribution<> _dist(0, vals.size() - 1);
    int r = _dist(_gen);
    return vals[r];
  }
  fetch::oef::pb::Explorer_Direction generateRandomMove() {
    auto unknowns = filterMove(_current, GridState::UNKNOWN);
    assert(unknowns.size() == 0);
    auto rooms = filterMove(_current, GridState::ROOM);
    if(rooms.size() > 0)
      return choose(rooms);
    return choose(filterMove(_current, GridState::VISITED_ROOM));
  }
  fetch::oef::pb::Explorer_Direction backtrack() {
    assert(!_path.empty());
    auto dir = _path.top();
    _path.pop();
    switch(dir) {
    case fetch::oef::pb::Explorer_Direction_N:
      return fetch::oef::pb::Explorer_Direction_S;
    case fetch::oef::pb::Explorer_Direction_S:
      return fetch::oef::pb::Explorer_Direction_N;
    case fetch::oef::pb::Explorer_Direction_W:
      return fetch::oef::pb::Explorer_Direction_E;
    case fetch::oef::pb::Explorer_Direction_E:
      return fetch::oef::pb::Explorer_Direction_W;
    }
  }
  fetch::oef::pb::Explorer_Direction generateMove() {
    auto rooms = filterMove(_current, GridState::ROOM);
    if(rooms.size() == 0)
      return backtrack();
    _path.push(rooms.front());
    return rooms.front();
  }
  void sendMove(Conversation<ExplorerState> &conversation) {
    _dir = generateMove();
    std::cerr << "Sending move " << static_cast<int>(_dir);
    fetch::oef::pb::Explorer_Message outgoing;
    auto *explorer_mv = outgoing.mutable_move();
    explorer_mv->set_dir(_dir);
    conversation.setState(ExplorerState::OEF_WAITING_FOR_MOVE_DELIVERED);
    asyncWriteBuffer(_socket, conversation.envelope(outgoing), 5);
  }
  Position newPos(const Position &oldpos, fetch::oef::pb::Explorer_Direction dir) const {
    Position pos = oldpos;
    switch(dir) {
    case fetch::oef::pb::Explorer_Direction_N:
      --pos.first; break;
    case fetch::oef::pb::Explorer_Direction_S:
      ++pos.first; break;
    case fetch::oef::pb::Explorer_Direction_W:
      --pos.second; break;
    case fetch::oef::pb::Explorer_Direction_E:
      ++pos.second; break;
    }
    return pos;
  }
  void updateGrid(fetch::oef::pb::Maze_Cell cell, const Position &pos, int deltaRow, int deltaCol) {
    if((deltaRow < 0 && pos.first < abs(deltaRow)) || (deltaCol < 0 && pos.second < abs(deltaCol)))
      return ;
    Position newPos = std::make_pair(pos.first + deltaRow, pos.second + deltaCol);
    if(newPos.first >= _grid->rows() || newPos.second >= _grid->cols()) 
      return ;
    auto gridCell = cell == fetch::oef::pb::Maze_Cell::Maze_Cell_WALL ? GridState::WALL : GridState::ROOM;
    auto current = _grid->get(newPos);
    if(current != GridState::VISITED_ROOM) {
      assert(current == GridState::UNKNOWN || current == gridCell);
      _grid->set(newPos, gridCell);
    } else {
      assert(gridCell == GridState::ROOM);
    }
  }
  void updateGrid(const fetch::oef::pb::Maze_Environment &env, const Position &pos) {
    // set the grid appropriately and check that it is consistent with previous info
    updateGrid(env.north(), pos, -1, 0);
    updateGrid(env.south(), pos, 1, 0);
    updateGrid(env.west(), pos, 0, -1);
    updateGrid(env.east(), pos, 0, 1);
  }
  void registerSeller() {
    static Attribute mazeName{"maze_name", Type::String, true};
    static std::vector<Attribute> attributes{mazeName};
    static DataModel seller{"maze_seller", attributes, "Just a maze demo."};
    static bool registered = false;
    if(_steps == 10 && !registered) {
      registered = true;
      std::unordered_map<std::string,std::string> props{{"maze_name", _maze}};
      Instance instance{seller, props};
      Register reg{instance};
      asyncWriteBuffer(_socket, serialize(reg.handle()), 5);
    }
  }
  void processMoved(const fetch::oef::pb::Maze_Moved &mv, Conversation<ExplorerState> &conversation) {
    assert(conversation.getState() == ExplorerState::MAZE_WAITING_FOR_MOVE);
    std::string output;
    assert(google::protobuf::TextFormat::PrintToString(mv, &output));
    std::cerr << "Moved " << output << std::endl;
    auto response = mv.resp();
    switch(response) {
    case fetch::oef::pb::Maze_Response_IMPOSSIBLE: // should not happen, unless the agent is dumb.
      {
        Position pos = newPos(_current, _dir);
        _grid->set(pos, GridState::WALL);
        sendMove(conversation);
      }
      break;
    case fetch::oef::pb::Maze_Response_OK:
      _current = newPos(_current, _dir);
      _grid->set(_current, GridState::VISITED_ROOM);
      updateGrid(mv.env(), _current);
      sendMove(conversation);
      ++_steps;
      break;
    case fetch::oef::pb::Maze_Response_EXITED:
      _current = newPos(_current, _dir);
      updateGrid(mv.env(), _current);
      ++_steps;
      std::cerr << "Youhou, exit is " << _current.first << ":" << _current.second << std::endl << _grid->to_string() << std::endl;
      break;
    case fetch::oef::pb::Maze_Response_NOT_NOW:
    default:
      std::cerr << "Error processMoved " << static_cast<int>(conversation.getState()) << " msgId " << conversation.msgId() << std::endl;
    }
    registerSeller();
    std::cerr << "Moved\n" << _grid->to_string() << std::endl;
  }
  void processRegistered(const fetch::oef::pb::Maze_Registered &reg, Conversation<ExplorerState> &conversation) {
    std::string output;
    assert(google::protobuf::TextFormat::PrintToString(reg, &output));
    std::cerr << "Registered " << output << std::endl;
    auto &pos = reg.pos();
    auto &dim = reg.dim();
    _grid = std::unique_ptr<Grid<GridState>>(new Grid<GridState>(dim.rows(), dim.cols()));
    _current = std::make_pair<uint32_t, uint32_t>(pos.row(), pos.col());
    _grid->set(_current, GridState::VISITED_ROOM);
    updateGrid(reg.env(), _current);
    std::cerr << "Grid:\n" << _grid->to_string() << std::endl;
    sendMove(conversation);
  }
  void processClients(const fetch::oef::pb::Server_AgentMessage &msg, fetch::oef::Conversation<ExplorerState> &conversation) {
    assert(msg.has_content());
    assert(msg.content().has_origin());
    fetch::oef::pb::Maze_Message incoming;
    incoming.ParseFromString(msg.content().content());
    std::cerr << "Message from " << msg.content().origin() << " == " << conversation.dest() << std::endl;
    auto maze_case = incoming.msg_case();
    switch(maze_case) {
    case fetch::oef::pb::Maze_Message::kRegistered:
      assert(conversation.getState() == ExplorerState::MAZE_WAITING_FOR_REGISTER);
      processRegistered(incoming.registered(), conversation);
      break;
    case fetch::oef::pb::Maze_Message::kMoved:
      assert(conversation.getState() == ExplorerState::MAZE_WAITING_FOR_MOVE);
      processMoved(incoming.moved(), conversation);
      break;
    default:
      std::cerr << "Error processClients " << static_cast<int>(conversation.getState()) << " msgId " << conversation.msgId() << std::endl;
    }
  }
  void processAgents(const fetch::oef::pb::Server_AgentMessage &msg, fetch::oef::Conversation<ExplorerState> &conversation) {
    assert(_maze == "");
    assert(msg.has_agents());
    if(msg.agents().agents_size() == 0) { // no answer yet, let's try again 
      asyncWriteBuffer(_socket, serialize(_mazeQuery->handle()), 5);
    }
    _maze = msg.agents().agents(0);
    std::cerr << "Found maze " << _maze << std::endl;
    fetch::oef::pb::Explorer_Message outgoing;
    (void)outgoing.mutable_register_();
    Conversation<ExplorerState> maze_conversation{_maze};
    maze_conversation.setState(ExplorerState::OEF_WAITING_FOR_REGISTER);
    _conversations.insert({maze_conversation.uuid(), std::make_shared<Conversation<ExplorerState>>(maze_conversation)});
    asyncWriteBuffer(_socket, maze_conversation.envelope(outgoing),5);
  }
  
public:
  Explorer(asio::io_context &io_context, const std::string &id, const std::string &host, uint64_t account)
    : MultiClient<ExplorerState,Explorer>{io_context, id, host}, _account{account}, _gen{_rd()}
  {
    static Attribute version{"version", Type::Int, true};
    static std::vector<Attribute> attributes{version};
    static std::unordered_map<std::string,std::string> props{{"version", "1"}};
    static DataModel maze{"maze", attributes, "Just a maze demo."};
    static ConstraintType eqOne{Relation{Relation::Op::Eq, 1}};
    static Constraint version_c{version, eqOne};
    static QueryModel ql{{version_c}, maze};

    _mazeQuery = std::unique_ptr<Query>(new Query{ql});

    Conversation<ExplorerState> c{"", ""};
    c.setState(ExplorerState::OEF_WAITING_FOR_MAZE);
    _conversations.insert({"", std::make_shared<Conversation<ExplorerState>>(c)});
    asyncWriteBuffer(_socket, serialize(_mazeQuery->handle()), 5);
  }
  Explorer(const Explorer &) = delete;
  Explorer operator=(const Explorer &) = delete;

  void onMsg(const fetch::oef::pb::Server_AgentMessage &msg, fetch::oef::Conversation<ExplorerState> &conversation) {
    std::string output;
    assert(google::protobuf::TextFormat::PrintToString(msg, &output));
    std::cerr << "OnMsg cid " << conversation.uuid() << " dest " << conversation.dest() << " id " << conversation.msgId() << ": " << output << std::endl;
    switch(msg.payload_case()) {
    case fetch::oef::pb::Server_AgentMessage::kStatus: // oef
      processOEFStatus(msg);
      break;
    case fetch::oef::pb::Server_AgentMessage::kContent: // from an explorer
      processClients(msg, conversation);
      break;
    case fetch::oef::pb::Server_AgentMessage::kAgents: // answer for the query
      processAgents(msg, conversation);
      break;
    case fetch::oef::pb::Server_AgentMessage::PAYLOAD_NOT_SET:
    default:
      std::cerr << "Error onMsg " << static_cast<int>(conversation.getState()) << " msgId " << conversation.msgId() << std::endl;
    }
  }  
};

int main(int argc, char* argv[])
{
  bool showHelp = false;
  std::string host = "127.0.0.1";
  uint32_t nbClients = 100;
  std::string prefix = "Agent_";
  uint64_t account = 0;
  
  auto parser = clara::Help(showHelp)
    | clara::Opt(nbClients, "nbClients")["--nbClients"]["-n"]("Number of clients. Default 100.")
    | clara::Opt(account, "account")["--account"]["-a"]("Initial amount of tokens. Default 0.")
    | clara::Opt(prefix, "prefix")["--prefix"]["-p"]("Prefix used for all agents name. Default: Agent_")
    | clara::Opt(host, "host")["--host"]["-h"]("Host address to connect. Default: 127.0.0.1");
  auto result = parser.parse(clara::Args(argc, argv));

  if(showHelp || argc == 1) {
    std::cout << parser << std::endl;
  } 
  // need to increase max nb file open
  // > ulimit -n 8000
  // ulimit -n 1048576
  IoContextPool pool(4);
  pool.run();

  std::vector<std::unique_ptr<Explorer>> explorers;
  std::vector<std::future<std::unique_ptr<Explorer>>> futures;
  try {
    for(size_t i = 1; i <= nbClients; ++i) {
      std::string name = prefix;
      name += std::to_string(i);
      futures.push_back(std::async(std::launch::async,
                                   [&host,&pool,&account](const std::string &n){
                                     return std::make_unique<Explorer>(pool.getIoContext(),n, host, account);
                                   }, name));
    }
    std::cerr << "Futures created\n";
    for(auto &fut : futures) {
      explorers.emplace_back(fut.get());
    }
    std::cerr << "Futures got\n";
  } catch(std::exception &e) {
    std::cerr << "BUG " << e.what() << "\n";
  }
  std::cerr << "Start sleeping ...\n";
  std::this_thread::sleep_for(std::chrono::seconds{(nbClients / 500) + 2});
  std::cerr << "Stopped sleeping ...\n";
  pool.join();
  return 0;
}
