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

#endif  // PAWNET_SRC_SETTINGS_H_
