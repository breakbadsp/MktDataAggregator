#include "Mmf.hpp"

#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace sp;
void MMF::Cleanup() {
    if (mapped_ptr_ != MAP_FAILED && mapped_ptr_ != nullptr) {
        munmap(mapped_ptr_, mapped_size_);
        mapped_ptr_ = nullptr;
    }
    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

int MMF::GetOpenFlags() const {
    switch (mode_) {
        case OpenMode::ReadWrite:
            return O_RDWR | O_CREAT;
        case OpenMode::ReadOnly:
        default:
            return O_RDONLY;
    }
}

int MMF::GetProtFlags() const {
    switch (mode_) {
        case OpenMode::WriteOnly:
            return PROT_WRITE;
        case OpenMode::ReadWrite:
            return PROT_READ | PROT_WRITE;
        case OpenMode::ReadOnly:
        default:
            return PROT_READ;
    }
}

MMF::MMF(const std::string& filename, OpenMode mode)
    : fd_(-1)
    , mapped_ptr_(MAP_FAILED)
    , file_size_(0)
    , mapped_size_(0)
    , current_position_(0)
    , offset_(0)
    , filename_(filename)
    , is_valid_(false)
    , last_error_(Error::None)
    , mode_(mode) {

    fd_ = open(filename.c_str(), GetOpenFlags(), 0644);
    if (fd_ == -1) {
        last_error_ = Error::FileOpenFailed;
        return;
    }

    struct stat file_stat;
    if (fstat(fd_, &file_stat) == -1) {
        last_error_ = Error::FileStatFailed;
        Cleanup();
        return;
    }

    mapped_size_ = file_size_ = file_stat.st_size;

    if (mode_ != OpenMode::ReadOnly && file_size_ == 0) {
        if (ftruncate(fd_, mapped_size_) == -1) {
            last_error_ = Error::FileOpenFailed;
            Cleanup();
            return;
        }
    }

    if (file_size_ == 0) {
        mapped_ptr_ = nullptr;
    } else {
        std::cout << "Mapping file: " << filename_
                  << " with size: " << file_size_ << std::endl;
        mapped_ptr_ = mmap(nullptr, mapped_size_, GetProtFlags(),
                         MAP_SHARED, fd_, 0);
        if (mapped_ptr_ == MAP_FAILED) {
            last_error_ = Error::MapFailed;
            Cleanup();
            return;
        }
    }
    is_valid_ = true;
}

MMF::MMF(const std::string& filename, size_t offset, size_t size, OpenMode mode)
    : fd_(-1)
    , mapped_ptr_(MAP_FAILED)
    , file_size_(0)
    , mapped_size_(0)
    , current_position_(0)
    , filename_(filename)
    , is_valid_(false)
    , last_error_(Error::None)
    , mode_(mode) {

    fd_ = open(filename.c_str(), GetOpenFlags(), 0644);
    if (fd_ == -1) {
        last_error_ = Error::FileOpenFailed;
        return;
    }

    struct stat file_stat;
    if (fstat(fd_, &file_stat) == -1) {
        last_error_ = Error::FileStatFailed;
        Cleanup();
        return;
    }

    file_size_ = file_stat.st_size;

    if (offset >= file_size_) {
        last_error_ = Error::InvalidOffset;
        Cleanup();
        return;
    }

    long page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size == -1) {
        last_error_ = Error::MapFailed;
        Cleanup();
        return;
    }

    size_t page_aligned_offset = (offset / page_size) * page_size;
    size_t max_available_from_offset = file_size_ - offset;
    size_t effective_size = std::min(size, max_available_from_offset);
    mapped_size_ = (offset - page_aligned_offset) + effective_size;

    if (mapped_size_ > 0) {
        std::cout << "Mapping file: " << filename_
                  << " with effective size: " << file_size_
                  << " from offset: " << offset
                  << " Page aligned offset: " << page_aligned_offset
                  << " with size: " << mapped_size_ << std::endl;

        mapped_ptr_ = mmap(nullptr, mapped_size_, PROT_READ,
                         MAP_PRIVATE, fd_, page_aligned_offset);
        if (mapped_ptr_ == MAP_FAILED) {
            last_error_ = Error::MapFailed;
            Cleanup();
            return;
        }
        if (page_aligned_offset < offset) {
            current_position_ = offset - page_aligned_offset;
        } else {
            current_position_ = 0;
        }
        offset_ = page_aligned_offset;
    }

    is_valid_ = true;
}

MMF::~MMF() {
    Cleanup();
}

