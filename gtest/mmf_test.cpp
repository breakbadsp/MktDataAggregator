#include "../Mmf.hpp"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <random>
#include <string>
#include <unistd.h>
#include <vector>

using namespace sp;
// Helper function to convert MMF::Error to string for better error messages
std::string MMFErrorToString(MMF::Error error) {
  switch (error) {
    case MMF::Error::None:
      return "None";
    case MMF::Error::FileOpenFailed:
      return "FileOpenFailed";
    case MMF::Error::FileStatFailed:
      return "FileStatFailed";
    case MMF::Error::MapFailed:
      return "MapFailed";
    case MMF::Error::InvalidOffset:
      return "InvalidOffset";
    case MMF::Error::InvalidPosition:
      return "InvalidPosition";
    case MMF::Error::NotMapped:
      return "NotMapped";
    case MMF::Error::EndOfFile:
      return "EndOfFile";
    case MMF::Error::WriteError:
      return "WriteError";
    default:
      return "Unknown";
  }
}

std::string GetErrnoInfo() {
  std::string msg = " (errno: ";
  msg += std::to_string(errno) + ", " + strerror(errno) + ")";
  return msg;
}

// Helper function to create detailed error message
std::string CreateErrorMessage(const std::string& context,
                             MMF::Error error,
                             bool include_errno = false) {
  std::string msg = context + ", error: " + MMFErrorToString(error);
  if (include_errno &&
      (error == MMF::Error::FileOpenFailed ||
       error == MMF::Error::FileStatFailed ||
       error == MMF::Error::MapFailed)) {
    msg += GetErrnoInfo();
  }
  return msg;
}

class MMFTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Create test directory
    test_dir_ = "test_mmf_files";
    std::filesystem::create_directory(test_dir_);

    // Create test files
    CreateTestFiles();
  }

  void TearDown() override {
    // Clean up test files
    std::filesystem::remove_all(test_dir_);
  }

  void CreateTestFiles() {
    // Empty file
    empty_file_ = test_dir_ + "/empty.txt";
    std::ofstream(empty_file_).close();

    // Single line file
    single_line_file_ = test_dir_ + "/single_line.txt";
    std::ofstream(single_line_file_) << "Hello World";

    // Multi-line file
    multi_line_file_ = test_dir_ + "/multi_line.txt";
    std::ofstream multi_file(multi_line_file_);
    multi_file << "Line 1\n";
    multi_file << "Line 2\n";
    multi_file << "Line 3\n";
    multi_file << "Line 4";
    multi_file.close();

    // File with different line endings
    mixed_endings_file_ = test_dir_ + "/mixed_endings.txt";
    std::ofstream mixed_file(mixed_endings_file_, std::ios::binary);
    mixed_file << "Unix line\n";
    mixed_file << "Windows line\r\n";
    mixed_file << "Mac line\n";
    mixed_file << "No ending";
    mixed_file.close();

    // Large file for partial mapping tests
    large_file_ = test_dir_ + "/large.txt";
    std::ofstream large_file(large_file_);
    for (int i = 0; i < 1000; ++i) {
      large_file << "This is line " << i
                 << " with some content to make it longer\n";
    }
    large_file.close();

    // Binary file with null bytes
    binary_file_ = test_dir_ + "/binary.bin";
    std::ofstream binary_file(binary_file_, std::ios::binary);
    for (int i = 0; i < 256; ++i) {
      binary_file.put(static_cast<char>(i));
    }
    binary_file.close();

    // File with very long lines
    long_lines_file_ = test_dir_ + "/long_lines.txt";
    std::ofstream long_file(long_lines_file_);
    std::string long_line(10000, 'A');
    long_file << long_line << "\n";
    std::string another_long_line(5000, 'B');
    long_file << another_long_line;
    long_file.close();

    // New file for write tests
    write_test_file_ = test_dir_ + "/write_test.txt";
  }

  std::string test_dir_;
  std::string empty_file_;
  std::string single_line_file_;
  std::string multi_line_file_;
  std::string mixed_endings_file_;
  std::string large_file_;
  std::string binary_file_;
  std::string long_lines_file_;
  std::string non_existent_file_ = "non_existent_file.txt";
  std::string write_test_file_;  // New member for write tests
};

// Constructor Tests
TEST_F(MMFTest, ConstructorFullFileValid) {
  MMF mmf(single_line_file_);
  EXPECT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::None)
      << CreateErrorMessage("Expected no error", mmf.GetLastError(),
                             true);
  EXPECT_EQ(mmf.GetFilename(), single_line_file_);
}

TEST_F(MMFTest, ConstructorFullFileNonExistent) {
  MMF mmf(non_existent_file_);
  EXPECT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::FileOpenFailed)
      << CreateErrorMessage("Expected FileOpenFailed",
                             mmf.GetLastError(), true);
  EXPECT_EQ(mmf.GetFilename(), non_existent_file_);
}

