#include "src/downloads.h"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

std::string DownloadsDirectory() {
  const char* home = std::getenv("HOME");
  if (!home || !*home) {
    return "";
  }
  const fs::path directory = fs::path(home) / "Downloads";
  std::error_code ec;
  fs::create_directories(directory, ec);
  return ec ? "" : directory.string();
}

std::string UniqueDownloadPath(const std::string& directory,
                               const std::string& suggested_name) {
  // Only the file name is kept, so a name cannot point outside the folder.
  std::string name = suggested_name;
  const size_t slash = name.find_last_of('/');
  if (slash != std::string::npos) {
    name = name.substr(slash + 1);
  }
  if (name.empty() || name == "." || name == "..") {
    name = "download";
  }

  const fs::path base(name);
  const std::string stem = base.stem().string();
  const std::string extension = base.extension().string();

  fs::path candidate = fs::path(directory) / name;
  for (int copy = 1; fs::exists(candidate); ++copy) {
    candidate = fs::path(directory) /
                (stem + " (" + std::to_string(copy) + ")" + extension);
  }
  return candidate.string();
}
