# PawNet
A fresh and 100% easy customisable open source browser made with little paws

PawNet is built on the [Chromium Embedded Framework (CEF)](https://bitbucket.org/chromiumembedded/cef):
the rendering engine is Chromium, and PawNet provides the window, the address bar and the tabs.

## Status

Early prototype:

- [x] A main window that loads a page
- [x] Tabs (open, switch, close) and an address bar (URLs, bare hosts and search)
- [ ] Settings page driven by a configuration file
- [ ] Downloads, history, bookmarks
- [ ] Packaging

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

## Running

```sh
./build/pawnet                      # opens the default start page
./build/pawnet --url=about:blank    # opens a given address instead
```

The CEF binaries are copied next to the executable during the build, so `build/` is self-contained.
The CEF sandbox is disabled by default. Enable it with `-DUSE_SANDBOX=ON`, which also needs
`chrome-sandbox` to be owned by root with the SUID bit set (the build does this).
