#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "server.h"
#include <iostream>
#include <chrono>
#include <future>
#include "sd.h"
#include "multiclient.h"

using namespace fetch::oef;

class SimpleAgent : public fetch::oef::Agent {
 public:
  std::vector<std::string> results_;
  SimpleAgent(const std::string &agentId, asio::io_context &io_context, const std::string &host)
    : fetch::oef::Agent{std::unique_ptr<fetch::oef::OEFCoreInterface>(new fetch::oef::OEFCoreNetworkProxy{agentId, io_context, host})}
  {
    start();
  }
  virtual ~SimpleAgent() = default;
  void onError(fetch::oef::pb::Server_AgentMessage_Error_Operation operation, stde::optional<uint32_t> dialogueId, stde::optional<uint32_t> msgId) override {}
  void onSearchResult(uint32_t search_id, const std::vector<std::string> &results) override {
    results_ = results;
  }
  void onMessage(const std::string &from, uint32_t dialogueId, const std::string &content) override {}
  void onCFP(const std::string &from, uint32_t dialogueId, uint32_t msgId, uint32_t target, const fetch::oef::CFPType &constraints) override {}
  void onPropose(const std::string &from, uint32_t dialogueId, uint32_t msgId, uint32_t target, const fetch::oef::ProposeType &proposals) override {}
  void onAccept(const std::string &from, uint32_t dialogueId, uint32_t msgId, uint32_t target) override {}
  void onDecline(const std::string &from, uint32_t dialogueId, uint32_t msgId, uint32_t target) override {}
};

class SimpleAgentLocal : public fetch::oef::Agent {
 public:
  std::vector<std::string> results_;
  SimpleAgentLocal(const std::string &agentId, fetch::oef::SchedulerPB &scheduler)
    : fetch::oef::Agent{std::unique_ptr<fetch::oef::OEFCoreInterface>(new fetch::oef::OEFCoreLocalPB{agentId, scheduler})}
  {
    start();
  }
  virtual ~SimpleAgentLocal() = default;
  void onError(fetch::oef::pb::Server_AgentMessage_Error_Operation operation, stde::optional<uint32_t> dialogueId, stde::optional<uint32_t> msgId) override {}
  void onSearchResult(uint32_t search_id, const std::vector<std::string> &results) override {
    std::cerr << "onSearchResult " << results.size() << std::endl;
    results_ = results;
  }
  void onMessage(const std::string &from, uint32_t dialogueId, const std::string &content) override {}
  void onCFP(const std::string &from, uint32_t dialogueId, uint32_t msgId, uint32_t target, const fetch::oef::CFPType &constraints) override {}
  void onPropose(const std::string &from, uint32_t dialogueId, uint32_t msgId, uint32_t target, const fetch::oef::ProposeType &proposals) override {}
  void onAccept(const std::string &from, uint32_t dialogueId, uint32_t msgId, uint32_t target) override {}
  void onDecline(const std::string &from, uint32_t dialogueId, uint32_t msgId, uint32_t target) override {}
};

TEST_CASE("testing register", "[ServiceDiscovery]") {
  fetch::oef::Server as;
  std::cerr << "Server created\n";
  as.run();
  std::cerr << "Server started\n";
  std::cerr << "Waiting\n";
  std::this_thread::sleep_for(std::chrono::seconds{1});
  std::cerr << "NbAgents " << as.nbAgents() << "\n";
  REQUIRE(as.nbAgents() == 0);
  {
    IoContextPool pool(2);
    pool.run();
    SimpleAgent c1("Agent1", pool.getIoContext(), "127.0.0.1");
    SimpleAgent c2("Agent2", pool.getIoContext(), "127.0.0.1");
    SimpleAgent c3("Agent3", pool.getIoContext(), "127.0.0.1");
    REQUIRE(as.nbAgents() == 3);
    Attribute manufacturer{"manufacturer", Type::String, true};
    Attribute colour{"colour", Type::String, false};
    Attribute luxury{"luxury", Type::Bool, true};
    DataModel car{"car", {manufacturer, colour, luxury}, "Car sale."};
    Instance ferrari{car, {{"manufacturer", VariantType{std::string{"Ferrari"}}},
                           {"colour", VariantType{std::string{"Aubergine"}}},
                           {"luxury", VariantType{true}}}};
    c1.registerService(ferrari);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    c1.unregisterService(ferrari);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    c1.registerService(ferrari);
    Instance lamborghini{car, {{"manufacturer", VariantType{std::string{"Lamborghini"}}},
                               {"luxury", VariantType{true}}}};
    c2.registerService(lamborghini);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    ConstraintType eqTrue{Relation{Relation::Op::Eq, true}};
    Constraint luxury_c{luxury, eqTrue};
    QueryModel q1{{luxury_c}, car};
    c3.searchServices(1, q1);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    auto agents = c3.results_;
    std::sort(agents.begin(), agents.end());
    REQUIRE(agents.size() == 2);
    REQUIRE(agents == std::vector<std::string>({"Agent1", "Agent2"}));
    c1.stop();
    c2.stop();
    c3.stop();
    pool.stop();
    std::cerr << "Agent3 received\n";
    for(auto &s : agents) {
      std::cerr << s << std::endl;
    }
  }
  //  std::this_thread::sleep_for(std::chrono::seconds{1});
  as.stop();
  std::cerr << "Server stopped\n";
}

