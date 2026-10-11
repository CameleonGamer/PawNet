#ifndef PAWNET_SRC_HOME_PAGE_H_
#define PAWNET_SRC_HOME_PAGE_H_

#include <string>

#include "include/cef_scheme.h"
#include "src/settings.h"

// Serves pawnet://home, the home page, built from the user's settings.
class PawNetSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  explicit PawNetSchemeHandlerFactory(const Settings& settings);

  // CefSchemeHandlerFactory:
  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override;

 private:
  // Kept for the lifetime of the factory: the response streams from this memory.
  const std::string home_html_;

  IMPLEMENT_REFCOUNTING(PawNetSchemeHandlerFactory);
};

#endif  // PAWNET_SRC_HOME_PAGE_H_
