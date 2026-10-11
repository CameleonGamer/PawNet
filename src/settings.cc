#include "src/settings.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <utility>

namespace {

std::string Trim(const std::string& text) {
  const size_t first = text.find_first_not_of(" \t\r");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = text.find_last_not_of(" \t\r");
  return text.substr(first, last - first + 1);
}

// Parses "#RRGGBB" into an opaque color.
bool ParseColor(const std::string& text, cef_color_t* color) {
  if (text.size() != 7 || text[0] != '#') {
    return false;
  }
  for (size_t i = 1; i < text.size(); ++i) {
    if (!std::isxdigit(static_cast<unsigned char>(text[i]))) {
      return false;
    }
  }
  *color = 0xFF000000 | static_cast<cef_color_t>(
                            std::stoul(text.substr(1), nullptr, 16));
  return true;
}

// Applies one "key = value" pair. Returns false for unknown keys or bad values.
bool ApplySetting(const std::string& key,
                  const std::string& value,
                  Settings* settings) {
  if (key == "home_page") {
    settings->home_page = value;
  } else if (key == "search_url") {
    settings->search_url = value;
  } else if (key == "home_background") {
    settings->home_background = value;
  } else if (key == "accent_color") {
    cef_color_t ignored;
    if (!ParseColor(value, &ignored)) {
      return false;
    }
    settings->accent_color = value;
  } else if (key == "home_layout") {
    if (value == "centered") {
      settings->home_layout = HomeLayout::kCentered;
    } else if (value == "top") {
      settings->home_layout = HomeLayout::kTop;
    } else {
      return false;
    }
  } else if (key == "home_greeting") {
    if (value == "on") {
      settings->home_greeting = true;
    } else if (value == "off") {
      settings->home_greeting = false;
    } else {
      return false;
    }
  } else if (key == "toolbar_color") {
    return ParseColor(value, &settings->toolbar_color);
  } else if (key == "active_tab_color") {
    return ParseColor(value, &settings->active_tab_color);
  } else if (key == "inactive_tab_color") {
    return ParseColor(value, &settings->inactive_tab_color);
  } else {
    return false;
  }
  return !value.empty();
}

// The file lines for the shortcuts. An empty list is written as "shortcut =",
// so that the defaults are not restored on the next start.
std::vector<std::string> ShortcutLines(const std::vector<Shortcut>& shortcuts) {
  if (shortcuts.empty()) {
    return {"shortcut ="};
  }
  std::vector<std::string> lines;
  for (const Shortcut& shortcut : shortcuts) {
    lines.push_back("shortcut = " + shortcut.name + " | " + shortcut.url);
  }
  return lines;
}

}  // namespace

std::string SettingsPath() {
  const char* config_home = std::getenv("XDG_CONFIG_HOME");
  if (config_home && *config_home) {
    return std::string(config_home) + "/pawnet/config";
  }
  const char* home = std::getenv("HOME");
  if (home && *home) {
    return std::string(home) + "/.config/pawnet/config";
  }
  return "";
}

Settings LoadSettings() {
  Settings settings;

  const std::string path = SettingsPath();
  std::ifstream file(path);
  if (path.empty() || !file) {
    return settings;
  }

  // The first shortcut line replaces the default shortcuts. "shortcut =" with
  // no value leaves the home page without shortcuts.
  bool shortcuts_configured = false;

  std::string line;
  for (int number = 1; std::getline(file, line); ++number) {
    const std::string text = Trim(line);
    if (text.empty() || text[0] == '#') {
      continue;
    }

    const size_t equals = text.find('=');
    if (equals == std::string::npos) {
      std::cerr << path << ":" << number << ": ignored line: " << text << "\n";
      continue;
    }
    const std::string key = Trim(text.substr(0, equals));
    const std::string value = Trim(text.substr(equals + 1));

    bool ok;
    if (key == "shortcut") {
      if (!shortcuts_configured) {
        settings.shortcuts.clear();
        shortcuts_configured = true;
      }
      Shortcut shortcut;
      ok = value.empty() || ParseShortcut(value, &shortcut);
      if (ok && !value.empty()) {
        settings.shortcuts.push_back(shortcut);
      }
    } else {
      ok = ApplySetting(key, value, &settings);
    }

    if (!ok) {
      std::cerr << path << ":" << number << ": ignored line: " << text << "\n";
    }
  }
  return settings;
}

