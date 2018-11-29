#pragma once

#include "agent.pb.h"
#include "schema.h"

namespace fetch {
  namespace oef {

    class Register {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit Register(const Instance &instance) {
        auto *reg = _envelope.mutable_register_service();
        auto *inst = reg->mutable_description();
        inst->CopyFrom(instance.handle());
      }
      const fetch::oef::pb::Envelope &handle() const { return _envelope; }
    };
    
    class Unregister {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit Unregister(const Instance &instance) {
        auto *reg = _envelope.mutable_unregister_service();
        auto *inst = reg->mutable_description();
        inst->CopyFrom(instance.handle());
      }
      const fetch::oef::pb::Envelope &handle() const { return _envelope; }
    };
    
    class UnregisterDescription {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit UnregisterDescription() {
        (void) _envelope.mutable_unregister_description();
      }
      const fetch::oef::pb::Envelope &handle() const { return _envelope; }
    };
    
    class SearchServices {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit SearchServices(uint32_t search_id, const QueryModel &model) {
        auto *desc = _envelope.mutable_search_services();
        desc->set_search_id(search_id);
        auto *mod = desc->mutable_query();
        mod->CopyFrom(model.handle());
      }
      const fetch::oef::pb::Envelope &handle() const { return _envelope; }
    };
    
    class Message {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit Message(uint32_t dialogueId, const std::string &dest, const std::string &msg) {
        auto *message = _envelope.mutable_send_message();
        message->set_dialogue_id(dialogueId);
        message->set_destination(dest);
        message->set_content(msg);
      }
      const fetch::oef::pb::Envelope &handle() const { return _envelope; }
    };
    
    class CFP {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit CFP(uint32_t dialogueId, const std::string &dest, const fetch::oef::CFPType &query, uint32_t msgId = 1, uint32_t target = 0) {
        auto *message = _envelope.mutable_send_message();
        message->set_dialogue_id(dialogueId);
        message->set_destination(dest);
        auto *fipa_msg = message->mutable_fipa();
        fipa_msg->set_msg_id(msgId);
        fipa_msg->set_target(target);
        auto *cfp = fipa_msg->mutable_cfp();
        query.match(
                    [cfp](const std::string &content) { cfp->set_content(content); },
                    [cfp](const QueryModel &query) { auto *q = cfp->mutable_query(); q->CopyFrom(query.handle());},
                    [cfp](stde::nullopt_t) { (void)cfp->mutable_nothing(); } );
      }
      fetch::oef::pb::Envelope &handle() { return _envelope; }
    };
    
    class Propose {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit Propose(uint32_t dialogueId, const std::string &dest, const fetch::oef::ProposeType &proposals, uint32_t msgId, uint32_t target) {
        auto *message = _envelope.mutable_send_message();
        message->set_dialogue_id(dialogueId);
        message->set_destination(dest);
        auto *fipa_msg = message->mutable_fipa();
        fipa_msg->set_msg_id(msgId);
        fipa_msg->set_target(target);
        auto *props = fipa_msg->mutable_propose();
        proposals.match(
                        [props](const std::string &content) { props->set_content(content); },
                        [props](const std::vector<Instance> &instances) {
                          auto *p = props->mutable_proposals();
                          auto *objs = p->mutable_objects();
                          objs->Reserve(instances.size());
                          for(auto &instance: instances) {
                            auto *inst = objs->Add();
                            inst->CopyFrom(instance.handle());
                          }
                        });
      }
      fetch::oef::pb::Envelope &handle() { return _envelope; }
    };
    
    class Accept {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit Accept(uint32_t dialogueId, const std::string &dest, uint32_t msgId, uint32_t target) {
        auto *message = _envelope.mutable_send_message();
        message->set_dialogue_id(dialogueId);
        message->set_destination(dest);
        auto *fipa_msg = message->mutable_fipa();
        fipa_msg->set_msg_id(msgId);
        fipa_msg->set_target(target);
        (void) fipa_msg->mutable_accept();
      }
      fetch::oef::pb::Envelope &handle() { return _envelope; }
    };
    
    class Decline {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit Decline(uint32_t dialogueId, const std::string &dest, uint32_t msgId, uint32_t target) {
        auto *message = _envelope.mutable_send_message();
        message->set_dialogue_id(dialogueId);
        message->set_destination(dest);
        auto *fipa_msg = message->mutable_fipa();
        fipa_msg->set_msg_id(msgId);
        fipa_msg->set_target(target);
        (void) fipa_msg->mutable_decline();
      }
      fetch::oef::pb::Envelope &handle() { return _envelope; }
    };
    
    
    class SearchAgents {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit SearchAgents(uint32_t search_id, const QueryModel &model) {
        auto *desc = _envelope.mutable_search_agents();
        desc->set_search_id(search_id);
        auto *mod = desc->mutable_query();
        mod->CopyFrom(model.handle());
      }
      const fetch::oef::pb::Envelope &handle() const { return _envelope; }
    };
    
    class Description {
    private:
      fetch::oef::pb::Envelope _envelope;
    public:
      explicit Description(const Instance &instance) {
        auto *desc = _envelope.mutable_register_description();
        auto *inst = desc->mutable_description();
        inst->CopyFrom(instance.handle());
      }
      const fetch::oef::pb::Envelope &handle() const { return _envelope; }
    };
  };
};
