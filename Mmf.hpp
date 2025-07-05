#ifndef Mmf_hpp
#define Mmf_hpp
#include <optional>
#include <string>

namespace sp {
  class MMF {
  public:
    enum class OpenMode {
      ReadOnly,
      WriteOnly, //Not implemented, use ReadWrite instead
      ReadWrite
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

    void Cleanup();
    int GetOpenFlags() const;
    int GetProtFlags() const;

  public:
    explicit MMF(const std::string& filename, OpenMode mode = OpenMode::ReadOnly);
    MMF(const std::string& filename, size_t offset, size_t size, OpenMode mode = OpenMode::ReadOnly);
    ~MMF();

    MMF(MMF&& other) noexcept;
    MMF& operator=(MMF&& other) noexcept;
    MMF(const MMF&) = delete;
    MMF& operator=(const MMF&) = delete;

    bool IsValid() const { return is_valid_; }
    Error GetLastError() const { return last_error_; }
    const std::string& GetFilename() const { return filename_; }
    bool IsEOF() const { return !is_valid_ || current_position_ >= mapped_size_; }
    std::optional<size_t> GetCurrentPosition() const { return is_valid_ ? std::optional<size_t>(current_position_) : std::nullopt; }
    std::optional<size_t> GetMappedSize() const { return is_valid_ ? std::optional<size_t>(mapped_size_) : std::nullopt; }
    std::optional<size_t> GetFileSize() const { return is_valid_ ? std::optional<size_t>(file_size_) : std::nullopt; }
    std::optional<const void*> GetData() const { return (is_valid_ && mapped_ptr_ != nullptr) ? std::optional<const void*>(mapped_ptr_) : std::nullopt; }
    std::optional<size_t> GetMappedOffset() const { return is_valid_ ? std::optional<size_t>(0) : std::nullopt; }

    std::optional<std::string> ReadLine();
    std::optional<std::string_view> ReadLineView();
    Error WriteLine(const std::string& line);
    Error Reset();
    Error SetPosition(size_t position);
  };
}//namespace sp

#endif