TEST_F(MMFTest, ConstructorFullFileEmpty) {
  MMF mmf(empty_file_);
  EXPECT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid for empty file",
                             mmf.GetLastError(), true);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::None)
      << CreateErrorMessage("Expected no error", mmf.GetLastError(),
                             true);
  EXPECT_EQ(mmf.GetFileSize().value_or(0), 0);
}

TEST_F(MMFTest, ConstructorPartialFileValid) {
  MMF mmf(large_file_, 0, 1024);
  EXPECT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid for partial mapping",
                             mmf.GetLastError(), true);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::None)
      << CreateErrorMessage("Expected no error", mmf.GetLastError(),
                             true);
  EXPECT_EQ(mmf.GetMappedSize().value_or(0), 1024);
}

TEST_F(MMFTest, ConstructorPartialFileInvalidOffset) {
  size_t large_offset = 1000000; // Larger than file size
  MMF mmf(single_line_file_, large_offset, 100);
  EXPECT_FALSE(mmf.IsValid())
      << CreateErrorMessage(
             "MMF should be invalid for offset beyond file size",
             mmf.GetLastError(), true);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::InvalidOffset)
      << CreateErrorMessage("Expected InvalidOffset",
                             mmf.GetLastError(), true);
}

TEST_F(MMFTest, ConstructorPartialFileNonExistent) {
  MMF mmf(non_existent_file_, 0, 100);
  EXPECT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::FileOpenFailed)
      << CreateErrorMessage("Expected FileOpenFailed",
                             mmf.GetLastError(), true);
}

TEST_F(MMFTest, ConstructorPartialFileSizeBeyondEnd) {
  MMF mmf(single_line_file_, 5, 1000); // Size larger than remaining file
  EXPECT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid even with size beyond end",
                             mmf.GetLastError(), true);
  // Should map only available bytes from the offset
  auto file_size = mmf.GetFileSize().value_or(0);
  auto mapped_size = mmf.GetMappedSize().value_or(0);
  EXPECT_GE(mapped_size, file_size - 5);
}

// Move Constructor and Assignment Tests
TEST_F(MMFTest, MoveConstructor) {
  MMF mmf1(single_line_file_);
  ASSERT_TRUE(mmf1.IsValid())
      << CreateErrorMessage("MMF1 should be valid", mmf1.GetLastError(),
                             true);

  MMF mmf2(std::move(mmf1));
  EXPECT_TRUE(mmf2.IsValid())
      << CreateErrorMessage("MMF2 should be valid after move",
                             mmf2.GetLastError(), true);
  EXPECT_FALSE(mmf1.IsValid())
      << CreateErrorMessage("MMF1 should be invalid after move",
                             mmf1.GetLastError(), false);
  EXPECT_EQ(mmf2.GetFilename(), single_line_file_);
}

TEST_F(MMFTest, MoveAssignment) {
  MMF mmf1(single_line_file_);
  MMF mmf2(multi_line_file_);
  ASSERT_TRUE(mmf1.IsValid())
      << CreateErrorMessage("MMF1 should be valid", mmf1.GetLastError(),
                             true);
  ASSERT_TRUE(mmf2.IsValid())
      << CreateErrorMessage("MMF2 should be valid", mmf2.GetLastError(),
                             true);

  mmf2 = std::move(mmf1);
  EXPECT_TRUE(mmf2.IsValid())
      << CreateErrorMessage("MMF2 should be valid after move assignment",
                             mmf2.GetLastError(), true);
  EXPECT_FALSE(mmf1.IsValid())
      << CreateErrorMessage("MMF1 should be invalid after move assignment",
                             mmf1.GetLastError(), false);
  EXPECT_EQ(mmf2.GetFilename(), single_line_file_);
}

// ReadLine Tests
TEST_F(MMFTest, ReadLineSingleLine) {
  MMF mmf(single_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto line = mmf.ReadLine();
  ASSERT_TRUE(line.has_value())
      << CreateErrorMessage("ReadLine should return a value",
                             mmf.GetLastError(), false);
  EXPECT_EQ(line.value(), "Hello World");
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::None)
      << CreateErrorMessage("Expected no error after ReadLine",
                             mmf.GetLastError(), false);

  // Second read should return empty (EOF)
  auto line2 = mmf.ReadLine();
  EXPECT_FALSE(line2.has_value())
      << CreateErrorMessage("Second ReadLine should return nullopt",
                             mmf.GetLastError(), false);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile)
      << CreateErrorMessage("Expected EndOfFile", mmf.GetLastError(),
                             false);
}

TEST_F(MMFTest, ReadLineMultipleLines) {
  MMF mmf(multi_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  std::vector<std::string> expected = {"Line 1", "Line 2", "Line 3",
                                       "Line 4"};
  std::vector<std::string> actual;

  while (auto line = mmf.ReadLine()) {
    actual.push_back(line.value());
  }

  EXPECT_EQ(actual, expected);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile)
      << CreateErrorMessage("Expected EndOfFile after reading all lines",
                             mmf.GetLastError(), false);
}

