#include "src/bookmarks.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>

#include "src/settings.h"

namespace {

std::mutex& FileMutex() {
  static std::mutex mutex;
  return mutex;
}

// ---- A minimal JSON reader and writer, for the array of objects we store. ----

std::string EscapeJson(const std::string& text) {
  std::string out;
  for (const char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buffer[8];
          std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
          out += buffer;
        } else {
          out += c;
        }
    }
  }
  return out;
}

void AppendUtf8(std::string* out, unsigned int code_point) {
  if (code_point < 0x80) {
    *out += static_cast<char>(code_point);
  } else if (code_point < 0x800) {
    *out += static_cast<char>(0xC0 | (code_point >> 6));
    *out += static_cast<char>(0x80 | (code_point & 0x3F));
  } else if (code_point < 0x10000) {
    *out += static_cast<char>(0xE0 | (code_point >> 12));
    *out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    *out += static_cast<char>(0x80 | (code_point & 0x3F));
  } else {
    *out += static_cast<char>(0xF0 | (code_point >> 18));
    *out += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
    *out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    *out += static_cast<char>(0x80 | (code_point & 0x3F));
  }
}

// Reads `[ {"key": "value", ...}, ... ]`. The build has exceptions disabled,
// so errors set |ok_| to false and every step checks it.
class JsonReader {
 public:
  explicit JsonReader(const std::string& text) : text_(text) {}

  bool ReadObjects(std::vector<std::map<std::string, std::string>>* objects) {
    objects->clear();
    if (!Expect('[')) {
      return false;
    }
    SkipSpace();
    if (Peek() == ']') {
      ++pos_;
      return ok_ && AtEnd();
    }
    while (ok_) {
      std::map<std::string, std::string> object;
      if (!ReadObject(&object)) {
        return false;
      }
      objects->push_back(object);
      SkipSpace();
      const char next = Take();
      if (next == ']') {
        break;
      }
      if (next != ',') {
        return false;
      }
    }
    SkipSpace();
    return ok_ && AtEnd();
  }

 private:
  bool ReadObject(std::map<std::string, std::string>* object) {
    if (!Expect('{')) {
      return false;
    }
    SkipSpace();
    if (Peek() == '}') {
      ++pos_;
      return ok_;
    }
    while (ok_) {
      SkipSpace();
      std::string key;
      std::string value;
      if (!ReadString(&key)) {
        return false;
      }
      SkipSpace();
      if (!Expect(':')) {
        return false;
      }
      SkipSpace();
      if (!ReadString(&value)) {
        return false;
      }
      (*object)[key] = value;
      SkipSpace();
      const char next = Take();
      if (next == '}') {
        return ok_;
      }
      if (next != ',') {
        return false;
      }
    }
    return false;
  }

  bool ReadString(std::string* out) {
    if (!Expect('"')) {
      return false;
    }
    while (ok_) {
      const char c = Take();
      if (c == '"') {
        return ok_;
      }
      if (c != '\\') {
        *out += c;
        continue;
      }
      const char escape = Take();
      switch (escape) {
        case '"':
        case '\\':
        case '/':
          *out += escape;
          break;
        case 'n':
          *out += '\n';
          break;
        case 'r':
          *out += '\r';
          break;
        case 't':
          *out += '\t';
          break;
        case 'b':
          *out += '\b';
          break;
        case 'f':
          *out += '\f';
          break;
        case 'u': {
          unsigned int code_point = 0;
          if (!ReadHex4(&code_point)) {
            return false;
          }
          AppendUtf8(out, code_point);
          break;
        }
        default:
          ok_ = false;
      }
    }
    return false;
  }

  bool ReadHex4(unsigned int* value) {
    *value = 0;
    for (int i = 0; i < 4 && ok_; ++i) {
      const char c = Take();
      *value <<= 4;
      if (c >= '0' && c <= '9') {
        *value |= c - '0';
      } else if (c >= 'a' && c <= 'f') {
        *value |= c - 'a' + 10;
      } else if (c >= 'A' && c <= 'F') {
        *value |= c - 'A' + 10;
      } else {
        ok_ = false;
      }
    }
    return ok_;
  }

