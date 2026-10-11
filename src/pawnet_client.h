#ifndef PAWNET_SRC_PAWNET_CLIENT_H_
#define PAWNET_SRC_PAWNET_CLIENT_H_

#include "include/cef_client.h"
#include "include/cef_display_handler.h"
#include "include/cef_life_span_handler.h"

class BrowserWindow;

// Handles the browser of a single tab and reports its events to the window.
class PawNetClient : public CefClient,
                     public CefLifeSpanHandler,
                     public CefDisplayHandler {
 public:
  // |window| owns the tabs, so it must outlive every client it creates.
  PawNetClient(BrowserWindow* window, int tab_id);

  // CefClient:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

  // CefLifeSpanHandler:
  bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     int popup_id,
                     const CefString& target_url,
                     const CefString& target_frame_name,
                     WindowOpenDisposition target_disposition,
                     bool user_gesture,
                     const CefPopupFeatures& popupFeatures,
                     CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient>& client,
                     CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue>& extra_info,
                     bool* no_javascript_access) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefDisplayHandler:
  void OnAddressChange(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const CefString& url) override;
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

 private:
  BrowserWindow* const window_;
  const int tab_id_;

  IMPLEMENT_REFCOUNTING(PawNetClient);
};

#endif  // PAWNET_SRC_PAWNET_CLIENT_H_
