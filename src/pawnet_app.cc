#include "src/pawnet_app.h"

#include "include/cef_command_line.h"
#include "src/browser_window.h"
#include "src/home_page.h"
#include "src/settings.h"

namespace {

// Internal pages: pawnet://home, and later pawnet://settings.
const char kPawNetScheme[] = "pawnet";
const char kHomeHost[] = "home";

}  // namespace

void PawNetApp::OnRegisterCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {
  registrar->AddCustomScheme(kPawNetScheme,
                             CEF_SCHEME_OPTION_STANDARD |
                                 CEF_SCHEME_OPTION_SECURE |
                                 CEF_SCHEME_OPTION_CORS_ENABLED);
}

void PawNetApp::OnContextInitialized() {
  Settings settings = LoadSettings();

  // --url=<address> overrides the home page for this run, which is handy for
  // testing.
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
  if (command_line->HasSwitch("url")) {
    settings.home_page = command_line->GetSwitchValue("url").ToString();
  }

  CefRegisterSchemeHandlerFactory(
      kPawNetScheme, kHomeHost, new PawNetSchemeHandlerFactory(settings));

  BrowserWindow::Create(settings);
}
