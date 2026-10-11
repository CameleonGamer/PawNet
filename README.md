# PawNet
A fresh and 100% easy customisable open source browser made with little paws

PawNet is built on the [Chromium Embedded Framework (CEF)](https://bitbucket.org/chromiumembedded/cef):
the rendering engine is Chromium, and PawNet provides the window, the address bar and the tabs.

## Status

Early prototype:

- [x] A main window that loads a page
- [x] Tabs (open, switch, close) and an address bar (URLs, bare hosts and search)
- [x] Fullscreen requested by web pages
- [x] Confirmation of unsaved pages (`beforeunload`) when closing the window
- [x] Configuration file (home page, search engine, colors)
- [ ] Settings page (`pawnet://settings`)
- [ ] Downloads, history, bookmarks
- [ ] Packaging

Known limitations of the prototype:

- Closing a single tab with the `x` button does not show the page's `beforeunload` prompt.
- The tab strip does not scroll: with many tabs, the rightmost ones are clipped.

## Building (Linux x64)

1. Download a **stable** CEF binary distribution for Linux 64-bit from
   <https://cef-builds.spotifycdn.com/index.html> and unpack it, for example to `~/cef/sdk`.
2. Install the build dependencies (Debian/Ubuntu):

   ```sh
   sudo apt-get install build-essential cmake libgtk-3-dev libnss3-dev libasound2-dev \
     libxss-dev libxtst-dev libgbm-dev libdrm-dev
   ```

3. Configure and build:

   ```sh
   cmake -S . -B build -DCEF_ROOT=$HOME/cef/sdk -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j"$(nproc)"
   ```

The first build also compiles CEF's C++ wrapper and takes a while.

## Configuration

PawNet reads `$XDG_CONFIG_HOME/pawnet/config` (or `~/.config/pawnet/config`) at startup.
Each line is `key = value`; lines starting with `#` are comments. Missing keys keep their defaults,
and unknown or invalid lines are reported on stderr and skipped.

| Key                  | Default                       | Meaning                                    |
|----------------------|-------------------------------|--------------------------------------------|
| `home_page`          | `https://duckduckgo.com/`     | Page opened in the first tab and by `+`    |
| `search_url`         | `https://duckduckgo.com/?q=`  | Search query is appended to this address   |
| `toolbar_color`      | `#F5F5F5`                     | Background of the toolbar and tab strip    |
| `active_tab_color`   | `#1A1A1A`                     | Text color of the active tab               |
| `inactive_tab_color` | `#8A8A8A`                     | Text color of the other tabs               |

Example:

```
home_page = https://example.org/
search_url = https://example.org/search?q=
toolbar_color = #CCE5FF
```

## Running

```sh
./build/pawnet                      # opens the default start page
./build/pawnet --url=about:blank    # opens a given address instead
```

The CEF binaries are copied next to the executable during the build, so `build/` is self-contained.
The CEF sandbox is disabled by default. Enable it with `-DUSE_SANDBOX=ON`, which also needs
`chrome-sandbox` to be owned by root with the SUID bit set (the build does this).
