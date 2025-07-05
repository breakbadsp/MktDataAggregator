#ifndef MMF_H
#define MMF_H

#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

class MMF {
public:
    enum class OpenMode {
        ReadOnly,   // Default mode
        WriteOnly,  // Write-only mode
        ReadWrite   // Both read and write
    };

    enum class Error {
        None,
        FileOpenFailed,
        FileStatFailed,
        MapFailed,
        InvalidOffset,
        InvalidPosition,
        NotMapped,
        EndOfFile,
        WriteError
    };

private:
    int fd_;
    void* mapped_ptr_;
    size_t file_size_;
    size_t mapped_size_;
    size_t current_position_;
    std::string filename_;
    bool is_valid_;
    Error last_error_;
    OpenMode mode_;

    void Cleanup() {
        if (mapped_ptr_ != MAP_FAILED && mapped_ptr_ != nullptr) {
            munmap(mapped_ptr_, mapped_size_);
            mapped_ptr_ = nullptr;
        }
        if (fd_ != -1) {
            close(fd_);
            fd_ = -1;
        }
    }

    int GetOpenFlags() const {
        switch (mode_) {
            case OpenMode::ReadWrite:
                return O_RDWR | O_CREAT;
            case OpenMode::ReadOnly:
            default:
                return O_RDONLY;
        }
    }

    int GetProtFlags() const {
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

public:
    // Constructor that maps the entire file
    explicit MMF(const std::string& filename, OpenMode mode = OpenMode::ReadOnly)
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

        // file_size and mapped_size are the same for full file mapping
        mapped_size_ = file_size_ = file_stat.st_size;

        if (mode_ != OpenMode::ReadOnly && file_size_ == 0) {
            // For write modes, create initial file size
            if (ftruncate(fd_, mapped_size_) == -1) {
                last_error_ = Error::FileOpenFailed;
                Cleanup();
                return;
            }
        }

        if (file_size_ == 0) {
          // we do not map empty files
          // Reading will return EndOfFile
          // Writing will extend the file
          mapped_ptr_ = nullptr;
        }
         else {
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

    // Constructor that maps a portion of the file
    MMF(const std::string& filename, size_t offset, size_t size, OpenMode mode = OpenMode::ReadOnly)
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

        // Calculate page-aligned mapping
        size_t page_aligned_offset = (offset / page_size) * page_size;
        
        // Cap the size to what's actually available from the requested offset
        size_t max_available_from_offset = file_size_ - offset;
        size_t effective_size = std::min(size, max_available_from_offset);
        
        // Now calculate mapped size from page-aligned offset
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
                // Adjust current position to the requested offset
                current_position_ = offset - page_aligned_offset;
            } else {
                current_position_ = 0; // If no offset adjustment, start at 0
            }
        }

        is_valid_ = true;
    }

    // Write a line to the memory-mapped file
    Error WriteLine(const std::string& line) {
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
            // If the file is empty, we need to create a new mapping
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
            // Need to extend the file
            std::cout << "Extending file: " << filename_
                    << " from size: " << mapped_size_
                    << " to new size: " << new_size
                    << " current position: " << current_position_
                    << " to accommodate new line of size: " << write_size + 1
                    << std::endl;

            if (ftruncate(fd_, new_size) == -1) {
                return Error::WriteError;
            }

            // Unmap and remap with new size
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

        // Ensure changes are written to disk
        if (msync(mapped_ptr_, mapped_size_, MS_SYNC) == -1) {
            return Error::WriteError;
        }

        return Error::None;
    }

    ~MMF() { Cleanup(); }

    // Move constructor
    MMF(MMF&& other) noexcept 
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

    // Move assignment
    MMF& operator=(MMF&& other) noexcept {
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

    MMF(const MMF&) = delete;
    MMF& operator=(const MMF&) = delete;

    bool IsValid() const { return is_valid_; }
    Error GetLastError() const { return last_error_; }

    std::optional<std::string> ReadLine() {
        if (!is_valid_ || mapped_ptr_ == nullptr) {
            last_error_ = Error::NotMapped;
            return std::nullopt;
        }

        if (current_position_ >= mapped_size_) {
            last_error_ = Error::EndOfFile;
            return std::nullopt;
        }

        const char* data = static_cast<const char*>(mapped_ptr_);
        size_t line_start = current_position_;
        size_t line_end = current_position_;

        while (line_end < mapped_size_ && data[line_end] != '\n') {
            line_end++;
        }

        std::string line(data + line_start, line_end - line_start);
        current_position_ = (line_end < mapped_size_ && 
                           data[line_end] == '\n') ? line_end + 1 : line_end;

        last_error_ = Error::None;
        return line;
    }

    std::optional<std::string_view> ReadLineView() {
        if (!is_valid_ || mapped_ptr_ == nullptr) {
            last_error_ = Error::NotMapped;
            return std::nullopt;
        }

        if (current_position_ >= mapped_size_) {
            last_error_ = Error::EndOfFile;
            return std::nullopt;
        }

        const char* data = static_cast<const char*>(mapped_ptr_);
        size_t line_start = current_position_;
        size_t line_end = current_position_;

        while (line_end < mapped_size_ && data[line_end] != '\n') {
            line_end++;
        }

        std::string_view line_view(data + line_start, line_end - line_start);
        current_position_ = (line_end < mapped_size_ && 
                           data[line_end] == '\n') ? line_end + 1 : line_end;

        last_error_ = Error::None;
        return line_view;
    }

    Error Reset() {
        if (!is_valid_) {
            last_error_ = Error::NotMapped;
            return last_error_;
        }
        current_position_ = 0;
        last_error_ = Error::None;
        return Error::None;
    }

    std::optional<size_t> GetCurrentPosition() const {
        return is_valid_ ? std::optional<size_t>(current_position_) 
                         : std::nullopt;
    }

    Error SetPosition(size_t position) {
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

    std::optional<size_t> GetMappedSize() const {
        return is_valid_ ? std::optional<size_t>(mapped_size_) : std::nullopt;
    }

    std::optional<size_t> GetFileSize() const {
        return is_valid_ ? std::optional<size_t>(file_size_) : std::nullopt;
    }

    const std::string& GetFilename() const { return filename_; }

    bool IsEOF() const {
        return !is_valid_ || current_position_ >= mapped_size_;
    }

    std::optional<const void*> GetData() const {
        return (is_valid_ && mapped_ptr_ != nullptr) ? 
               std::optional<const void*>(mapped_ptr_) : std::nullopt;
    }

    std::optional<size_t> GetMappedOffset() const {
        return is_valid_ ? std::optional<size_t>(0) : std::nullopt;
    }
};

#endif // MMF_H