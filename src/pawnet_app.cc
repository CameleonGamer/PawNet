#include "src/pawnet_app.h"

#include "include/cef_command_line.h"
#include "src/browser_window.h"
#include "src/settings.h"

void PawNetApp::OnContextInitialized() {
  Settings settings = LoadSettings();

  // --url=<address> overrides the home page for this run, which is handy for
  // testing.
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
  if (command_line->HasSwitch("url")) {
    settings.home_page = command_line->GetSwitchValue("url").ToString();
  }

  BrowserWindow::Create(settings);
}
