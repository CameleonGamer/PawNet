#include "include/cef_app.h"
#include "src/pawnet_app.h"

// Entry point shared by every CEF process: the browser process, plus the
// renderer, GPU and utility helpers that CEF launches from this same binary.
int main(int argc, char* argv[]) {
  CefMainArgs main_args(argc, argv);

  // The app is passed to every process, so helper processes also register the
  // custom schemes.
  CefRefPtr<PawNetApp> app(new PawNetApp);

  // Helper processes are handled here and exit with a non-negative code.
  // Returns -1 for the browser process, which continues below.
  const int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
  if (exit_code >= 0) {
    return exit_code;
  }

  CefSettings settings;

  // The sandbox needs a SUID helper; it is off until the build enables it.
#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
    return CefGetExitCode();
  }

  CefRunMessageLoop();
  CefShutdown();
  return 0;
}
