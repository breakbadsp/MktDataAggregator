#ifndef UTILS_HPP
#define UTILS_HPP

namespace sp {
  unsigned int GetCpuCoreCount();
  size_t GetTotalSystemMemory();
  size_t GetMaxMemoryPerThread();
}