#include "src/browser_window.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>

#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "include/cef_image.h"
#include "include/cef_parser.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_fill_layout.h"
#include "include/views/cef_panel_delegate.h"
#include "include/views/cef_textfield_delegate.h"
#include "src/pawnet_client.h"

namespace {

const int kWindowWidth = 1200;
const int kWindowHeight = 800;
const int kReturnKeyCode = 0x0D;
const size_t kMaxTabTitleLength = 24;
const char kNewTabTitle[] = "New tab";

// True when |text| starts with a scheme whose URL may contain spaces, such as
// data: URLs with inline HTML.
bool HasUrlSchemePrefix(const std::string& text) {
  static const char* const kSchemes[] = {"about:", "data:", "file:",
                                         "view-source:"};
  for (const char* scheme : kSchemes) {
    if (text.rfind(scheme, 0) == 0) {
      return true;
    }
  }
  return false;
}

// Turns the address bar's text into something to load: a URL as typed, a bare
// host (https is assumed), or otherwise a search query.
std::string ToNavigableUrl(const std::string& input,
                           const std::string& search_url) {
  const size_t first = input.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = input.find_last_not_of(" \t");
  const std::string text = input.substr(first, last - first + 1);

  if (text.find("://") != std::string::npos || HasUrlSchemePrefix(text)) {
    return text;
  }
  if (text.find(' ') == std::string::npos &&
      text.find('.') != std::string::npos) {
    return "https://" + text;
  }
  return search_url + CefURIEncode(text, true).ToString();
}

// Shortens |text| to |max_chars| characters, counting UTF-8 code points.
std::string Truncate(const std::string& text, size_t max_chars) {
  size_t chars = 0;
  for (size_t i = 0; i < text.size(); ++i) {
    // Continuation bytes (10xxxxxx) belong to the code point before them.
    if ((static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) {
      continue;
    }
    if (chars == max_chars) {
      return text.substr(0, i) + "...";
    }
    ++chars;
  }
  return text;
}

// Loads the app icon that the build copies next to the executable. Returns
// null when it is missing, and the window then keeps the default icon.
CefRefPtr<CefImage> LoadAppIcon() {
  const std::string program =
      CefCommandLine::GetGlobalCommandLine()->GetProgram().ToString();
  const size_t slash = program.find_last_of('/');
  const std::string directory =
      slash == std::string::npos ? "" : program.substr(0, slash + 1);

  std::ifstream file(directory + "pawnet-256.png", std::ios::binary);
  if (!file) {
    return nullptr;
  }
  const std::string png((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

  CefRefPtr<CefImage> image = CefImage::CreateImage();
  if (!image->AddPNG(1.0f, png.data(), png.size())) {
    return nullptr;
  }
  return image;
}

// Runs a callback when a button is pressed.
class CallbackButton : public CefButtonDelegate {
 public:
  explicit CallbackButton(std::function<void()> on_press)
      : on_press_(std::move(on_press)) {}

  void OnButtonPressed(CefRefPtr<CefButton> button) override { on_press_(); }

 private:
  const std::function<void()> on_press_;

  IMPLEMENT_REFCOUNTING(CallbackButton);
};

// Runs a callback with the typed text when Enter is pressed.
class AddressBarDelegate : public CefTextfieldDelegate {
 public:
  explicit AddressBarDelegate(std::function<void(const std::string&)> on_submit)
      : on_submit_(std::move(on_submit)) {}

  bool OnKeyEvent(CefRefPtr<CefTextfield> textfield,
                  const CefKeyEvent& event) override {
    if (event.type != KEYEVENT_RAWKEYDOWN ||
        event.windows_key_code != kReturnKeyCode) {
      return false;
    }
    on_submit_(textfield->GetText().ToString());
    return true;
  }

 private:
  const std::function<void(const std::string&)> on_submit_;

  IMPLEMENT_REFCOUNTING(AddressBarDelegate);
};

// Keeps each tab's BrowserView on the Alloy runtime, matching the window.
class AlloyBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  IMPLEMENT_REFCOUNTING(AlloyBrowserViewDelegate);
};

// Keeps a panel's background color. CEF resets it on every theme change, so
// the color is applied again each time.
class PanelColorDelegate : public CefPanelDelegate {
 public:
  explicit PanelColorDelegate(cef_color_t color) : color_(color) {}

  void OnThemeChanged(CefRefPtr<CefView> view) override {
    view->SetBackgroundColor(color_);
  }

 private:
  const cef_color_t color_;

  IMPLEMENT_REFCOUNTING(PanelColorDelegate);
};

CefRefPtr<CefLabelButton> MakeButton(const std::string& text,
                                     std::function<void()> on_press) {
  return CefLabelButton::CreateLabelButton(
      new CallbackButton(std::move(on_press)), text);
}

}  // namespace

// static
void BrowserWindow::Create(const Settings& settings) {
  CefRefPtr<BrowserWindow> window(new BrowserWindow(settings));
  window->self_ = window;
  CefWindow::CreateTopLevelWindow(window.get());
}

BrowserWindow::BrowserWindow(const Settings& settings) : settings_(settings) {}

void BrowserWindow::OnWindowCreated(CefRefPtr<CefWindow> window) {
  window_ = window;
  window_->SetTitle("PawNet");
  if (CefRefPtr<CefImage> icon = LoadAppIcon()) {
    window_->SetWindowIcon(icon);
    window_->SetWindowAppIcon(icon);
  }
  window_->SetToFillLayout();

  BuildUi();

  window_->CenterWindow(CefSize(kWindowWidth, kWindowHeight));
  window_->Show();

  OpenTab(settings_.home_page);
}

void BrowserWindow::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  // Release the window reference so the window and this delegate can be freed.
  window_ = nullptr;
  window_destroyed_ = true;
  FinishIfDone();
}

bool BrowserWindow::CanClose(CefRefPtr<CefWindow> window) {
  // Ask every page first: a beforeunload prompt makes TryCloseBrowser() return
  // false, which cancels the window close. After the user confirms, CEF calls
  // CanClose() again and the pages are ready.
  for (const Tab& tab : tabs_) {
    CefRefPtr<CefBrowser> browser = tab.view->GetBrowser();
    if (browser && !browser->GetHost()->TryCloseBrowser()) {
      return false;
    }
  }
  // Leave the views in the window: destroying the window destroys them, and
  // each browser then reports OnBrowserClosed().
  return true;
}

void BrowserWindow::OnFullscreenModeChanged(bool fullscreen) {
  // Hide the toolbar and tab strip so the page can use the whole screen.
  toolbar_->SetVisible(!fullscreen);
  tab_strip_->SetVisible(!fullscreen);
  window_->SetFullscreen(fullscreen);
}

void BrowserWindow::BuildUi() {
  // Toolbar: close tab, new tab, then the address bar, which takes the rest of
  // the width. The actions stay visible even when many tabs overflow the strip.
  address_bar_ = CefTextfield::CreateTextfield(new AddressBarDelegate(
      [this](const std::string& input) { NavigateActiveTab(input); }));
  address_bar_->SetPlaceholderText("Search or enter an address");

  CefBoxLayoutSettings row;
  row.horizontal = true;
  row.inside_border_insets = {8, 8, 8, 8};
  row.between_child_spacing = 8;
  row.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;

  toolbar_ = CefPanel::CreatePanel(
      new PanelColorDelegate(settings_.toolbar_color));
  CefRefPtr<CefBoxLayout> toolbar_layout = toolbar_->SetToBoxLayout(row);
  toolbar_->SetBackgroundColor(settings_.toolbar_color);
  toolbar_->AddChildView(MakeButton("x", [this] { CloseActiveTab(); }));
  toolbar_->AddChildView(
      MakeButton("+", [this] { OpenTab(settings_.home_page); }));
  toolbar_->AddChildView(address_bar_);
  toolbar_layout->SetFlexForView(address_bar_, 1);

  // Tab strip: one button per tab, added by OpenTab().
  tab_strip_ = CefPanel::CreatePanel(
      new PanelColorDelegate(settings_.toolbar_color));
  tab_strip_->SetToBoxLayout(row);
  tab_strip_->SetBackgroundColor(settings_.toolbar_color);

  content_ = CefPanel::CreatePanel(nullptr);
  content_->SetToFillLayout();

  CefBoxLayoutSettings column;
  column.horizontal = false;
  column.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;

  CefRefPtr<CefPanel> root = CefPanel::CreatePanel(nullptr);
  CefRefPtr<CefBoxLayout> root_layout = root->SetToBoxLayout(column);
  root->AddChildView(toolbar_);
  root->AddChildView(tab_strip_);
  root->AddChildView(content_);
  root_layout->SetFlexForView(content_, 1);

  window_->AddChildView(root);
}

void BrowserWindow::OpenTab(const std::string& url) {
  const int id = next_tab_id_++;

  CefRefPtr<CefClient> client(new PawNetClient(this, id));
  CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
      client, url, CefBrowserSettings(), nullptr, nullptr,
      new AlloyBrowserViewDelegate);