MMF::MMF(MMF&& other) noexcept
    : fd_(other.fd_)
    , mapped_ptr_(other.mapped_ptr_)
    , file_size_(other.file_size_)
    , mapped_size_(other.mapped_size_)
    , current_position_(other.current_position_)
    , filename_(std::move(other.filename_))
    , is_valid_(other.is_valid_)
    , last_error_(other.last_error_)
    , mode_(other.mode_) {

    other.fd_ = -1;
    other.mapped_ptr_ = MAP_FAILED;
    other.file_size_ = 0;
    other.mapped_size_ = 0;
    other.current_position_ = 0;
    other.is_valid_ = false;
    other.last_error_ = Error::None;
}

MMF& MMF::operator=(MMF&& other) noexcept {
    if (this != &other) {
        Cleanup();

        fd_ = other.fd_;
        mapped_ptr_ = other.mapped_ptr_;
        file_size_ = other.file_size_;
        mapped_size_ = other.mapped_size_;
        current_position_ = other.current_position_;
        filename_ = std::move(other.filename_);
        is_valid_ = other.is_valid_;
        last_error_ = other.last_error_;
        mode_ = other.mode_;

        other.fd_ = -1;
        other.mapped_ptr_ = MAP_FAILED;
        other.file_size_ = 0;
        other.mapped_size_ = 0;
        other.current_position_ = 0;
        other.is_valid_ = false;
        other.last_error_ = Error::None;
    }
    return *this;
}

std::optional<std::string> MMF::ReadLine(bool p_extend_mapping) {
  const auto& bounds  = GetNextLineBounds(p_extend_mapping);
  if (!bounds)
    return std::nullopt;
  const auto line_start = bounds->first;
  const auto line_end = bounds->second;

  if (line_start > line_end) {
    last_error_ = Error::EndOfFile;
    return std::nullopt;
  }
  if (line_start == line_end) {
    // Empty line
    current_position_ = line_end + 1; // Move past the newline character
    last_error_ = Error::None;
    return std::string(); // Return empty string for empty line
  }

  const char* data = static_cast<const char*>(mapped_ptr_);
  std::string line(data + line_start, line_end - line_start);
  current_position_ = (line_end < mapped_size_ && data[line_end] == '\n') ? line_end + 1 : line_end;

  last_error_ = Error::None;
  return line;
}


std::optional<std::string_view> MMF::ReadLineView(bool p_extend_mapping) {
  const auto& bounds  = GetNextLineBounds(p_extend_mapping);
  if (!bounds)
    return std::nullopt;
  const auto line_start = bounds->first;
  const auto line_end = bounds->second;

  if (line_start > line_end) {
    last_error_ = Error::EndOfFile;
    return std::nullopt;
  }

  if (line_start == line_end) {
    last_error_ = Error::None;
    current_position_ = line_end + 1; // Move past the newline character
    return std::string_view(); // Return empty string view for empty line
  }

  const char* data = static_cast<const char*>(mapped_ptr_);
  std::string_view line(data + line_start, line_end - line_start);
  current_position_ = (line_end < mapped_size_ && data[line_end] == '\n') ? line_end + 1 : line_end;

  last_error_ = Error::None;
  return line;
}

// In Mmf.cpp or Mmf.hpp (as appropriate)
std::pair<size_t, size_t> MMF::GetAlignedOffsetAndSize(size_t offset, size_t size) const {
  long page_size = sysconf(_SC_PAGE_SIZE);
  size_t page_aligned_offset = (offset / page_size) * page_size;
  size_t offset_delta = offset - page_aligned_offset;
  size_t mapped_size = offset_delta + size;
  std::cout << "Page aligned offset: " << page_aligned_offset
    << ", Original offset: " << offset
    << ", Mapped size: " << mapped_size
    << ", Size: " << size << std::endl;
  return {page_aligned_offset, mapped_size};
}

