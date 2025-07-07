#ifndef MKT_DATA_MESSAGE_HPP
#define MKT_DATA_MESSAGE_HPP
#include <string_view>


namespace sp {
  struct MktDataMessage {
    MktDataMessage(
      std::string_view p_symbol,
      std::string_view p_mkt_data,
      size_t p_batch_id)
    : symbol_(p_symbol),
      mkt_data_(p_mkt_data),
      batch_id_(p_batch_id) {}

    std::string_view symbol_; // Symbol for the market data
    std::string_view mkt_data_; // Market data
    size_t batch_id_; // Unique identifier for the batch
  };
}