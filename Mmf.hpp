#ifndef MMF_H
#define MMF_H

#include <string>
#include <string_view>
#include <optional>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

enum class MMFError {
    None,
    FileOpenFailed,
    FileStatFailed,
    MapFailed,
    InvalidOffset,
    InvalidPosition,
    NotMapped,
    EndOfFile
};

class MMF {
private:
    int fd_;
    void* mapped_ptr_;
    size_t file_size_;
    size_t mapped_size_;
    size_t current_position_;
    std::string filename_;
    bool is_valid_;
    MMFError last_error_;

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

public:
    // Constructor that maps the entire file
    explicit MMF(const std::string& filename) 
        : fd_(-1)
        , mapped_ptr_(MAP_FAILED)
        , file_size_(0)
        , mapped_size_(0)
        , current_position_(0)
        , filename_(filename)
        , is_valid_(false)
        , last_error_(MMFError::None) {
        
        fd_ = open(filename.c_str(), O_RDONLY);
        if (fd_ == -1) {
            last_error_ = MMFError::FileOpenFailed;
            return;
        }

        struct stat file_stat;
        if (fstat(fd_, &file_stat) == -1) {
            last_error_ = MMFError::FileStatFailed;
            Cleanup();
            return;
        }
        
        file_size_ = file_stat.st_size;
        mapped_size_ = file_size_;

        if (file_size_ > 0) {
            mapped_ptr_ = mmap(nullptr, file_size_, PROT_READ, 
                             MAP_PRIVATE, fd_, 0);
            if (mapped_ptr_ == MAP_FAILED) {
                last_error_ = MMFError::MapFailed;
                Cleanup();
                return;
            }
        }

        is_valid_ = true;
    }

    // Constructor that maps a portion of the file
    MMF(const std::string& filename, size_t offset, size_t size) 
        : fd_(-1)
        , mapped_ptr_(MAP_FAILED)
        , file_size_(0)
        , mapped_size_(0)
        , current_position_(0)
        , filename_(filename)
        , is_valid_(false)
        , last_error_(MMFError::None) {
        
        fd_ = open(filename.c_str(), O_RDONLY);
        if (fd_ == -1) {
            last_error_ = MMFError::FileOpenFailed;
            return;
        }

        struct stat file_stat;
        if (fstat(fd_, &file_stat) == -1) {
            last_error_ = MMFError::FileStatFailed;
            Cleanup();
            return;
        }
        
        file_size_ = file_stat.st_size;

        if (offset >= file_size_) {
            last_error_ = MMFError::InvalidOffset;
            Cleanup();
            return;
        }

        long page_size = sysconf(_SC_PAGE_SIZE);
        if (page_size == -1) {
            last_error_ = MMFError::MapFailed;
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
                      << " with size: " << mapped_size_ << std::endl;
            mapped_ptr_ = mmap(nullptr, mapped_size_, PROT_READ, 
                             MAP_PRIVATE, fd_, page_aligned_offset);
            if (mapped_ptr_ == MAP_FAILED) {
                last_error_ = MMFError::MapFailed;
                Cleanup();
                return;
            }
        }

        is_valid_ = true;
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
        , last_error_(other.last_error_) {
        
        other.fd_ = -1;
        other.mapped_ptr_ = MAP_FAILED;
        other.file_size_ = 0;
        other.mapped_size_ = 0;
        other.current_position_ = 0;
        other.is_valid_ = false;
        other.last_error_ = MMFError::None;
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
            
            other.fd_ = -1;
            other.mapped_ptr_ = MAP_FAILED;
            other.file_size_ = 0;
            other.mapped_size_ = 0;
            other.current_position_ = 0;
            other.is_valid_ = false;
            other.last_error_ = MMFError::None;
        }
        return *this;
    }

    MMF(const MMF&) = delete;
    MMF& operator=(const MMF&) = delete;

    bool IsValid() const { return is_valid_; }
    MMFError GetLastError() const { return last_error_; }

    std::optional<std::string> ReadLine() {
        if (!is_valid_ || mapped_ptr_ == nullptr) {
            last_error_ = MMFError::NotMapped;
            return std::nullopt;
        }

        if (current_position_ >= mapped_size_) {
            last_error_ = MMFError::EndOfFile;
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

        last_error_ = MMFError::None;
        return line;
    }

    std::optional<std::string_view> ReadLineView() {
        if (!is_valid_ || mapped_ptr_ == nullptr) {
            last_error_ = MMFError::NotMapped;
            return std::nullopt;
        }

        if (current_position_ >= mapped_size_) {
            last_error_ = MMFError::EndOfFile;
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

        last_error_ = MMFError::None;
        return line_view;
    }

    MMFError Reset() {
        if (!is_valid_) {
            last_error_ = MMFError::NotMapped;
            return last_error_;
        }
        current_position_ = 0;
        last_error_ = MMFError::None;
        return MMFError::None;
    }

    std::optional<size_t> GetCurrentPosition() const {
        return is_valid_ ? std::optional<size_t>(current_position_) 
                         : std::nullopt;
    }

    MMFError SetPosition(size_t position) {
        if (!is_valid_) {
            last_error_ = MMFError::NotMapped;
            return last_error_;
        }
        if (position > mapped_size_) {
            last_error_ = MMFError::InvalidPosition;
            return last_error_;
        }
        current_position_ = position;
        last_error_ = MMFError::None;
        return MMFError::None;
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