TEST_F(MMFTest, ReadLineEmptyFile) {
  MMF mmf(empty_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid for empty file",
                             mmf.GetLastError(), true);

  auto line = mmf.ReadLine();
  EXPECT_FALSE(line.has_value())
      << CreateErrorMessage("ReadLine should return nullopt for empty file",
                             mmf.GetLastError(), false);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::NotMapped)
      << CreateErrorMessage("Expected NotMapped for empty file",
                             mmf.GetLastError(), false);
}

TEST_F(MMFTest, ReadLineInvalidObject) {
  MMF mmf(non_existent_file_);
  ASSERT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);

  auto line = mmf.ReadLine();
  EXPECT_FALSE(line.has_value())
      << CreateErrorMessage(
             "ReadLine should return nullopt for invalid object",
             mmf.GetLastError(), false);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::NotMapped)
      << CreateErrorMessage("Expected NotMapped", mmf.GetLastError(),
                             false);
}

TEST_F(MMFTest, ReadLineMixedLineEndings) {
  MMF mmf(mixed_endings_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  std::vector<std::string> expected = {"Unix line", "Windows line\r",
                                       "Mac line", "No ending"};
  std::vector<std::string> actual;

  while (auto line = mmf.ReadLine()) {
    actual.push_back(line.value());
  }

  EXPECT_EQ(actual, expected);
}

TEST_F(MMFTest, ReadLineLongLines) {
  MMF mmf(long_lines_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto line1 = mmf.ReadLine();
  ASSERT_TRUE(line1.has_value())
      << CreateErrorMessage("First ReadLine should return a value",
                             mmf.GetLastError(), false);
  EXPECT_EQ(line1.value().length(), 10000);
  EXPECT_EQ(line1.value(), std::string(10000, 'A'));

  auto line2 = mmf.ReadLine();
  ASSERT_TRUE(line2.has_value())
      << CreateErrorMessage("Second ReadLine should return a value",
                             mmf.GetLastError(), false);
  EXPECT_EQ(line2.value().length(), 5000);
  EXPECT_EQ(line2.value(), std::string(5000, 'B'));
}

// ReadLineView Tests
TEST_F(MMFTest, ReadLineViewSingleLine) {
  MMF mmf(single_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto line_view = mmf.ReadLineView();
  ASSERT_TRUE(line_view.has_value())
      << CreateErrorMessage("ReadLineView should return a value",
                             mmf.GetLastError(), false);
  EXPECT_EQ(line_view.value(), "Hello World");
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::None)
      << CreateErrorMessage("Expected no error after ReadLineView",
                             mmf.GetLastError(), false);

  // Second read should return empty (EOF)
  auto line_view2 = mmf.ReadLineView();
  EXPECT_FALSE(line_view2.has_value())
      << CreateErrorMessage("Second ReadLineView should return nullopt",
                             mmf.GetLastError(), false);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile)
      << CreateErrorMessage("Expected EndOfFile", mmf.GetLastError(),
                             false);
}

TEST_F(MMFTest, ReadLineViewMultipleLines) {
  MMF mmf(multi_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  std::vector<std::string> expected = {"Line 1", "Line 2", "Line 3",
                                       "Line 4"};
  std::vector<std::string> actual;

  while (auto line_view = mmf.ReadLineView()) {
    actual.emplace_back(line_view.value());
  }

  EXPECT_EQ(actual, expected);
}

TEST_F(MMFTest, ReadLineViewInvalidObject) {
  MMF mmf(non_existent_file_);
  ASSERT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);

  auto line_view = mmf.ReadLineView();
  EXPECT_FALSE(line_view.has_value())
      << CreateErrorMessage(
             "ReadLineView should return nullopt for invalid object",
             mmf.GetLastError(), false);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::NotMapped)
      << CreateErrorMessage("Expected NotMapped", mmf.GetLastError(),
                             false);
}

// Position Management Tests
TEST_F(MMFTest, GetCurrentPositionValid) {
  MMF mmf(multi_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto pos = mmf.GetCurrentPosition();
  ASSERT_TRUE(pos.has_value())
      << CreateErrorMessage("GetCurrentPosition should return a value",
                             mmf.GetLastError(), false);
  EXPECT_EQ(pos.value(), 0);

  mmf.ReadLine(); // Read first line
  pos = mmf.GetCurrentPosition();
  ASSERT_TRUE(pos.has_value())
      << CreateErrorMessage(
             "GetCurrentPosition should return a value after ReadLine",
             mmf.GetLastError(), false);
  EXPECT_GT(pos.value(), 0);
}

TEST_F(MMFTest, GetCurrentPositionInvalid) {
  MMF mmf(non_existent_file_);
  ASSERT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);

  auto pos = mmf.GetCurrentPosition();
  EXPECT_FALSE(pos.has_value())
      << CreateErrorMessage(
             "GetCurrentPosition should return nullopt for invalid object",
             mmf.GetLastError(), false);
}