  void SkipSpace() {
    while (pos_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
  }

  bool AtEnd() const { return pos_ == text_.size(); }

  char Peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

  char Take() {
    if (pos_ >= text_.size()) {
      ok_ = false;
      return '\0';
    }
    return text_[pos_++];
  }

  bool Expect(char expected) {
    if (Take() != expected) {
      ok_ = false;
    }
    return ok_;
  }

  const std::string& text_;
  size_t pos_ = 0;
  bool ok_ = true;
};

// Reads the file. A missing file is an empty list, not an error.
bool ReadBookmarks(std::vector<Bookmark>* bookmarks, std::string* error) {
  bookmarks->clear();
  const std::string path = BookmarksPath();
  if (path.empty()) {
    return true;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return true;
  }
  const std::string text((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
  std::vector<std::map<std::string, std::string>> objects;
  if (!JsonReader(text).ReadObjects(&objects)) {
    *error = path + " cannot be read: it is not a list of bookmarks.";
    return false;
  }
  for (const auto& object : objects) {
    const auto url = object.find("url");
    if (url == object.end() || url->second.empty()) {
      continue;
    }
    const auto title = object.find("title");
    bookmarks->push_back(
        {title == object.end() ? url->second : title->second, url->second});
  }
  return true;
}

bool WriteBookmarks(const std::vector<Bookmark>& bookmarks, std::string* error) {
  const std::string path = BookmarksPath();
  if (path.empty()) {
    *error = "No home directory is known, so bookmarks cannot be saved.";
    return false;
  }

  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(),
                                      ec);
  if (ec) {
    *error = "Cannot create the settings folder: " + ec.message();
    return false;
  }

  std::string text = "[\n";
  for (size_t i = 0; i < bookmarks.size(); ++i) {
    text += "  {\"title\": \"" + EscapeJson(bookmarks[i].title) +
            "\", \"url\": \"" + EscapeJson(bookmarks[i].url) + "\"}";
    text += i + 1 < bookmarks.size() ? ",\n" : "\n";
  }
  text += "]\n";

  // Write to a temporary file and rename it, so a failed write keeps the old list.
  const std::string temporary = path + ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) {
      *error = "Cannot write " + temporary;
      return false;
    }
    out << text;
  }
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    *error = "Cannot replace " + path + ": " + ec.message();
    return false;
  }
  return true;
}

}  // namespace

std::string BookmarksPath() {
  const std::string settings_path = SettingsPath();
  if (settings_path.empty()) {
    return "";
  }
  return (std::filesystem::path(settings_path).parent_path() / "bookmarks.json")
      .string();
}

std::vector<Bookmark> LoadBookmarks() {
  std::lock_guard<std::mutex> lock(FileMutex());
  std::vector<Bookmark> bookmarks;
  std::string error;
  if (!ReadBookmarks(&bookmarks, &error)) {
    std::cerr << error << "\n";
  }
  return bookmarks;
}

bool IsBookmarked(const std::string& url) {
  for (const Bookmark& bookmark : LoadBookmarks()) {
    if (bookmark.url == url) {
      return true;
    }
  }
  return false;
}

bool ToggleBookmark(const std::string& title,
                    const std::string& url,
                    bool* bookmarked,
                    std::string* error) {
  std::lock_guard<std::mutex> lock(FileMutex());
  std::vector<Bookmark> bookmarks;
  if (!ReadBookmarks(&bookmarks, error)) {
    return false;
  }

  const auto existing =
      std::find_if(bookmarks.begin(), bookmarks.end(),
                   [&url](const Bookmark& b) { return b.url == url; });
  if (existing != bookmarks.end()) {
    bookmarks.erase(existing);
    *bookmarked = false;
  } else {
    bookmarks.push_back({title.empty() ? url : title, url});
    *bookmarked = true;
  }
  return WriteBookmarks(bookmarks, error);
}

bool RemoveBookmark(const std::string& url, std::string* error) {
  std::lock_guard<std::mutex> lock(FileMutex());
  std::vector<Bookmark> bookmarks;
  if (!ReadBookmarks(&bookmarks, error)) {
    return false;
  }
  bookmarks.erase(std::remove_if(bookmarks.begin(), bookmarks.end(),
                                 [&url](const Bookmark& b) {
                                   return b.url == url;
                                 }),
                  bookmarks.end());
  return WriteBookmarks(bookmarks, error);
}