  CefRefPtr<CefLabelButton> button = MakeButton(
      kNewTabTitle, [this, id] { ActivateTab(IndexOfTab(id)); });

  tab_strip_->AddChildViewAt(button, static_cast<int>(tabs_.size()));
  content_->AddChildView(view);
  tabs_.push_back({id, url, view, button});
  ++open_browsers_;

  ActivateTab(tabs_.size() - 1);
}

void BrowserWindow::ActivateTab(size_t index) {
  if (index >= tabs_.size()) {
    return;
  }
  active_tab_ = index;

  for (size_t i = 0; i < tabs_.size(); ++i) {
    const bool active = i == index;
    tabs_[i].view->SetVisible(active);
    tabs_[i].button->SetTextColor(
        CEF_BUTTON_STATE_NORMAL,
        active ? settings_.active_tab_color : settings_.inactive_tab_color);
  }

  address_bar_->SetText(tabs_[index].url);
  tabs_[index].view->RequestFocus();
}

void BrowserWindow::CloseActiveTab() {
  if (tabs_.empty()) {
    return;
  }
  // Keep this object alive: closing the last tab can finish the application
  // before this function returns.
  CefRefPtr<BrowserWindow> keep_alive(this);

  RemoveTabAndShowNeighbour(active_tab_);
  if (tabs_.empty()) {
    window_->Close();
  }
}