TEST_F(MMFTest, SetPositionValid) {
  MMF mmf(multi_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto result = mmf.SetPosition(5);
  EXPECT_EQ(result, MMF::Error::None)
      << "SetPosition should succeed, got: " << MMFErrorToString(result);

  auto pos = mmf.GetCurrentPosition();
  ASSERT_TRUE(pos.has_value())
      << CreateErrorMessage(
             "GetCurrentPosition should return a value after SetPosition",
             mmf.GetLastError(), false);
  EXPECT_EQ(pos.value(), 5);
}

TEST_F(MMFTest, SetPositionInvalidPosition) {
  MMF mmf(single_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto mapped_size = mmf.GetMappedSize().value_or(0);
  auto result = mmf.SetPosition(mapped_size + 100);
  EXPECT_EQ(result, MMF::Error::InvalidPosition)
      << "SetPosition should fail with InvalidPosition, got: "
      << MMFErrorToString(result);
  EXPECT_EQ(mmf.GetLastError(), MMF::Error::InvalidPosition)
      << CreateErrorMessage("GetLastError should return InvalidPosition",
                             mmf.GetLastError(), false);
}

TEST_F(MMFTest, SetPositionInvalidObject) {
  MMF mmf(non_existent_file_);
  ASSERT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);

  auto result = mmf.SetPosition(0);
  EXPECT_EQ(result, MMF::Error::NotMapped)
      << "SetPosition should fail with NotMapped, got: "
      << MMFErrorToString(result);
}

TEST_F(MMFTest, ResetValid) {
  MMF mmf(multi_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  mmf.ReadLine(); // Move position
  auto result = mmf.Reset();
  EXPECT_EQ(result, MMF::Error::None)
      << "Reset should succeed, got: " << MMFErrorToString(result);

  auto pos = mmf.GetCurrentPosition();
  ASSERT_TRUE(pos.has_value())
      << CreateErrorMessage(
             "GetCurrentPosition should return a value after Reset",
             mmf.GetLastError(), false);
  EXPECT_EQ(pos.value(), 0);
}

TEST_F(MMFTest, ResetInvalidObject) {
  MMF mmf(non_existent_file_);
  ASSERT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);

  auto result = mmf.Reset();
  EXPECT_EQ(result, MMF::Error::NotMapped)
      << "Reset should fail with NotMapped, got: "
      << MMFErrorToString(result);
}

// Size and Info Tests
TEST_F(MMFTest, GetMappedSizeValid) {
  MMF mmf(single_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto size = mmf.GetMappedSize();
  ASSERT_TRUE(size.has_value())
      << CreateErrorMessage("GetMappedSize should return a value",
                             mmf.GetLastError(), false);
  EXPECT_GT(size.value(), 0);
}

TEST_F(MMFTest, GetMappedSizeInvalid) {
  MMF mmf(non_existent_file_);
  ASSERT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);

  auto size = mmf.GetMappedSize();
  EXPECT_FALSE(size.has_value())
      << CreateErrorMessage(
             "GetMappedSize should return nullopt for invalid object",
             mmf.GetLastError(), false);
}

TEST_F(MMFTest, GetFileSizeValid) {
  MMF mmf(single_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto size = mmf.GetFileSize();
  ASSERT_TRUE(size.has_value())
      << CreateErrorMessage("GetFileSize should return a value",
                             mmf.GetLastError(), false);
  EXPECT_GT(size.value(), 0);
}

TEST_F(MMFTest, GetFileSizeInvalid) {
  MMF mmf(non_existent_file_);
  ASSERT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);

  auto size = mmf.GetFileSize();
  EXPECT_FALSE(size.has_value())
      << CreateErrorMessage(
             "GetFileSize should return nullopt for invalid object",
             mmf.GetLastError(), false);
}

TEST_F(MMFTest, GetFilename) {
  MMF mmf(single_line_file_);
  EXPECT_EQ(mmf.GetFilename(), single_line_file_);

  MMF mmf_invalid(non_existent_file_);
  EXPECT_EQ(mmf_invalid.GetFilename(), non_existent_file_);
}

TEST_F(MMFTest, GetMappedOffsetFullFile) {
  MMF mmf(single_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto offset = mmf.GetMappedOffset();
  ASSERT_TRUE(offset.has_value())
      << CreateErrorMessage("GetMappedOffset should return a value",
                             mmf.GetLastError(), false);
  EXPECT_EQ(offset.value(), 0);
}

TEST_F(MMFTest, GetMappedOffsetPartialFile) {
  MMF mmf(large_file_, 100, 500);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid for partial mapping",
                             mmf.GetLastError(), true);

  auto offset = mmf.GetMappedOffset();
  ASSERT_TRUE(offset.has_value())
      << CreateErrorMessage("GetMappedOffset should return a value",
                             mmf.GetLastError(), false);
  // With our new implementation, GetMappedOffset() always returns 0
  // because the user pointer starts from the requested data
  EXPECT_EQ(offset.value(), 0);
}

// EOF Tests
TEST_F(MMFTest, IsEOFValid) {
  MMF mmf(single_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  EXPECT_FALSE(mmf.IsEOF());

  mmf.ReadLine(); // Read the only line
  EXPECT_TRUE(mmf.IsEOF());
}

TEST_F(MMFTest, IsEOFInvalid) {
  MMF mmf(non_existent_file_);
  ASSERT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);

  EXPECT_TRUE(mmf.IsEOF()); // Invalid objects should return true for EOF
}

TEST_F(MMFTest, IsEOFEmptyFile) {
  MMF mmf(empty_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid for empty file",
                             mmf.GetLastError(), true);

  EXPECT_TRUE(mmf.IsEOF()); // Empty file should be at EOF immediately
}

// GetData Tests
TEST_F(MMFTest, GetDataValid) {
  MMF mmf(single_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto data = mmf.GetData();
  ASSERT_TRUE(data.has_value())
      << CreateErrorMessage("GetData should return a value",
                             mmf.GetLastError(), false);
  EXPECT_NE(data.value(), nullptr);
}

TEST_F(MMFTest, GetDataInvalid) {
  MMF mmf(non_existent_file_);
  ASSERT_FALSE(mmf.IsValid())
      << CreateErrorMessage("MMF should be invalid for non-existent file",
                             mmf.GetLastError(), true);

  auto data = mmf.GetData();
  EXPECT_FALSE(data.has_value())
      << CreateErrorMessage(
             "GetData should return nullopt for invalid object",
             mmf.GetLastError(), false);
}

// Partial Mapping Tests
TEST_F(MMFTest, PartialMappingReadLines) {
  MMF mmf(large_file_, 0, 200); // Map first 200 bytes
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid for partial mapping",
                             mmf.GetLastError(), true);

  std::vector<std::string> lines;
  while (auto line = mmf.ReadLine()) {
    lines.push_back(line.value());
  }

  EXPECT_GT(lines.size(), 0);
  EXPECT_LT(lines.size(), 1000); // Should be less than total lines in file
}

TEST_F(MMFTest, PartialMappingMiddleOfFile) {
  size_t offset = 500;
  size_t size = 300;
  MMF mmf(large_file_, offset, size);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid for partial mapping",
                             mmf.GetLastError(), true);

  auto mapped_size = mmf.GetMappedSize();
  ASSERT_TRUE(mapped_size.has_value())
      << CreateErrorMessage("GetMappedSize should return a value",
                             mmf.GetLastError(), false);
  EXPECT_GE(mapped_size.value(), size);

  auto mapped_offset = mmf.GetMappedOffset();
  ASSERT_TRUE(mapped_offset.has_value())
      << CreateErrorMessage("GetMappedOffset should return a value",
                             mmf.GetLastError(), false);
  EXPECT_EQ(mapped_offset.value(), 0); // Always 0 with new implementation
}