TEST_CASE("local testing register", "[ServiceDiscovery]") {
  // spdlog::set_level(spdlog::level::level_enum::trace);
  fetch::oef::SchedulerPB scheduler;
  std::cerr << "Scheduler created\n";
  std::cerr << "NbAgents " << scheduler.nbAgents() << "\n";
  REQUIRE(scheduler.nbAgents() == 0);
  {
    SimpleAgentLocal c1("Agent1", scheduler);
    SimpleAgentLocal c2("Agent2", scheduler);
    SimpleAgentLocal c3("Agent3", scheduler);
    REQUIRE(scheduler.nbAgents() == 3);
    Attribute manufacturer{"manufacturer", Type::String, true};
    Attribute colour{"colour", Type::String, false};
    Attribute luxury{"luxury", Type::Bool, true};
    DataModel car{"car", {manufacturer, colour, luxury}, "Car sale."};
    Instance ferrari{car, {{"manufacturer", VariantType{std::string{"Ferrari"}}},
                           {"colour", VariantType{std::string{"Aubergine"}}},
                           {"luxury", VariantType{true}}}};
    c1.registerService(ferrari);
    c1.unregisterService(ferrari);
    c1.registerService(ferrari);
    Instance lamborghini{car, {{"manufacturer", VariantType{std::string{"Lamborghini"}}},
                               {"luxury", VariantType{true}}}};
    c2.registerService(lamborghini);
    ConstraintType eqTrue{Relation{Relation::Op::Eq, true}};
    Constraint luxury_c{luxury, eqTrue};
    QueryModel q1{{luxury_c}, car};
    c3.searchServices(1, q1);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    auto agents = c3.results_;
    std::sort(agents.begin(), agents.end());
    REQUIRE(agents.size() == 2);
    REQUIRE(agents == std::vector<std::string>({"Agent1", "Agent2"}));
    c1.stop();
    c2.stop();
    c3.stop();
    std::cerr << "Agent3 received\n";
    for(auto &s : agents) {
      std::cerr << s << std::endl;
    }
  }
  scheduler.stop();
  std::cerr << "Scheduler stopped\n";
}

TEST_CASE("description", "[ServiceDiscovery]") {
  fetch::oef::Server as;
  std::cerr << "Server created\n";
  as.run();
  std::cerr << "Server started\n";
  std::cerr << "Waiting\n";
  std::this_thread::sleep_for(std::chrono::seconds{1});
  std::cerr << "NbAgents " << as.nbAgents() << "\n";
  REQUIRE(as.nbAgents() == 0);
  {
    IoContextPool pool(2);
    pool.run();
    SimpleAgent c1("Agent1", pool.getIoContext(), "127.0.0.1");
    SimpleAgent c2("Agent2", pool.getIoContext(), "127.0.0.1");
    SimpleAgent c3("Agent3", pool.getIoContext(), "127.0.0.1");
    REQUIRE(as.nbAgents() == 3);

    Attribute manufacturer{"manufacturer", Type::String, true};
    Attribute model{"model", Type::String, true};
    Attribute wireless{"wireless", Type::Bool, true};
    DataModel station{"weather_station", {manufacturer, model, wireless}, "Weather station"};
    Instance youshiko{station, {{"manufacturer", VariantType{std::string{"Youshiko"}}},
                                {"model", VariantType{std::string{"YC9315"}}},
                                {"wireless", VariantType{true}}}};
    Instance opes{station, {{"manufacturer", VariantType{std::string{"Opes"}}},
                            {"model", VariantType{std::string{"17500"}}}, {"wireless", VariantType{true}}}};
    c1.registerDescription(youshiko);
    c2.registerDescription(opes);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    ConstraintType eqTrue{Relation{Relation::Op::Eq, true}};
    Constraint wireless_c{wireless, eqTrue};
    QueryModel q1{{wireless_c}, station};
    c3.searchAgents(1, q1);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    auto agents = c3.results_;
    std::sort(agents.begin(), agents.end());
    REQUIRE(agents.size() == 2);
    REQUIRE(agents == std::vector<std::string>({"Agent1", "Agent2"}));
    ConstraintType eqYoushiko{Relation{Relation::Op::Eq, std::string{"Youshiko"}}};
    Constraint manufacturer_c{manufacturer, eqYoushiko};
    QueryModel q2{{manufacturer_c}};
    c3.searchAgents(1, q2);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    auto agents2 = c3.results_;
    REQUIRE(agents2.size() == 1);
    REQUIRE(agents2 == std::vector<std::string>({"Agent1"}));
    
    c1.stop();
    c2.stop();
    c3.stop();
    pool.stop();
  }
  as.stop();
  std::cerr << "Server stopped\n";
}

