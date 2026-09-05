#ifndef MKT_DATA_MSG_PROCESSOR_HPP
#define MKT_DATA_MSG_PROCESSOR_HPP
#include "MPSCQueue.hpp"

namespace sp {
  struct MktDataMessage;
}
using QueueType = sp::MPSCQueue<sp::MktDataMessage>;

namespace  sp {
  class MktDataMsgProcessor {
    MktDataMsgProcessor(QueueType& queue) : queue_(queue) {}
    void Run();
  private:
    QueueType& queue_;

  };
} // namespace sp