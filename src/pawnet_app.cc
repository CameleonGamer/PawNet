#include "src/pawnet_app.h"

#include <string>

#include "include/cef_command_line.h"
#include "src/browser_window.h"

namespace {

const char kDefaultHomePage[] = "https://duckduckgo.com/";

}  // namespace

void PawNetApp::OnContextInitialized() {
  // --url=<address> overrides the start page, which is handy for testing.
  std::string home_page = kDefaultHomePage;
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
  if (command_line->HasSwitch("url")) {
    home_page = command_line->GetSwitchValue("url").ToString();
  }

  BrowserWindow::Create(home_page);
}