// Binary File Tests
TEST_F(MMFTest, BinaryFileMapping) {
  MMF mmf(binary_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid for binary file",
                             mmf.GetLastError(), true);

  auto data = mmf.GetData();
  ASSERT_TRUE(data.has_value())
      << CreateErrorMessage("GetData should return a value for binary file",
                             mmf.GetLastError(), false);

  auto size = mmf.GetMappedSize();
  ASSERT_TRUE(size.has_value())
      << CreateErrorMessage(
             "GetMappedSize should return a value for binary file",
             mmf.GetLastError(), false);
  EXPECT_EQ(size.value(), 256);
}

// Stress Tests
TEST_F(MMFTest, MultipleReadResetCycles) {
  MMF mmf(multi_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  for (int cycle = 0; cycle < 10; ++cycle) {
    std::vector<std::string> lines;
    while (auto line = mmf.ReadLine()) {
      lines.push_back(line.value());
    }

    EXPECT_EQ(lines.size(), 4);
    EXPECT_EQ(lines[0], "Line 1");
    EXPECT_EQ(lines[3], "Line 4");

    auto result = mmf.Reset();
    EXPECT_EQ(result, MMF::Error::None)
        << "Reset should succeed in cycle " << cycle
        << ", got: " << MMFErrorToString(result);
  }
}

TEST_F(MMFTest, RandomPositionAccess) {
  MMF mmf(large_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto file_size = mmf.GetMappedSize().value_or(0);
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<size_t> dis(0, file_size - 1);

  for (int i = 0; i < 100; ++i) {
    size_t pos = dis(gen);
    auto result = mmf.SetPosition(pos);
    EXPECT_EQ(result, MMF::Error::None)
        << "SetPosition should succeed for position " << pos
        << ", got: " << MMFErrorToString(result);

    auto current_pos = mmf.GetCurrentPosition();
    ASSERT_TRUE(current_pos.has_value())
        << CreateErrorMessage(
               "GetCurrentPosition should return a value after SetPosition",
               mmf.GetLastError(), false);
    EXPECT_EQ(current_pos.value(), pos);
  }
}

// Edge Cases
TEST_F(MMFTest, ZeroSizeMapping) {
  MMF mmf(single_line_file_, 0, 0);
  EXPECT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid for zero size mapping",
                             mmf.GetLastError(), true);

  auto mapped_size = mmf.GetMappedSize();
  ASSERT_TRUE(mapped_size.has_value())
      << CreateErrorMessage(
             "GetMappedSize should return a value for zero size mapping",
             mmf.GetLastError(), false);
  EXPECT_EQ(mapped_size.value(), 0);

  EXPECT_TRUE(mmf.IsEOF());
}

TEST_F(MMFTest, LastByteOffset) {
  MMF mmf(single_line_file_);
  ASSERT_TRUE(mmf.IsValid())
      << CreateErrorMessage("MMF should be valid", mmf.GetLastError(),
                             true);

  auto file_size = mmf.GetFileSize().value_or(0);
  if (file_size > 0) {
    MMF mmf_partial(single_line_file_, file_size - 1, 1);
    EXPECT_TRUE(mmf_partial.IsValid())
        << CreateErrorMessage("MMF should be valid for last byte mapping",
                               mmf_partial.GetLastError(), true);

    auto mapped_size = mmf_partial.GetMappedSize();
    ASSERT_TRUE(mapped_size.has_value())
        << CreateErrorMessage(
               "GetMappedSize should return a value for last byte mapping",
               mmf_partial.GetLastError(), false);
    EXPECT_GE(mapped_size.value(), 1);
  }
}

// Write Tests
TEST_F(MMFTest, ReadWriteModeCreatesNewFile) {
    MMF mmf(write_test_file_, MMF::OpenMode::ReadWrite);
    ASSERT_TRUE(mmf.IsValid()) << CreateErrorMessage("Failed to create file in write mode", mmf.GetLastError(), true);

    MMF::Error write_result = mmf.WriteLine("Test line 1");
    ASSERT_EQ(write_result, MMF::Error::None)
      << CreateErrorMessage("Failed to write first line with error: ", write_result, true);

    write_result = mmf.WriteLine("Test line 2");
    ASSERT_EQ(write_result, MMF::Error::None) << "Failed to write second line";
}

TEST_F(MMFTest, ReadWriteModeWorks) {
    // First create and write to file
    {
        MMF mmf(write_test_file_, MMF::OpenMode::ReadWrite);
        ASSERT_TRUE(mmf.IsValid());
        ASSERT_EQ(mmf.WriteLine("Line 1"), MMF::Error::None);
        ASSERT_EQ(mmf.WriteLine("Line 2"), MMF::Error::None);
    }

    // Then open in read-write mode
    MMF mmf(write_test_file_, MMF::OpenMode::ReadWrite);
    ASSERT_TRUE(mmf.IsValid());

    // Read existing content
    auto line1 = mmf.ReadLine();
    ASSERT_TRUE(line1.has_value());
    ASSERT_EQ(*line1, "Line 1");

    // Append new content
    ASSERT_EQ(mmf.WriteLine("Line 3"), MMF::Error::None);
}

TEST_F(MMFTest, WriteToReadOnlyFails) {
    MMF mmf(multi_line_file_); // Default ReadOnly mode
    ASSERT_TRUE(mmf.IsValid());

    MMF::Error write_result = mmf.WriteLine("Should fail");
    ASSERT_EQ(write_result, MMF::Error::WriteError);
}

TEST_F(MMFTest, AutomaticFileGrowth) {
    MMF mmf(write_test_file_, MMF::OpenMode::ReadWrite);
    ASSERT_TRUE(mmf.IsValid());

    // Write data larger than initial mapping
    std::string large_line(8192, 'A'); // 8KB line
    ASSERT_EQ(mmf.WriteLine(large_line), MMF::Error::None);

    // Verify file size grew
    auto size = mmf.GetMappedSize();
    ASSERT_TRUE(size.has_value());
    ASSERT_GT(*size, 8192) << "File didn't grow as expected";
}

TEST_F(MMFTest, WriteModeInitialization) {
    // Test different open modes
    {
        MMF read_mmf(multi_line_file_); // Default ReadOnly
        ASSERT_TRUE(read_mmf.IsValid());
        ASSERT_EQ(read_mmf.WriteLine("test"), MMF::Error::WriteError);
    }

    {
        MMF write_mmf(write_test_file_, MMF::OpenMode::ReadWrite);
        ASSERT_TRUE(write_mmf.IsValid());
        ASSERT_EQ(write_mmf.WriteLine("test"), MMF::Error::None);
    }

    {
        MMF readwrite_mmf(write_test_file_, MMF::OpenMode::ReadWrite);
        ASSERT_TRUE(readwrite_mmf.IsValid());
        auto line = readwrite_mmf.ReadLine();
        ASSERT_TRUE(line.has_value());
        ASSERT_EQ(*line, "test");
        ASSERT_EQ(readwrite_mmf.WriteLine("new line"), MMF::Error::None);
    }
}

TEST_F(MMFTest, WriteWithOffset) {
    // Create initial content
    {
        MMF mmf(write_test_file_, MMF::OpenMode::ReadWrite);
        ASSERT_TRUE(mmf.IsValid());
        ASSERT_EQ(mmf.WriteLine("Line 1"), MMF::Error::None);
        ASSERT_EQ(mmf.WriteLine("Line 2"), MMF::Error::None);
    }

    // Open with offset in ReadWrite mode
    MMF mmf(write_test_file_, 7, 100, MMF::OpenMode::ReadWrite); // Skip "Line 1\n"
    ASSERT_TRUE(mmf.IsValid());

    auto line = mmf.ReadLine();
    ASSERT_TRUE(line.has_value());
    ASSERT_EQ(line.value(), "Line 2");
    EXPECT_EQ(mmf.GetCurrentPosition().value(), 7 + line.value().size() + 1); // +1 for '\n'
}

// Test chunked remapping in ReadLine()
TEST_F(MMFTest, ChunkedRemappingReadLine) {

  const auto page_size = sysconf(_SC_PAGE_SIZE); // 4KB pages
  int chunk_size = page_size * 10; // 40KB chunks
  constexpr int total_line = 2000;
  // Create a file with many lines, each line is short
  std::string chunked_file = test_dir_ + "/chunked.txt";
  {
    std::ofstream ofs(chunked_file);
    for (int i = 0; i < total_line; ++i) {
      ofs << "Line " << i << "\n";
    }
  }

  // Open the file with MMF in chunked mode
  MMF mmf(chunked_file, 0, chunk_size);
  ASSERT_TRUE(mmf.IsValid());

  std::vector<std::string> lines;
  while (auto line = mmf.ReadLine(true)) {
    lines.push_back(line.value());
    //TODO:: Add tests checks here to verify chunked remapping
  }

  // Should read all 200 lines
  ASSERT_EQ(lines.size(), total_line )
    << CreateErrorMessage(
      "Failed to create file in write mode",
      mmf.GetLastError(),
      true);

  ASSERT_LE(mmf.GetMappedSize().value_or(0), chunk_size);
  for (int i = 0; i < total_line; ++i) {
    ASSERT_EQ(lines[i], "Line " + std::to_string(i));
  }
  ASSERT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile);
}

