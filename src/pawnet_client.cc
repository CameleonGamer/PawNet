#include "src/pawnet_client.h"

#include <string>

#include "include/cef_task.h"
#include "src/browser_window.h"
#include "src/downloads.h"

namespace {

// Opens a new tab from the UI thread. Popups are posted rather than handled
// inline so the window is not changed while CEF is still creating a browser.
class OpenTabTask : public CefTask {
 public:
  OpenTabTask(BrowserWindow* window, const std::string& url)
      : window_(window), url_(url) {}

  void Execute() override { window_->OpenTab(url_); }

 private:
  BrowserWindow* const window_;
  const std::string url_;

  IMPLEMENT_REFCOUNTING(OpenTabTask);
};

}  // namespace

PawNetClient::PawNetClient(BrowserWindow* window, int tab_id)
    : window_(window), tab_id_(tab_id) {}

bool PawNetClient::OnBeforePopup(CefRefPtr<CefBrowser> browser,
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
                                 bool* no_javascript_access) {
  // Open links such as target="_blank" in a tab instead of a native window.
  CefPostTask(TID_UI, new OpenTabTask(window_, target_url.ToString()));
  return true;  // Cancel the native popup.
}

void PawNetClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  window_->OnBrowserClosed();
}

void PawNetClient::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   const CefString& url) {
  // Subframes (ads, iframes) must not replace the address bar.
  if (frame->IsMain()) {
    window_->OnAddressChanged(tab_id_, url.ToString());
  }
}

void PawNetClient::OnLoadStart(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               TransitionType transition_type) {
  if (frame->IsMain()) {
    window_->OnAddressChanged(tab_id_, frame->GetURL().ToString());
  }
}

void PawNetClient::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                 const CefString& title) {
  window_->OnTitleChanged(tab_id_, title.ToString());
}

void PawNetClient::OnFullscreenModeChange(CefRefPtr<CefBrowser> browser,
                                          bool fullscreen) {
  // Required with Alloy style: pages such as video players request fullscreen.
  window_->OnFullscreenModeChanged(fullscreen);
}

bool PawNetClient::OnBeforeDownload(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    const CefString& suggested_name,
    CefRefPtr<CefBeforeDownloadCallback> callback) {
  // Save straight into the downloads folder, without asking where to put it.
  const std::string directory = DownloadsDirectory();
  callback->Continue(UniqueDownloadPath(directory, suggested_name.ToString()),
                     /*show_dialog=*/false);
  window_->OnDownloadUpdated(download_item);
  return true;
}

void PawNetClient::OnDownloadUpdated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    CefRefPtr<CefDownloadItemCallback> callback) {
  window_->OnDownloadUpdated(download_item);
}
