#include "wallpaper/async_file_writer.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "test_support.h"

using wallpaper::AsyncFileWriter;

namespace {

std::string ReadAll(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return {};
  }
  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return data;
}

std::filesystem::path TempPath(const std::string& name) {
  return std::filesystem::temp_directory_path() / name;
}

}  // namespace

TEST_CASE(AsyncFileWriter_WritesAndFlushes) {
  const auto path = TempPath("async_file_writer_flush.txt");
  std::error_code ec;
  std::filesystem::remove(path, ec);

  AsyncFileWriter writer(8, /*startWorker=*/false);
  EXPECT_TRUE(writer.Enqueue(AsyncFileWriter::Task{path, false, std::string("first\n")}));
  EXPECT_TRUE(writer.Enqueue(AsyncFileWriter::Task{path, true, std::string("second\n")}));
  writer.FlushAndStop();

  const std::string content = ReadAll(path);
  EXPECT_EQ(content, "first\nsecond\n");
  EXPECT_EQ(writer.dropped_count(), static_cast<std::size_t>(0));
  EXPECT_EQ(writer.failure_count(), static_cast<std::size_t>(0));
}

TEST_CASE(AsyncFileWriter_DropsOldestWhenFull) {
  const auto path = TempPath("async_file_writer_drop.txt");
  std::error_code ec;
  std::filesystem::remove(path, ec);

  AsyncFileWriter writer(1, /*startWorker=*/false);
  EXPECT_TRUE(writer.Enqueue(AsyncFileWriter::Task{path, false, std::string("keep\n")}));
  EXPECT_TRUE(writer.Enqueue(AsyncFileWriter::Task{path, true, std::string("new\n")}));
  writer.FlushAndStop();

  const std::string content = ReadAll(path);
  EXPECT_EQ(content, "new\n");
  EXPECT_EQ(writer.dropped_count(), static_cast<std::size_t>(1));
  EXPECT_EQ(writer.failure_count(), static_cast<std::size_t>(0));
}
