#ifndef MktData_hpp
#define MktData_hpp
#include <sstream>
#include <string>
#include <string_view>

namespace sp {
  namespace MktData {
    size_t GetHourFromTimestamp(const std::string_view& timestamp) {
      if (timestamp.size() < 19) return 0; // Invalid timestamp length
      return std::stoul(timestamp.substr(11, 2));
    }

    //e.g. 2021-03-05 10:00:00.123
    struct MktDataTimeFormat {
      MktDataTimeFormat(const std::string_view& p_str)
        : year(p_str.substr(0, 4)),
          month(p_str.substr(5, 2)),
          day(p_str.substr(8, 2)),
          hour(p_str.substr(11, 2)),
          minute(p_str.substr(14, 2)),
          second(p_str.substr(17, 2)),
          millisecond(p_str.substr(20, 3)) {}

      size_t year;
      size_t month;
      size_t day;
      size_t hour;
      size_t minute;
      size_t second;
      size_t millisecond;
    };

  } // namespace sp
} // namespace MktData

#endif // MktData_hpp