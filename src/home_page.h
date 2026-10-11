#ifndef PAWNET_SRC_HOME_PAGE_H_
#define PAWNET_SRC_HOME_PAGE_H_

#include <functional>
#include <mutex>
#include <string>

#include "include/cef_scheme.h"
#include "src/settings.h"

// Serves the internal pages: pawnet://home, pawnet://settings, pawnet://bookmarks
// and the form targets pawnet://settings/save and pawnet://bookmarks/remove. Pages are built from the current settings on
// every request, so a saved change shows up at once.
class PawNetSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  // Called on the UI thread after the settings have been saved.
  using SettingsListener = std::function<void(const Settings&)>;

  explicit PawNetSchemeHandlerFactory(const Settings& settings);

  void SetSettingsListener(SettingsListener listener);

  // CefSchemeHandlerFactory:
  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override;

 private:
  Settings CurrentSettings();
  CefRefPtr<CefResourceHandler> HandleSave(CefRefPtr<CefRequest> request);
  CefRefPtr<CefResourceHandler> HandleRemoveBookmark(
      CefRefPtr<CefRequest> request);

  // Requests can arrive on the IO thread, so settings are read and written
  // under this lock.
  std::mutex mutex_;
  Settings settings_;
  SettingsListener listener_;

  IMPLEMENT_REFCOUNTING(PawNetSchemeHandlerFactory);
};

#endif  // PAWNET_SRC_HOME_PAGE_H_