// Test chunked remapping with lines split across chunk boundaries
TEST_F(MMFTest, ChunkedRemappingSplitLine) {
    const auto page_size = sysconf(_SC_PAGE_SIZE);
    int chunk_size = page_size * 2;
    std::string file = test_dir_ + "/split_line.txt";
    {
        std::ofstream ofs(file);
        ofs << std::string(chunk_size - 1, 'A'); // Fill almost one chunk
        ofs << "\nB\n"; // Line break at chunk boundary, then another line
    }
    MMF mmf(file, 0, chunk_size);
    ASSERT_TRUE(mmf.IsValid());

    auto line1 = mmf.ReadLine(true);
    ASSERT_TRUE(line1.has_value());
    ASSERT_EQ(line1->size(), chunk_size - 1);

    auto line2 = mmf.ReadLine(true);
    ASSERT_TRUE(line2.has_value());
    ASSERT_EQ(*line2, "B");

    auto line3 = mmf.ReadLine(true);
    ASSERT_FALSE(line3.has_value());
    ASSERT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile);
}

// Test chunked remapping with ReadLineView
TEST_F(MMFTest, ChunkedRemappingReadLineView) {
    const auto page_size = sysconf(_SC_PAGE_SIZE);
    int chunk_size = page_size * 3;
    constexpr int total_lines = 100;
    std::string file = test_dir_ + "/chunked_view.txt";
    {
        std::ofstream ofs(file);
        for (int i = 0; i < total_lines; ++i) {
            ofs << "ViewLine " << i << "\n";
        }
    }
    MMF mmf(file, 0, chunk_size);
    ASSERT_TRUE(mmf.IsValid());

    std::vector<std::string> lines;
    while (auto line = mmf.ReadLineView(true)) {
        lines.emplace_back(line.value());
    }
    ASSERT_EQ(lines.size(), total_lines);
    for (int i = 0; i < total_lines; ++i) {
        ASSERT_EQ(lines[i], "ViewLine " + std::to_string(i));
    }
    ASSERT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile);
}

