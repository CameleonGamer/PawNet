#include "src/browser_window.h"

#include <algorithm>
#include <functional>
#include <utility>
#include <vector>

#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_fill_layout.h"
#include "include/views/cef_textfield_delegate.h"
#include "src/pawnet_client.h"

namespace {

const int kWindowWidth = 1200;
const int kWindowHeight = 800;
const int kReturnKeyCode = 0x0D;
const char kSearchUrl[] = "https://duckduckgo.com/?q=";
const char kNewTabTitle[] = "New tab";
const cef_color_t kActiveTabColor = 0xFF1A1A1A;
const cef_color_t kInactiveTabColor = 0xFF8A8A8A;

// Turns the address bar's text into something to load: a URL as typed, a bare
// host (https is assumed), or otherwise a search query.
std::string ToNavigableUrl(const std::string& input) {
  const size_t first = input.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = input.find_last_not_of(" \t");
  const std::string text = input.substr(first, last - first + 1);

  if (text.find("://") != std::string::npos || text.rfind("about:", 0) == 0) {
    return text;
  }
  if (text.find(' ') == std::string::npos &&
      text.find('.') != std::string::npos) {
    return "https://" + text;
  }
  return kSearchUrl + CefURIEncode(text, true).ToString();
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

CefRefPtr<CefLabelButton> MakeButton(const std::string& text,
                                     std::function<void()> on_press) {
  return CefLabelButton::CreateLabelButton(
      new CallbackButton(std::move(on_press)), text);
}

}  // namespace

// static
void BrowserWindow::Create(const std::string& home_page) {
  CefRefPtr<CefWindowDelegate> delegate(new BrowserWindow(home_page));
  CefWindow::CreateTopLevelWindow(delegate);
}

BrowserWindow::BrowserWindow(const std::string& home_page)
    : home_page_(home_page) {}

void BrowserWindow::OnWindowCreated(CefRefPtr<CefWindow> window) {
  window_ = window;
  window_->SetTitle("PawNet");
  window_->SetToFillLayout();

  BuildUi();

  window_->CenterWindow(CefSize(kWindowWidth, kWindowHeight));
  window_->Show();

  OpenTab(home_page_);
}

void BrowserWindow::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  // Release the window reference so the window and this delegate can be freed.
  window_ = nullptr;
  CefQuitMessageLoop();
}

bool BrowserWindow::CanClose(CefRefPtr<CefWindow> window) {
  if (tabs_.empty()) {
    return true;
  }
  // Each tab closes asynchronously. The last one to close also closes the
  // window, which then calls back here with no tabs left.
  CloseAllTabs();
  return false;
}

void BrowserWindow::BuildUi() {
  // Toolbar: the address bar takes all the remaining width.
  address_bar_ = CefTextfield::CreateTextfield(new AddressBarDelegate(
      [this](const std::string& input) { NavigateActiveTab(input); }));
  address_bar_->SetPlaceholderText("Search or enter an address");

  CefBoxLayoutSettings row;
  row.horizontal = true;
  row.inside_border_insets = {8, 8, 8, 8};
  row.between_child_spacing = 8;
  row.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;

  CefRefPtr<CefPanel> toolbar = CefPanel::CreatePanel(nullptr);
  CefRefPtr<CefBoxLayout> toolbar_layout = toolbar->SetToBoxLayout(row);
  toolbar->AddChildView(address_bar_);
  toolbar_layout->SetFlexForView(address_bar_, 1);

  // Tab strip: one button per tab, then the "new tab" and "close tab" buttons.
  // OpenTab() inserts tab buttons in front of those two.
  tab_strip_ = CefPanel::CreatePanel(nullptr);
  tab_strip_->SetToBoxLayout(row);
  tab_strip_->AddChildView(
      MakeButton("+", [this] { OpenTab(home_page_); }));
  tab_strip_->AddChildView(MakeButton("x", [this] { CloseActiveTab(); }));

  content_ = CefPanel::CreatePanel(nullptr);
  content_->SetToFillLayout();

  CefBoxLayoutSettings column;
  column.horizontal = false;
  column.cross_axis_alignment = CEF_AXIS_ALIGNMENT_STRETCH;

  CefRefPtr<CefPanel> root = CefPanel::CreatePanel(nullptr);
  CefRefPtr<CefBoxLayout> root_layout = root->SetToBoxLayout(column);
  root->AddChildView(toolbar);
  root->AddChildView(tab_strip_);
  root->AddChildView(content_);
  root_layout->SetFlexForView(content_, 1);

  window_->AddChildView(root);
}

void BrowserWindow::OpenTab(const std::string& url) {
  const int id = next_tab_id_++;

  CefRefPtr<CefClient> client(new PawNetClient(this, id));
  CefRefPtr<CefBrowserView> view = CefBrowserView::CreateBrowserView(
      client, url, CefBrowserSettings(), nullptr, nullptr, nullptr);

  CefRefPtr<CefLabelButton> button = MakeButton(
      kNewTabTitle, [this, id] { ActivateTab(IndexOfTab(id)); });

  tab_strip_->AddChildViewAt(button, static_cast<int>(tabs_.size()));
  content_->AddChildView(view);
  tabs_.push_back({id, url, view, button});

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
        CEF_BUTTON_STATE_NORMAL, active ? kActiveTabColor : kInactiveTabColor);
  }

  address_bar_->SetText(tabs_[index].url);
  tabs_[index].view->RequestFocus();
}

void BrowserWindow::CloseActiveTab() {
  if (tabs_.empty()) {
    return;
  }
  CefRefPtr<CefBrowser> browser = tabs_[active_tab_].view->GetBrowser();
  if (browser) {
    browser->GetHost()->CloseBrowser(false);
  }
}

void BrowserWindow::CloseAllTabs() {
  // Iterate over a copy: OnTabClosed() removes entries from tabs_.
  const std::vector<Tab> tabs = tabs_;
  for (const Tab& tab : tabs) {
    CefRefPtr<CefBrowser> browser = tab.view->GetBrowser();
    if (browser) {
      browser->GetHost()->CloseBrowser(false);
    }
  }
}

void BrowserWindow::NavigateActiveTab(const std::string& input) {
  const std::string url = ToNavigableUrl(input);
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
  tabs_[index].button->SetText(title.empty() ? tabs_[index].url : title);
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

void BrowserWindow::OnTabClosed(int tab_id) {
  const size_t index = IndexOfTab(tab_id);
  if (index == tabs_.size()) {
    return;
  }

  content_->RemoveChildView(tabs_[index].view);
  tab_strip_->RemoveChildView(tabs_[index].button);
  tabs_.erase(tabs_.begin() + index);

  if (tabs_.empty()) {
    window_->Close();
    return;
  }
  if (index < active_tab_) {
    --active_tab_;
  }
  ActivateTab(std::min(active_tab_, tabs_.size() - 1));
}

size_t BrowserWindow::IndexOfTab(int tab_id) const {
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].id == tab_id) {
      return i;
    }
  }
  return tabs_.size();
}
