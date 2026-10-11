#include "src/pawnet_app.h"

#include "include/cef_command_line.h"
#include "src/browser_window.h"
#include "src/home_page.h"
#include "src/settings.h"

namespace {

// Internal pages: pawnet://home and pawnet://settings.
const char kPawNetScheme[] = "pawnet";
const char kHomeHost[] = "home";
const char kSettingsHost[] = "settings";
const char kBookmarksHost[] = "bookmarks";

}  // namespace

void PawNetApp::OnRegisterCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {
  registrar->AddCustomScheme(kPawNetScheme,
                             CEF_SCHEME_OPTION_STANDARD |
                                 CEF_SCHEME_OPTION_SECURE |
                                 CEF_SCHEME_OPTION_CORS_ENABLED);
}

void PawNetApp::OnContextInitialized() {
  const Settings settings = LoadSettings();

  // One handler serves both internal pages, built from the settings it holds.
  CefRefPtr<PawNetSchemeHandlerFactory> pages(
      new PawNetSchemeHandlerFactory(settings));
  CefRegisterSchemeHandlerFactory(kPawNetScheme, kHomeHost, pages);
  CefRegisterSchemeHandlerFactory(kPawNetScheme, kSettingsHost, pages);
  CefRegisterSchemeHandlerFactory(kPawNetScheme, kBookmarksHost, pages);

  // --url=<address> overrides the home page for this run only, which is handy
  // for testing. Saving the settings never writes this value to the file.
  Settings window_settings = settings;
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
  if (command_line->HasSwitch("url")) {
    window_settings.home_page = command_line->GetSwitchValue("url").ToString();
  }

  // The window lives until the application quits, and saving is only possible
  // from a page of that window, so the pointer stays valid.
  BrowserWindow* window = BrowserWindow::Create(window_settings);
  pages->SetSettingsListener(
      [window](const Settings& saved) { window->ApplySettings(saved); });
}