std::optional<std::pair<size_t, size_t>> MMF::GetNextLineBounds(bool p_extend_mapping) {
  if (!is_valid_ || mapped_ptr_ == nullptr) {
    last_error_ = Error::NotMapped;
    return std::nullopt;
  }

  // If we've reached the end of the current mapping, try to remap if possible
  if (current_position_ >= mapped_size_) {
    // Only for full file mapping, not partial mapping
    if (p_extend_mapping && (file_size_ > offset_ + mapped_size_) ){
      size_t next_offset = offset_ + mapped_size_;
      size_t remaining = file_size_ - next_offset;
      size_t map_size = std::min(mapped_size_, remaining);

      // Unmap previous region
      if (mapped_ptr_ != MAP_FAILED && mapped_ptr_ != nullptr) {
        munmap(mapped_ptr_, mapped_size_);
      }

      const auto [new_offset, new_map_size] = GetAlignedOffsetAndSize(next_offset, map_size);
      // Map next region

      std::cout << "Creating new mapping for file:" << filename_ << ", current_position_:"
        << current_position_ << ", new_map_size:" << mapped_size_ << ", total file size:"
        << file_size_ << ", offset:" << new_offset << std::endl;

      mapped_ptr_ = mmap(nullptr, new_map_size, GetProtFlags(), MAP_SHARED, fd_, new_offset);
      if (mapped_ptr_ == MAP_FAILED) {
        mapped_ptr_ = nullptr;
        last_error_ = Error::MapFailed;
        is_valid_ = false;
        return std::nullopt;
      }
      offset_ = new_offset;
      mapped_size_ = new_map_size;
      current_position_ = next_offset - new_offset; // Reset current position to the start of the new mapping

      std::cout << "Created new mapping for file:" << filename_
        << ", current_position_:" << current_position_
        << ", new_map_size:" << mapped_size_
        << ", total file size:" << file_size_
        << ", offset:" << offset_ << std::endl;
      // file_size_ remains unchanged
    } else {
      last_error_ = Error::EndOfFile;
      return std::nullopt;
    }
  }

  const char* data = static_cast<const char*>(mapped_ptr_);
  size_t line_start = current_position_;
  size_t line_end = current_position_;

  // Handle empty line at the start of the mapping
  if (line_start < mapped_size_ && data[line_start] == '\n') {
    // Empty line
    std::cout << "Empty line at start of mapping" << std::endl;
    return std::make_pair(line_start, line_start);
  }

  while (line_end < mapped_size_ && data[line_end] != '\n') {
    line_end++;
  }

  return std::make_pair(line_start, line_end);
}


MMF::Error MMF::WriteLine(const std::string& line) {
    if (!is_valid_ || mapped_ptr_ == MAP_FAILED) {
        return Error::NotMapped;
    }

    if (mode_ == OpenMode::ReadOnly) {
        return Error::WriteError;
    }

    size_t write_size = line.size();

    if (write_size <= 0) {
      return Error::WriteError;
    }

    if (mapped_ptr_ == nullptr) {
        if (ftruncate(fd_, write_size + 1) == -1) {
            return Error::WriteError;
        }
        mapped_size_ = write_size + 1;
        std::cout << "Creating new mapping for file: " << filename_
          << " with size: " << mapped_size_ << std::endl;

        mapped_ptr_ = mmap(nullptr, mapped_size_, GetProtFlags(),
                         MAP_SHARED, fd_, 0);
        if (mapped_ptr_ == MAP_FAILED) {
            is_valid_ = false;
            return Error::MapFailed;
        }
    }

    if (current_position_ + write_size + 1 > mapped_size_) {
        size_t new_size = mapped_size_ * 2;
        if (new_size == 0) {
          new_size = write_size + 1;
        }
        while (current_position_ + write_size + 1 > new_size) {
            new_size *= 2;
        }
        std::cout << "Extending file: " << filename_
                << " from size: " << mapped_size_
                << " to new size: " << new_size
                << " current position: " << current_position_
                << " to accommodate new line of size: " << write_size + 1
                << std::endl;

        if (ftruncate(fd_, new_size) == -1) {
            return Error::WriteError;
        }

        munmap(mapped_ptr_, mapped_size_);
        mapped_size_ = new_size;
        mapped_ptr_ = mmap(nullptr, mapped_size_, GetProtFlags(),
                         MAP_SHARED, fd_, 0);

        if (mapped_ptr_ == MAP_FAILED) {
            is_valid_ = false;
            return Error::MapFailed;
        }
    }

    char* write_ptr = static_cast<char*>(mapped_ptr_) + current_position_;
    std::memcpy(write_ptr, line.c_str(), write_size);
    write_ptr[write_size] = '\n';
    current_position_ += write_size + 1;

    if (msync(mapped_ptr_, mapped_size_, MS_SYNC) == -1) {
        return Error::WriteError;
    }

    return Error::None;
}

MMF::Error MMF::Reset() {
    if (!is_valid_) {
        last_error_ = Error::NotMapped;
        return last_error_;
    }
    current_position_ = 0;
    last_error_ = Error::None;
    return Error::None;
}

MMF::Error MMF::SetPosition(size_t position) {
    if (!is_valid_) {
        last_error_ = Error::NotMapped;
        return last_error_;
    }
    if (position > mapped_size_) {
        last_error_ = Error::InvalidPosition;
        return last_error_;
    }
    current_position_ = position;
    last_error_ = Error::None;
    return Error::None;
}