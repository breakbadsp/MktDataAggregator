#pragma once
#include <thread>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>

namespace  sp {
  unsigned int GetCpuCoreCount() {
    return std::max(1u, std::thread::hardware_concurrency());
  }
  // Returns total system memory in bytes (Linux only)
  size_t GetTotalSystemMemory() {
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    while (std::getline(meminfo, line)) {
      if (line.find("MemTotal:") == 0) {
        std::istringstream iss(line);
        std::string label, unit;
        size_t mem_kb;
        iss >> label >> mem_kb >> unit;
        return mem_kb * 1024; // Convert kB to bytes
      }
    }
    return 0;
  }

  // Returns the max assignable memory per thread in bytes
  size_t GetMaxMemoryPerThread() {
    size_t total_mem = GetTotalSystemMemory();
    unsigned int cores = GetCpuCoreCount();
    if (cores == 0) return 0;
    return total_mem / cores;
  }
};// namespace sp