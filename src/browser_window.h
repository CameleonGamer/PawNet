#ifndef PAWNET_SRC_BROWSER_WINDOW_H_
#define PAWNET_SRC_BROWSER_WINDOW_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "include/cef_download_item.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "src/settings.h"

// The main window: a toolbar with the address bar, a tab strip and the page
// area. Each tab is its own CefBrowserView, and only the active one is visible.
class BrowserWindow : public CefWindowDelegate {
 public:
  // Opens a window whose first tab loads the home page from |settings|. The
  // window object lives until its browsers have closed.
  static BrowserWindow* Create(const Settings& settings);

  // Applies saved settings to the open window: colours, search address and
  // shortcuts, and reloads the home pages that are open.
  void ApplySettings(const Settings& settings);

  // Adds a tab that loads |url| and makes it the active tab.
  void OpenTab(const std::string& url);

  // Called by PawNetClient on the UI thread.
  void OnTitleChanged(int tab_id, const std::string& title);
  void OnAddressChanged(int tab_id, const std::string& url);
  void OnBrowserClosed();
  void OnFullscreenModeChanged(bool fullscreen);
  // Shows the latest download in the toolbar: name and progress or result.
  void OnDownloadUpdated(CefRefPtr<CefDownloadItem> download_item);

  // CefWindowDelegate:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  bool CanClose(CefRefPtr<CefWindow> window) override;
  // Alloy style is required: Chrome style allows only one BrowserView per
  // window, and every tab is one.
  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  struct Tab {
    int id;
    std::string url;
    CefRefPtr<CefBrowserView> view;
    CefRefPtr<CefLabelButton> button;
  };

  explicit BrowserWindow(const Settings& settings);

  void BuildUi();
  void ActivateTab(size_t index);
  void CloseActiveTab();
  // Removes a tab from the window. Its browser is destroyed once the view is
  // released, and OnBrowserClosed() is called then.
  void RemoveTab(size_t index);
  // Removes a tab and keeps a neighbouring tab active.
  void RemoveTabAndShowNeighbour(size_t index);
  void NavigateActiveTab(const std::string& input);
  // Opens the downloads folder in a new tab.
  void OpenDownloadsFolder();
  // Quits the application once the window and all its browsers are gone.
  void FinishIfDone();
  // Returns tabs_.size() when no tab has |tab_id|.
  size_t IndexOfTab(int tab_id) const;

  Settings settings_;
  // Shared with the panels' colour delegates, which reapply it on theme changes.
  std::shared_ptr<cef_color_t> toolbar_color_;
  CefRefPtr<CefWindow> window_;
  CefRefPtr<CefPanel> toolbar_;
  CefRefPtr<CefPanel> tab_strip_;
  CefRefPtr<CefPanel> content_;
  CefRefPtr<CefTextfield> address_bar_;
  CefRefPtr<CefLabelButton> download_button_;
  std::vector<Tab> tabs_;
  size_t active_tab_ = 0;
  int next_tab_id_ = 1;

  // Browsers that have not reported OnBeforeClose yet. Their clients point
  // back to this object, so |self_| keeps it alive until they are all gone.
  int open_browsers_ = 0;
  bool window_destroyed_ = false;
  CefRefPtr<BrowserWindow> self_;

  IMPLEMENT_REFCOUNTING(BrowserWindow);
};

#endif  // PAWNET_SRC_BROWSER_WINDOW_H_