bool SetSetting(const std::string& key,
                const std::string& value,
                Settings* settings) {
  Settings updated = *settings;
  if (!ApplySetting(key, value, &updated)) {
    return false;
  }
  *settings = updated;
  return true;
}

bool ParseShortcut(const std::string& text, Shortcut* shortcut) {
  const size_t bar = text.find('|');
  if (bar == std::string::npos) {
    return false;
  }
  shortcut->name = Trim(text.substr(0, bar));
  shortcut->url = Trim(text.substr(bar + 1));
  return !shortcut->name.empty() && !shortcut->url.empty();
}

std::string ColorToHex(cef_color_t color) {
  char buffer[8];
  std::snprintf(buffer, sizeof(buffer), "#%06X",
                static_cast<unsigned>(color & 0xFFFFFF));
  return buffer;
}

bool SaveSettings(const Settings& settings, std::string* error) {
  const std::string path = SettingsPath();
  if (path.empty()) {
    *error = "No home directory is known, so the settings cannot be saved.";
    return false;
  }

  const std::vector<std::pair<std::string, std::string>> values = {
      {"home_page", settings.home_page},
      {"search_url", settings.search_url},
      {"home_background", settings.home_background},
      {"accent_color", settings.accent_color},
      {"home_layout",
       settings.home_layout == HomeLayout::kTop ? "top" : "centered"},
      {"home_greeting", settings.home_greeting ? "on" : "off"},
      {"toolbar_color", ColorToHex(settings.toolbar_color)},
      {"active_tab_color", ColorToHex(settings.active_tab_color)},
      {"inactive_tab_color", ColorToHex(settings.inactive_tab_color)},
  };
  const std::vector<std::string> shortcut_lines =
      ShortcutLines(settings.shortcuts);

  std::set<std::string> managed = {"shortcut"};
  for (const auto& [key, value] : values) {
    managed.insert(key);
  }

  // Walk the existing file so comments and unknown lines stay in place. The
  // first line of each managed key is replaced; later duplicates are dropped.
  std::vector<std::string> out;
  std::set<std::string> written;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    const std::string text = Trim(line);
    const size_t equals = text.find('=');
    if (text.empty() || text[0] == '#' || equals == std::string::npos) {
      out.push_back(line);
      continue;
    }
    const std::string key = Trim(text.substr(0, equals));
    if (!managed.count(key)) {
      out.push_back(line);
      continue;
    }
    if (written.count(key)) {
      continue;
    }
    written.insert(key);
    if (key == "shortcut") {
      out.insert(out.end(), shortcut_lines.begin(), shortcut_lines.end());
      continue;
    }
    for (const auto& [name, value] : values) {
      if (name == key) {
        out.push_back(key + " = " + value);
      }
    }
  }
  in.close();

  for (const auto& [key, value] : values) {
    if (!written.count(key)) {
      out.push_back(key + " = " + value);
    }
  }
  if (!written.count("shortcut")) {
    out.insert(out.end(), shortcut_lines.begin(), shortcut_lines.end());
  }

  std::error_code ec;
  const std::filesystem::path file(path);
  std::filesystem::create_directories(file.parent_path(), ec);
  if (ec) {
    *error = "Cannot create the settings folder: " + ec.message();
    return false;
  }

  // Write to a temporary file first, so a failed write cannot empty the config.
  const std::filesystem::path temporary = path + ".tmp";
  {
    std::ofstream file_out(temporary, std::ios::trunc);
    if (!file_out) {
      *error = "Cannot write " + temporary.string();
      return false;
    }
    for (const std::string& written_line : out) {
      file_out << written_line << "\n";
    }
  }
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    *error = "Cannot replace " + path + ": " + ec.message();
    return false;
  }
  return true;
}
