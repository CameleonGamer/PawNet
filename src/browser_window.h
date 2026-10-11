#ifndef PAWNET_SRC_BROWSER_WINDOW_H_
#define PAWNET_SRC_BROWSER_WINDOW_H_

#include <cstddef>
#include <string>
#include <vector>

#include "include/views/cef_browser_view.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"

// The main window: an address bar, a tab strip and the page area. Each tab is
// its own CefBrowserView, and only the active one is visible.
class BrowserWindow : public CefWindowDelegate {
 public:
  // Opens a window whose first tab loads |home_page|.
  static void Create(const std::string& home_page);

  // Adds a tab that loads |url| and makes it the active tab.
  void OpenTab(const std::string& url);

  // Called by PawNetClient on the UI thread.
  void OnTitleChanged(int tab_id, const std::string& title);
  void OnAddressChanged(int tab_id, const std::string& url);
  void OnTabClosed(int tab_id);

  // CefWindowDelegate:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  bool CanClose(CefRefPtr<CefWindow> window) override;

 private:
  struct Tab {
    int id;
    std::string url;
    CefRefPtr<CefBrowserView> view;
    CefRefPtr<CefLabelButton> button;
  };

  explicit BrowserWindow(const std::string& home_page);

  void BuildUi();
  void ActivateTab(size_t index);
  void CloseActiveTab();
  void CloseAllTabs();
  void NavigateActiveTab(const std::string& input);
  // Returns tabs_.size() when no tab has |tab_id|.
  size_t IndexOfTab(int tab_id) const;

  const std::string home_page_;
  CefRefPtr<CefWindow> window_;
  CefRefPtr<CefPanel> tab_strip_;
  CefRefPtr<CefPanel> content_;
  CefRefPtr<CefTextfield> address_bar_;
  std::vector<Tab> tabs_;
  size_t active_tab_ = 0;
  int next_tab_id_ = 1;

  IMPLEMENT_REFCOUNTING(BrowserWindow);
};

#endif  // PAWNET_SRC_BROWSER_WINDOW_H_