void BrowserWindow::RemoveTab(size_t index) {
  // Alloy style: a browser is destroyed when its BrowserView is removed from
  // the window and released. CloseBrowser() is not used for single tabs, since
  // it would route the close through the window.
  Tab tab = tabs_[index];
  content_->RemoveChildView(tab.view);
  tab_strip_->RemoveChildView(tab.button);
  tabs_.erase(tabs_.begin() + index);
}

void BrowserWindow::RemoveTabAndShowNeighbour(size_t index) {
  const bool was_active = index == active_tab_;
  RemoveTab(index);
  if (tabs_.empty()) {
    return;
  }
  if (index < active_tab_) {
    --active_tab_;
  }
  if (was_active || active_tab_ >= tabs_.size()) {
    ActivateTab(std::min(active_tab_, tabs_.size() - 1));
  }
}

void BrowserWindow::NavigateActiveTab(const std::string& input) {
  const std::string url = ToNavigableUrl(input, settings_.search_url);
  if (url.empty() || tabs_.empty()) {
    return;
  }
  CefRefPtr<CefBrowser> browser = tabs_[active_tab_].view->GetBrowser();
  if (browser) {
    browser->GetMainFrame()->LoadURL(url);
  }
  tabs_[active_tab_].view->RequestFocus();
}

void BrowserWindow::OnTitleChanged(int tab_id, const std::string& title) {
  const size_t index = IndexOfTab(tab_id);
  if (index == tabs_.size()) {
    return;
  }
  const std::string& label = title.empty() ? tabs_[index].url : title;
  tabs_[index].button->SetText(Truncate(label, kMaxTabTitleLength));
}

void BrowserWindow::OnAddressChanged(int tab_id, const std::string& url) {
  const size_t index = IndexOfTab(tab_id);
  if (index == tabs_.size()) {
    return;
  }
  tabs_[index].url = url;
  if (index == active_tab_) {
    address_bar_->SetText(url);
  }
}

void BrowserWindow::OnBrowserClosed() {
  --open_browsers_;
  FinishIfDone();
}

void BrowserWindow::FinishIfDone() {
  if (!window_destroyed_ || open_browsers_ > 0) {
    return;
  }
  CefQuitMessageLoop();
  // Releasing the last reference may delete this object, so nothing after it.
  self_ = nullptr;
}

size_t BrowserWindow::IndexOfTab(int tab_id) const {
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].id == tab_id) {
      return i;
    }
  }
  return tabs_.size();
}
