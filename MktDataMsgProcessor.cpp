#include "MktDataMsgProcessor.hpp"

#include <unordered_map>

#include "MktDataMessage.hpp"


void sp::MktDataMsgProcessor::Run() {

  while (true) {
    std::unordered_map<size_t, std::vector<MktDataMessage>> backlog_messages;
    std::vector<std::string> output;
    size_t current_batch_id = 0;

    auto msg = queue_.Dequeue();
    if (!msg) {
      // Handle empty message case, e.g., log or break
      continue;
    }
    if (current_batch_id == 0) [[unlikely]] {
      // If batch_id is zero, it indicates a new batch
      current_batch_id = msg->batch_id_;
    }

    if (current_batch_id < msg.batch_id_) {
      backlog_messages[msg.batch_id_].push_back(msg);
      continue;
    }
    //asset current_batch_id > msg.batch_id_;

    // Process the message
    std::string_

  }
}
