#ifndef PAWNET_SRC_SETTINGS_H_
#define PAWNET_SRC_SETTINGS_H_

#include <string>
#include <vector>

#include "include/internal/cef_types.h"

// A favourite shown on the home page.
struct Shortcut {
  std::string name;
  std::string url;
};

// Where the home page content sits: centred in the page, or at the top.
enum class HomeLayout { kCentered, kTop };

// User settings, read from a plain text file at startup. Keys that are missing
// keep their defaults. Lines that cannot be used are reported and skipped.
struct Settings {
  std::string home_page = "pawnet://home";
  // Address that the search query is appended to.
  std::string search_url = "https://duckduckgo.com/?q=";
  // Any CSS background value, used by the home page.
  std::string home_background = "#16181D";
  // "#RRGGBB". Used for the logo, the search focus ring and the shortcut badges.
  std::string accent_color = "#FF7F62";
  HomeLayout home_layout = HomeLayout::kCentered;
  // Greeting and clock above the search field.
  bool home_greeting = true;
  // Replaced entirely by the first "shortcut" line in the file.
  std::vector<Shortcut> shortcuts = {
      {"Wikipedia", "https://www.wikipedia.org/"},
      {"GitHub", "https://github.com/"},
  };
  cef_color_t toolbar_color = 0xFFF5F5F5;
  cef_color_t active_tab_color = 0xFF1A1A1A;
  cef_color_t inactive_tab_color = 0xFF8A8A8A;
};

// Reads $XDG_CONFIG_HOME/pawnet/config, or ~/.config/pawnet/config when
// XDG_CONFIG_HOME is not set. Returns the defaults if there is no file.
Settings LoadSettings();

// Sets one setting from its text form, as the file has it. Returns false, and
// leaves |settings| unchanged, when the key is unknown or the value is invalid.
// The "shortcut" key is not handled here; see ParseShortcut().
bool SetSetting(const std::string& key,
                const std::string& value,
                Settings* settings);

// Parses "Name | https://address". Both parts are required.
bool ParseShortcut(const std::string& text, Shortcut* shortcut);

// "#RRGGBB" for the opaque colour |color|.
std::string ColorToHex(cef_color_t color);

// Path of the configuration file, or "" when no home directory is known.
std::string SettingsPath();

// Writes |settings| to the configuration file. Comments and lines for unknown
// keys are kept where they are. On failure returns false and sets |error|.
bool SaveSettings(const Settings& settings, std::string* error);

#endif  // PAWNET_SRC_SETTINGS_H_