// Test chunked remapping with empty lines at chunk boundaries
TEST_F(MMFTest, ChunkedRemappingEmptyLines) {
    const auto page_size = sysconf(_SC_PAGE_SIZE);
    int chunk_size = page_size;
    std::string file = test_dir_ + "/chunked_empty.txt";
    {
        std::ofstream ofs(file);
        ofs << std::string(chunk_size - 1, 'X') << "\n\nY\n";
    }
    MMF mmf(file, 0, chunk_size);
    ASSERT_TRUE(mmf.IsValid());

    auto line1 = mmf.ReadLine(true);
    ASSERT_TRUE(line1.has_value()) << CreateErrorMessage("Failed to read first line", mmf.GetLastError(), true);
    ASSERT_EQ(line1->size(), chunk_size - 1);

    auto line2 = mmf.ReadLine(true);
    ASSERT_TRUE(line2.has_value()) << CreateErrorMessage("Failed to read second line", mmf.GetLastError(), true);
    ASSERT_EQ(*line2, "");

    auto line3 = mmf.ReadLine(true);
    ASSERT_TRUE(line3.has_value());
    ASSERT_EQ(*line3, "Y");

    auto line4 = mmf.ReadLine(true);
    ASSERT_FALSE(line4.has_value());
    ASSERT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile);
}