TEST_CASE("local description", "[ServiceDiscovery]") {
  fetch::oef::SchedulerPB scheduler;
  std::cerr << "Scheduler created\n";
  std::this_thread::sleep_for(std::chrono::seconds{1});
  std::cerr << "NbAgents " << scheduler.nbAgents() << "\n";
  REQUIRE(scheduler.nbAgents() == 0);
  {
    SimpleAgentLocal c1("Agent1", scheduler);
    SimpleAgentLocal c2("Agent2", scheduler);
    SimpleAgentLocal c3("Agent3", scheduler);
    REQUIRE(scheduler.nbAgents() == 3);

    Attribute manufacturer{"manufacturer", Type::String, true};
    Attribute model{"model", Type::String, true};
    Attribute wireless{"wireless", Type::Bool, true};
    DataModel station{"weather_station", {manufacturer, model, wireless}, "Weather station"};
    Instance youshiko{station, {{"manufacturer", VariantType{std::string{"Youshiko"}}},
                                {"model", VariantType{std::string{"YC9315"}}},
                                {"wireless", VariantType{true}}}};
    Instance opes{station, {{"manufacturer", VariantType{std::string{"Opes"}}},
                            {"model", VariantType{std::string{"17500"}}}, {"wireless", VariantType{true}}}};
    c1.registerDescription(youshiko);
    c2.registerDescription(opes);
    ConstraintType eqTrue{Relation{Relation::Op::Eq, true}};
    Constraint wireless_c{wireless, eqTrue};
    QueryModel q1{{wireless_c}, station};
    c3.searchAgents(1, q1);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    auto agents = c3.results_;
    std::sort(agents.begin(), agents.end());
    REQUIRE(agents.size() == 2);
    REQUIRE(agents == std::vector<std::string>({"Agent1", "Agent2"}));
    ConstraintType eqYoushiko{Relation{Relation::Op::Eq, std::string{"Youshiko"}}};
    Constraint manufacturer_c{manufacturer, eqYoushiko};
    QueryModel q2{{manufacturer_c}};
    c3.searchAgents(1, q2);
    std::this_thread::sleep_for(std::chrono::seconds{1});
    auto agents2 = c3.results_;
    REQUIRE(agents2.size() == 1);
    REQUIRE(agents2 == std::vector<std::string>({"Agent1"}));
    
    c1.stop();
    c2.stop();
    c3.stop();
  }
  scheduler.stop();
  std::cerr << "Scheduler stopped\n";
}

TEST_CASE( "testing Server", "[Server]" ) {
  fetch::oef::Server as;
  std::cerr << "Server created\n";
  as.run();
  std::cerr << "Server started\n";
  REQUIRE(as.nbAgents() == 0);
  
  SECTION("1 agent") {
    IoContextPool pool(2);
    pool.run();
    SimpleAgent c1("Agent1", pool.getIoContext(), "127.0.0.1");
    REQUIRE(as.nbAgents() == 1);
    c1.stop();
    pool.stop();
  }
  std::this_thread::sleep_for(std::chrono::seconds{1});
  REQUIRE(as.nbAgents() == 0);
  //too fast ?
  SECTION("1000 agents") {
    // need to increase max nb file open
    // > ulimit -n 8000
    // ulimit -n 1048576
    
    IoContextPool pool(2);
    pool.run();
    std::vector<std::unique_ptr<SimpleAgent>> clients;
    std::vector<std::future<std::unique_ptr<SimpleAgent>>> futures;
    size_t nbClients = 1000;
    try {
      for(size_t i = 1; i <= nbClients; ++i) {
        std::string name = "Agent_";
        name += std::to_string(i);
        futures.push_back(std::async(std::launch::async, [&pool](const std::string &n){
                                                           return std::make_unique<SimpleAgent>(n, pool.getIoContext(), "127.0.0.1");
                                                         }, name));
      }
      std::cerr << "Futures created\n";
      for(auto &fut : futures) {
        clients.emplace_back(fut.get());
      }
      std::cerr << "Futures got\n";
    } catch(std::exception &e) {
      std::cerr << "BUG " << e.what() << "\n";
    }
    pool.stop();
    REQUIRE(as.nbAgents() == nbClients);
  }
  std::this_thread::sleep_for(std::chrono::seconds{1});
  REQUIRE(as.nbAgents() == 0);
  
  as.stop();
  std::cerr << "Server stopped\n";
}
