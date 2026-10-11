#ifndef PAWNET_SRC_PAWNET_APP_H_
#define PAWNET_SRC_PAWNET_APP_H_

#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"

// Application object for the browser process. CEF calls
// OnContextInitialized() once the engine is ready, which is when we open the
// main window.
class PawNetApp : public CefApp, public CefBrowserProcessHandler {
 public:
  PawNetApp() = default;

  // CefApp:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  // CefBrowserProcessHandler:
  void OnContextInitialized() override;

 private:
  IMPLEMENT_REFCOUNTING(PawNetApp);
};

#endif  // PAWNET_SRC_PAWNET_APP_H_