// Test chunked remapping with empty lines at chunk boundaries
TEST_F(MMFTest, ChunkedRemappingEmptyLinesViews) {
  const auto page_size = sysconf(_SC_PAGE_SIZE);
  int chunk_size = page_size;
  std::string file = test_dir_ + "/chunked_empty.txt";
  {
    std::ofstream ofs(file);
    ofs << std::string(chunk_size - 1, 'X') << "\n\nY\n";
  }
  MMF mmf(file, 0, chunk_size);
  ASSERT_TRUE(mmf.IsValid());

  auto line1 = mmf.ReadLineView(true);
  ASSERT_TRUE(line1.has_value()) << CreateErrorMessage("Failed to read first line", mmf.GetLastError(), true);
  ASSERT_EQ(line1->size(), chunk_size - 1);

  auto line2 = mmf.ReadLineView(true);
  ASSERT_TRUE(line2.has_value()) << CreateErrorMessage("Failed to read second line", mmf.GetLastError(), true);
  ASSERT_EQ(*line2, "");

  auto line3 = mmf.ReadLineView(true);
  ASSERT_TRUE(line3.has_value());
  ASSERT_EQ(*line3, "Y");

  auto line4 = mmf.ReadLineView(true);
  ASSERT_FALSE(line4.has_value());
  ASSERT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile);
}

// Test ReadLineView with empty lines at chunk boundaries
TEST_F(MMFTest, ChunkedRemappingEmptyLinesView) {
    const auto page_size = sysconf(_SC_PAGE_SIZE);
    int chunk_size = page_size;
    std::string file = test_dir_ + "/chunked_empty_view.txt";
    {
        std::ofstream ofs(file);
        ofs << std::string(chunk_size - 1, 'X') << "\n\nY\n";
    }
    MMF mmf(file, 0, chunk_size);
    ASSERT_TRUE(mmf.IsValid());

    auto line1 = mmf.ReadLineView(true);
    ASSERT_TRUE(line1.has_value()) << CreateErrorMessage("Failed to read first line", mmf.GetLastError(), true);
    ASSERT_EQ(line1->size(), chunk_size - 1);

    auto line2 = mmf.ReadLineView(true);
    ASSERT_TRUE(line2.has_value()) << CreateErrorMessage("Failed to read second line", mmf.GetLastError(), true);
    ASSERT_EQ(line2->size(), 0);

    auto line3 = mmf.ReadLineView(true);
    ASSERT_TRUE(line3.has_value());
    ASSERT_EQ(*line3, "Y");

    auto line4 = mmf.ReadLineView(true);
    ASSERT_FALSE(line4.has_value());
    ASSERT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile);
}

// Test ReadLineView with lines split across chunk boundaries
TEST_F(MMFTest, ChunkedRemappingSplitLineView) {
    const auto page_size = sysconf(_SC_PAGE_SIZE);
    int chunk_size = page_size * 2;
    std::string file = test_dir_ + "/split_line_view.txt";
    {
        std::ofstream ofs(file);
        ofs << std::string(chunk_size - 1, 'A'); // Fill almost one chunk
        ofs << "\nB\n"; // Line break at chunk boundary, then another line
    }
    MMF mmf(file, 0, chunk_size);
    ASSERT_TRUE(mmf.IsValid());

    auto line1 = mmf.ReadLineView(true);
    ASSERT_TRUE(line1.has_value());
    ASSERT_EQ(line1->size(), chunk_size - 1);
    ASSERT_EQ(std::string(line1->data(), line1->size()), std::string(chunk_size - 1, 'A'));

    auto line2 = mmf.ReadLineView(true);
    ASSERT_TRUE(line2.has_value());
    ASSERT_EQ(*line2, "B");

    auto line3 = mmf.ReadLineView(true);
    ASSERT_FALSE(line3.has_value());
    ASSERT_EQ(mmf.GetLastError(), MMF::Error::EndOfFile);
}