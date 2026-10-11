#include "src/settings.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {

std::string ConfigPath() {
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

}  // namespace

Settings LoadSettings() {
  Settings settings;

  const std::string path = ConfigPath();
  std::ifstream file(path);
  if (path.empty() || !file) {
    return settings;
  }

  std::string line;
  for (int number = 1; std::getline(file, line); ++number) {
    const std::string text = Trim(line);
    if (text.empty() || text[0] == '#') {
      continue;
    }

    const size_t equals = text.find('=');
    const bool ok =
        equals != std::string::npos &&
        ApplySetting(Trim(text.substr(0, equals)),
                     Trim(text.substr(equals + 1)), &settings);
    if (!ok) {
      std::cerr << path << ":" << number << ": ignored line: " << text << "\n";
    }
  }
  return settings;
}
