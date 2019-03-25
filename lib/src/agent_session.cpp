//------------------------------------------------------------------------------
//
//   Copyright 2018-2019 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "agent_session.hpp"
#include "agent.pb.h"
//#include <google/protobuf/text_format.h>

fetch::oef::Logger AgentSession_::logger = fetch::oef::Logger("oef-node::agent-session");
    
void AgentSession_::query_oef_search(std::shared_ptr<Buffer> query_buffer, 
  std::function<void(std::error_code, std::shared_ptr<Buffer>)> process_answer){
  /* TODO forward to new OEFSearch */
}

void AgentSession_::update_oef_search(std::shared_ptr<Buffer> update_buffer, 
  std::function<void(std::error_code, std::size_t length)> err_handler) {
  /* TODO forward to new OEFSearch */
}

void AgentSession_::process_register_description(uint32_t msg_id, const fetch::oef::pb::AgentDescription &desc) {
  /* TODO forward to new OEFSearch */
  description_ = Instance(desc.description());
  DEBUG(logger, "AgentSession_::processRegisterDescription setting description to agent {} : {}", 
      publicKey_, to_string(desc));
  if(!description_) {
    fetch::oef::pb::Server_AgentMessage answer;
    answer.set_answer_id(msg_id);
    auto *error = answer.mutable_oef_error();
    error->set_operation(fetch::oef::pb::Server_AgentMessage_OEFError::REGISTER_DESCRIPTION);
    logger.trace("AgentSession_::processRegisterDescription sending error {} to {}", error->operation(), publicKey_);
    send(answer);
  }
}

void AgentSession_::process_unregister_description(uint32_t msg_id) {
  /* TODO forward to new OEFSearch */
  description_ = stde::nullopt;
  DEBUG(logger, "AgentSession_::processUnregisterDescription setting description to agent {}", publicKey_);
}

void AgentSession_::process_register_service(uint32_t msg_id, const fetch::oef::pb::AgentDescription &desc) {
  /* TODO forward to new OEFSearch */
  DEBUG(logger, "AgentSession_::processRegisterService registering agent {} : {}", publicKey_, to_string(desc));
}

void AgentSession_::process_unregister_service(uint32_t msg_id, const fetch::oef::pb::AgentDescription &desc) {
  /* TODO forward to new OEFSearch */
  DEBUG(logger, "AgentSession_::processUnregisterService unregistering agent {} : {}", publicKey_, to_string(desc));
}

void AgentSession_::process_search_agents(uint32_t msg_id, const fetch::oef::pb::AgentSearch &search) {
  /* TODO forward to new OEFSearch */
  DEBUG(logger, "AgentSession_::processSearchAgents from agent {} : {}", publicKey_, to_string(search));
}

void AgentSession_::process_search_service(uint32_t msg_id, const fetch::oef::pb::AgentSearch &search) {
  /* TODO forward to new OEFSearch */
  DEBUG(logger, "AgentSession_::processQuery from agent {} : {}", publicKey_, to_string(search));
}

void AgentSession_::send_dialog_error(uint32_t msg_id, uint32_t dialogue_id, const std::string &origin) {
  fetch::oef::pb::Server_AgentMessage answer;
  answer.set_answer_id(msg_id);
  auto *error = answer.mutable_dialogue_error();
  error->set_dialogue_id(dialogue_id);
  error->set_origin(origin);
  logger.trace("AgentSession_::process_message sending dialogue error {} to {}", dialogue_id, publicKey_);
  send(answer);
}

void AgentSession_::send_error(uint32_t msg_id, fetch::oef::pb::Server_AgentMessage_OEFError error) {/*TODO*/}

void AgentSession_::process_message(uint32_t msg_id, fetch::oef::pb::Agent_Message *msg) {
  auto session = agentDirectory_.session(msg->destination());
  DEBUG(logger, "AgentSession_::process_message from agent {} : {}", publicKey_, to_string(*msg));
  logger.trace("AgentSession_::process_message to {} from {}", msg->destination(), publicKey_);
  uint32_t did = msg->dialogue_id();
  if(session) {
    fetch::oef::pb::Server_AgentMessage message;
    message.set_answer_id(msg_id);
    auto content = message.mutable_content();
    content->set_dialogue_id(did);
    content->set_origin(publicKey_);
    if(msg->has_content()) {
      content->set_allocated_content(msg->release_content());
    }
    if(msg->has_fipa()) {
      content->set_allocated_fipa(msg->release_fipa());
    }
    /* TOFIX until AgentDirectory returns agent_session_t */ 
    DEBUG(logger, "AgentSession_::process_message to agent {} : {}", msg->destination(), to_string(message));
    //auto buffer = serialize(message);
    session->send(message, 
        [this,did,msg_id,msg](std::error_code ec, std::size_t length) {
          if(ec) {
            send_dialog_error(msg_id, did, msg->destination());
          }
        }); 
  } else {
    send_dialog_error(msg_id, did, msg->destination());
  }
}

void AgentSession_::process(const std::shared_ptr<Buffer> &buffer) {
  auto envelope = serializer::deserialize<fetch::oef::pb::Envelope>(*buffer);
  auto payload_case = envelope.payload_case();
  uint32_t msg_id = envelope.msg_id();
  switch(payload_case) {
    case fetch::oef::pb::Envelope::kSendMessage:
      process_message(msg_id, envelope.release_send_message());
      break;
    case fetch::oef::pb::Envelope::kRegisterService:
      process_register_service(msg_id, envelope.register_service());
      break;
    case fetch::oef::pb::Envelope::kUnregisterService:
      process_unregister_service(msg_id, envelope.unregister_service());
      break;
    case fetch::oef::pb::Envelope::kRegisterDescription:
      process_register_description(msg_id, envelope.register_description());
      break;
    case fetch::oef::pb::Envelope::kUnregisterDescription:
      process_unregister_description(msg_id);
      break;
    case fetch::oef::pb::Envelope::kSearchAgents:
      process_search_agents(msg_id, envelope.search_agents());
      break;
    case fetch::oef::pb::Envelope::kSearchServices:
      process_search_service(msg_id, envelope.search_services());
      break;
    case fetch::oef::pb::Envelope::PAYLOAD_NOT_SET:
      logger.error("AgentSession_::process cannot process payload {} from {}", payload_case, publicKey_);
  }
}
      
void AgentSession_::read() {
        auto self(shared_from_this());
        comm_->receive_async([this, self](std::error_code ec, std::shared_ptr<Buffer> buffer) {
                                if(ec) {
                                  agentDirectory_.remove(publicKey_);
                                  logger.info("AgentSession_::read error on id {} ec {}", publicKey_, ec);
                                } else {
                                  process(buffer);
                                  read();
                                }
                             });
}

}
}
