#include "src/home_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "include/cef_parser.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"

namespace {

// The paw from assets/pawnet-256.png, on a 64 x 64 grid. Keep the two in sync
// (tools/make_icon.py draws the same shapes).
const char kPawSvg[] =
    R"SVG(<svg viewBox="0 0 64 64" aria-hidden="true" class="paw"><g fill="currentColor">)SVG"
    R"SVG(<ellipse cx="12" cy="30" rx="5.5" ry="7" transform="rotate(-20 12 30)"/>)SVG"
    R"SVG(<ellipse cx="25" cy="16" rx="5.5" ry="7.5" transform="rotate(-8 25 16)"/>)SVG"
    R"SVG(<ellipse cx="39" cy="16" rx="5.5" ry="7.5" transform="rotate(8 39 16)"/>)SVG"
    R"SVG(<ellipse cx="52" cy="30" rx="5.5" ry="7" transform="rotate(20 52 30)"/>)SVG"
    R"SVG(<ellipse cx="32" cy="44" rx="14" ry="12"/></g></svg>)SVG";

// Escapes text for HTML text and attribute values.
std::string HtmlEscape(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (const char c : text) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out += c;
    }
  }
  return out;
}

// Letter shown on a shortcut tile. Non-ASCII names get a neutral dot.
std::string Initial(const std::string& name) {
  if (name.empty() || static_cast<unsigned char>(name[0]) >= 0x80) {
    return "\xE2\x80\xA2";  // U+2022 BULLET
  }
  return std::string(1, static_cast<char>(std::toupper(name[0])));
}

// Styles are kept in one block so the pages need no other files. The accent
// colour is declared after this block, from the settings.
const char kStyle[] = R"(
  :root {
    color-scheme: dark;
    --text: #ECEEF2;
    --muted: #9AA0AA;
    --surface: rgba(255, 255, 255, 0.06);
    --surface-hover: rgba(255, 255, 255, 0.11);
    --border: rgba(255, 255, 255, 0.12);
  }
  * { box-sizing: border-box; }
  html, body { margin: 0; min-height: 100%; }
  body {
    min-height: 100vh;
    font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
    color: var(--text);
    -webkit-font-smoothing: antialiased;
  }
  /* The background is set from the settings; a soft glow sits on top of it. */
  .backdrop {
    position: fixed;
    inset: 0;
    z-index: -1;
  }
  .backdrop::after {
    content: "";
    position: absolute;
    inset: 0;
    background: radial-gradient(60% 45% at 50% 0%, color-mix(in srgb, var(--accent) 16%, transparent), transparent 70%);
  }
  main {
    min-height: 100vh;
    padding: 14vh 16px 48px;
    display: flex;
    flex-direction: column;
    align-items: center;
  }
  main.top { padding-top: 6vh; justify-content: flex-start; }
  .brand {
    display: flex;
    align-items: center;
    gap: 14px;
    margin-bottom: 12px;
    animation: rise 0.5s ease-out both;
  }
  .paw { width: 56px; height: 56px; color: var(--accent); }
  h1 {
    margin: 0;
    font-size: 44px;
    font-weight: 700;
    letter-spacing: -0.8px;
  }
  .greeting {
    margin: 0 0 24px;
    font-size: 15px;
    color: var(--muted);
    animation: rise 0.5s 0.04s ease-out both;
  }
  .greeting .clock { margin-left: 10px; font-variant-numeric: tabular-nums; }
  .search {
    width: min(600px, 100%);
    animation: rise 0.5s 0.08s ease-out both;
  }
  .search input {
    display: block;
    width: 100%;
    padding: 16px 22px;
    font: inherit;
    font-size: 17px;
    color: inherit;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 999px;
    outline: none;
    transition: border-color 0.2s, background-color 0.2s, box-shadow 0.2s;
  }
  .search input::placeholder { color: var(--muted); }
  .search input:hover { background: var(--surface-hover); }
  .search input:focus {
    border-color: var(--accent);
    box-shadow: 0 0 0 4px color-mix(in srgb, var(--accent) 18%, transparent);
  }
  .shortcuts {
    width: min(600px, 100%);
    margin-top: 40px;
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(132px, 1fr));
    gap: 14px;
    animation: rise 0.5s 0.16s ease-out both;
  }
  .tile {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 10px;
    padding: 18px 10px 16px;
    color: inherit;
    text-decoration: none;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 16px;
    transition: transform 0.2s, background-color 0.2s, border-color 0.2s;
  }
  .tile:hover, .tile:focus-visible {
    transform: translateY(-2px);
    background: var(--surface-hover);
    border-color: color-mix(in srgb, var(--accent) 55%, transparent);
    outline: none;
  }
  .badge {
    width: 44px;
    height: 44px;
    display: flex;
    align-items: center;
    justify-content: center;
    border-radius: 14px;
    background: var(--accent);
    color: #1B1D22;
    font-size: 19px;
    font-weight: 700;
  }
  .label {
    max-width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font-size: 14px;
    color: var(--text);
  }
  @keyframes rise {
    from { opacity: 0; transform: translateY(8px); }
    to { opacity: 1; transform: none; }
  }
  @media (prefers-reduced-motion: reduce) {
    *, *::before, *::after { animation: none !important; transition: none !important; }
  }
  @media (max-width: 480px) {
    h1 { font-size: 36px; }
    .paw { width: 46px; height: 46px; }
  }
)";

// Only used by the settings page, on top of kStyle.
const char kSettingsStyle[] = R"(
  main.settings { padding-top: 8vh; justify-content: flex-start; }
  .notice {
    width: min(560px, 100%);
    margin: 0 0 20px;
    padding: 12px 16px;
    font-size: 14px;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 12px;
  }
  .notice.error { border-color: #E5484D; color: #FFB4B4; }
  .settings-form {
    width: min(560px, 100%);
    display: grid;
    gap: 14px;
  }
  .field {
    display: grid;
    gap: 6px;
    font-size: 14px;
    color: var(--muted);
  }
  .field input[type=text], .field select, .field textarea {
    font: inherit;
    font-size: 15px;
    color: var(--text);
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 10px 14px;
    outline: none;
  }
  .field textarea { min-height: 120px; font-family: ui-monospace, Menlo, monospace; font-size: 13px; }
  .field input:focus, .field select:focus, .field textarea:focus { border-color: var(--accent); }
  .field input[type=color] {
    width: 56px;
    height: 36px;
    padding: 0;
    background: none;
    border: 1px solid var(--border);
    border-radius: 8px;
  }
  .colors { display: flex; flex-wrap: wrap; gap: 18px; }
  .save {
    justify-self: start;
    margin-top: 8px;
    padding: 12px 26px;
    font: inherit;
    font-weight: 600;
    color: #1B1D22;
    background: var(--accent);
    border: 0;
    border-radius: 999px;
    cursor: pointer;
  }
  .links { margin-top: 28px; font-size: 14px; }
  .links a { color: var(--accent); }
)";

// Serves a string as a readable stream. CefStreamReader::CreateForData does not
// copy its data, so the text is owned here instead.
class StringReadHandler : public CefReadHandler {
 public:
  explicit StringReadHandler(std::string data) : data_(std::move(data)) {}

  size_t Read(void* ptr, size_t size, size_t n) override {
    if (size == 0) {
      return 0;
    }
    const size_t available = position_ < data_.size() ? data_.size() - position_ : 0;
    const size_t count = std::min(size * n, available);
    std::memcpy(ptr, data_.data() + position_, count);
    position_ += count;
    return count / size;
  }

  int Seek(int64_t offset, int whence) override {
    int64_t base = 0;
    if (whence == SEEK_CUR) {
      base = static_cast<int64_t>(position_);
    } else if (whence == SEEK_END) {
      base = static_cast<int64_t>(data_.size());
    } else if (whence != SEEK_SET) {
      return -1;
    }
    const int64_t target = base + offset;
    if (target < 0 || target > static_cast<int64_t>(data_.size())) {
      return -1;
    }
    position_ = static_cast<size_t>(target);
    return 0;
  }

  int64_t Tell() override { return static_cast<int64_t>(position_); }
  int Eof() override { return position_ >= data_.size() ? 1 : 0; }
  bool MayBlock() override { return false; }

 private:
  const std::string data_;
  size_t position_ = 0;

  IMPLEMENT_REFCOUNTING(StringReadHandler);
};

// Runs a function on the thread it is posted to.
class CallbackTask : public CefTask {
 public:
  explicit CallbackTask(std::function<void()> callback)
      : callback_(std::move(callback)) {}

  void Execute() override { callback_(); }

 private:
  const std::function<void()> callback_;

  IMPLEMENT_REFCOUNTING(CallbackTask);
};

CefRefPtr<CefResourceHandler> HtmlResponse(const std::string& html) {
  return new CefStreamResourceHandler(
      "text/html",
      CefStreamReader::CreateForHandler(new StringReadHandler(html)));
}

// Answers with a 303 so that reloading the page does not submit the form again.
CefRefPtr<CefResourceHandler> RedirectResponse(const std::string& location) {
  CefResponse::HeaderMap headers;
  headers.emplace("Location", location);
  return new CefStreamResourceHandler(
      303, "See Other", "text/plain", headers,
      CefStreamReader::CreateForHandler(new StringReadHandler("")));
}

std::string UrlDecode(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < text.size() &&
               std::isxdigit(static_cast<unsigned char>(text[i + 1])) &&
               std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
      out += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
      i += 2;
    } else {
      out += c;
    }
  }
  return out;
}

// Parses "a=1&b=2" from a form body. A repeated key keeps its last value.
std::map<std::string, std::string> ParseForm(const std::string& body) {
  std::map<std::string, std::string> fields;
  size_t start = 0;
  while (start <= body.size()) {
    size_t end = body.find('&', start);
    if (end == std::string::npos) {
      end = body.size();
    }
    const std::string pair = body.substr(start, end - start);
    const size_t equals = pair.find('=');
    if (equals != std::string::npos) {
      fields[UrlDecode(pair.substr(0, equals))] =
          UrlDecode(pair.substr(equals + 1));
    }
    start = end + 1;
  }
  return fields;
}

std::string ReadPostBody(CefRefPtr<CefRequest> request) {
  std::string body;
  CefRefPtr<CefPostData> post = request->GetPostData();
  if (!post) {
    return body;
  }
  CefPostData::ElementVector elements;
  post->GetElements(elements);
  for (CefRefPtr<CefPostDataElement> element : elements) {
    if (element->GetType() != PDE_TYPE_BYTES) {
      continue;
    }
    const size_t size = element->GetBytesCount();
    std::string chunk(size, '\0');
    element->GetBytes(size, chunk.data());
    body += chunk;
  }
  return body;
}

std::string BuildHomeHtml(const Settings& settings) {
  std::string html = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Home</title>
<style>)HTML";
  html += kStyle;
  // The accent was checked as "#RRGGBB" when the settings were read, so it is
  // safe to put it in the stylesheet.
  html += ":root { --accent: " + settings.accent_color + "; }\n";
  html += R"HTML(</style>
</head>
<body>
<div class="backdrop" style="background: )HTML";
  html += HtmlEscape(settings.home_background);
  html += R"HTML("></div>
<main)HTML";
  html += settings.home_layout == HomeLayout::kTop ? " class=\"top\"" : "";
  html += R"HTML(>
  <header class="brand">
    )HTML";
  html += kPawSvg;
  html += R"HTML(
    <h1>PawNet</h1>
  </header>
)HTML";

  if (settings.home_greeting) {
    html += R"HTML(  <p class="greeting"><span id="greeting"></span><time id="clock" class="clock"></time></p>
)HTML";
  }

  html += R"HTML(  <form id="search" class="search" data-search=")HTML";
  html += HtmlEscape(settings.search_url);
  html += R"HTML(">
    <input name="q" type="search" placeholder="Search or enter a query" autocomplete="off" autofocus aria-label="Search">
  </form>
)HTML";

  if (!settings.shortcuts.empty()) {
    html += "  <nav class=\"shortcuts\" aria-label=\"Shortcuts\">\n";
    for (const Shortcut& shortcut : settings.shortcuts) {
      html += "    <a class=\"tile\" href=\"" + HtmlEscape(shortcut.url) +
              "\"><span class=\"badge\">" + HtmlEscape(Initial(shortcut.name)) +
              "</span><span class=\"label\">" + HtmlEscape(shortcut.name) +
              "</span></a>\n";
    }
    html += "  </nav>\n";
  }

  html += R"HTML(</main>
<script>
  const form = document.getElementById('search');
  form.addEventListener('submit', (event) => {
    event.preventDefault();
    const query = form.q.value.trim();
    if (query) {
      window.location.href = form.dataset.search + encodeURIComponent(query);
    }
  });

  const greeting = document.getElementById('greeting');
  const clock = document.getElementById('clock');
  if (greeting && clock) {
    const update = () => {
      const now = new Date();
      const hour = now.getHours();
      greeting.textContent = hour < 5 ? 'Good night'
        : hour < 12 ? 'Good morning'
        : hour < 18 ? 'Good afternoon'
        : 'Good evening';
      clock.textContent = now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    };
    update();
    setInterval(update, 15000);
  }
</script>
</body>
</html>
)HTML";
  return html;
}

// One labelled text input, pre-filled with |value|.
std::string TextField(const std::string& label,
                      const std::string& name,
                      const std::string& value) {
  return "<label class=\"field\">" + HtmlEscape(label) +
         "<input type=\"text\" name=\"" + name + "\" value=\"" +
         HtmlEscape(value) + "\"></label>\n";
}

// A colour picker inside a row of colours. |value| is "#RRGGBB".
std::string ColorField(const std::string& label,
                       const std::string& name,
                       const std::string& value) {
  return "<label class=\"field\">" + HtmlEscape(label) +
         "<input type=\"color\" name=\"" + name + "\" value=\"" +
         HtmlEscape(value) + "\"></label>\n";
}

std::string SelectField(
    const std::string& label,
    const std::string& name,
    const std::vector<std::pair<std::string, std::string>>& options,
    const std::string& selected) {
  std::string html = "<label class=\"field\">" + HtmlEscape(label) +
                     "<select name=\"" + name + "\">";
  for (const auto& [value, text] : options) {
    html += "<option value=\"" + value + "\"" +
            (value == selected ? " selected" : "") + ">" + HtmlEscape(text) +
            "</option>";
  }
  return html + "</select></label>\n";
}

// Shortcuts as "Name | address" lines, the same form the file uses.
std::string ShortcutText(const std::vector<Shortcut>& shortcuts) {
  std::string text;
  for (const Shortcut& shortcut : shortcuts) {
    text += shortcut.name + " | " + shortcut.url + "\n";
  }
  return text;
}

std::string BuildSettingsHtml(const Settings& settings,
                              const std::string& message,
                              bool is_error) {
  std::string html = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Settings</title>
<style>)HTML";
  html += kStyle;
  html += kSettingsStyle;
  html += ":root { --accent: " + settings.accent_color + "; }\n";
  html += R"HTML(</style>
</head>
<body>
<div class="backdrop" style="background: )HTML";
  html += HtmlEscape(settings.home_background);
  html += R"HTML("></div>
<main class="settings">
  <header class="brand">
    )HTML";
  html += kPawSvg;
  html += R"HTML(
    <h1>Settings</h1>
  </header>
)HTML";

  if (!message.empty()) {
    html += std::string("<p class=\"notice") + (is_error ? " error" : "") +
            "\">" + HtmlEscape(message) + "</p>\n";
  }

  html += R"HTML(<form class="settings-form" method="post" action="pawnet://settings/save">
)HTML";
  html += TextField("Home page", "home_page", settings.home_page);
  html += TextField("Search address (the query is appended)", "search_url",
                    settings.search_url);
  html += TextField("Home background (any CSS value)", "home_background",
                    settings.home_background);
  html += SelectField("Home layout", "home_layout",
                      {{"centered", "Centred"}, {"top", "At the top"}},
                      settings.home_layout == HomeLayout::kTop ? "top"
                                                               : "centered");
  html += SelectField("Greeting and clock", "home_greeting",
                      {{"on", "Shown"}, {"off", "Hidden"}},
                      settings.home_greeting ? "on" : "off");
  html += "<div class=\"colors\">\n";
  html += ColorField("Accent", "accent_color", settings.accent_color);
  html += ColorField("Toolbar", "toolbar_color",
                     ColorToHex(settings.toolbar_color));
  html += ColorField("Active tab text", "active_tab_color",
                     ColorToHex(settings.active_tab_color));
  html += ColorField("Other tabs text", "inactive_tab_color",
                     ColorToHex(settings.inactive_tab_color));
  html += "</div>\n";
  html += "<label class=\"field\">Shortcuts (one \"Name | address\" per line)"
          "<textarea name=\"shortcuts\">" +
          HtmlEscape(ShortcutText(settings.shortcuts)) + "</textarea></label>\n";
  html += R"HTML(<button class="save" type="submit">Save</button>
</form>
<p class="links"><a href="pawnet://home">Back to home</a></p>
</main>
</body>
</html>
)HTML";
  return html;
}

}  // namespace

PawNetSchemeHandlerFactory::PawNetSchemeHandlerFactory(const Settings& settings)
    : settings_(settings) {}

void PawNetSchemeHandlerFactory::SetSettingsListener(SettingsListener listener) {
  std::lock_guard<std::mutex> lock(mutex_);
  listener_ = std::move(listener);
}

Settings PawNetSchemeHandlerFactory::CurrentSettings() {
  std::lock_guard<std::mutex> lock(mutex_);
  return settings_;
}

CefRefPtr<CefResourceHandler> PawNetSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& scheme_name,
    CefRefPtr<CefRequest> request) {
  CefURLParts parts;
  if (!CefParseURL(request->GetURL(), parts)) {
    return nullptr;
  }
  const std::string host = CefString(&parts.host).ToString();
  const std::string path = CefString(&parts.path).ToString();
  const std::string query = CefString(&parts.query).ToString();

  if (host == "home") {
    return HtmlResponse(BuildHomeHtml(CurrentSettings()));
  }
  if (host == "settings" && path == "/save") {
    return HandleSave(request);
  }
  if (host == "settings") {
    const bool saved = query.find("saved=1") != std::string::npos;
    return HtmlResponse(BuildSettingsHtml(
        CurrentSettings(),
        saved ? "Saved. Open home pages now use the new settings." : "",
        false));
  }
  return nullptr;
}

CefRefPtr<CefResourceHandler> PawNetSchemeHandlerFactory::HandleSave(
    CefRefPtr<CefRequest> request) {
  const std::map<std::string, std::string> form =
      ParseForm(ReadPostBody(request));

  // Start from the current settings, so fields missing from the form keep
  // their values. Every value that is present is checked the same way as the
  // configuration file.
  Settings updated = CurrentSettings();
  std::vector<std::string> errors;
  const char* const kSimpleKeys[] = {
      "home_page",         "search_url",        "home_background",
      "accent_color",      "home_layout",       "home_greeting",
      "toolbar_color",     "active_tab_color",  "inactive_tab_color",
  };
  for (const char* key : kSimpleKeys) {
    const auto field = form.find(key);
    if (field != form.end() && !SetSetting(key, field->second, &updated)) {
      errors.push_back(std::string(key) + " has an invalid value.");
    }
  }

  const auto shortcuts = form.find("shortcuts");
  if (shortcuts != form.end()) {
    std::vector<Shortcut> parsed;
    size_t start = 0;
    const std::string& text = shortcuts->second;
    while (start < text.size()) {
      size_t end = text.find_first_of("\r\n", start);
      if (end == std::string::npos) {
        end = text.size();
      }
      const std::string line = text.substr(start, end - start);
      start = end + 1;
      const size_t first = line.find_first_not_of(" \t");
      if (first == std::string::npos) {
        continue;
      }
      Shortcut shortcut;
      if (ParseShortcut(line, &shortcut)) {
        parsed.push_back(shortcut);
      } else {
        errors.push_back("Shortcut line needs \"Name | address\": " + line);
      }
    }
    updated.shortcuts = parsed;
  }

  if (!errors.empty()) {
    std::string message;
    for (const std::string& error : errors) {
      message += error + " ";
    }
    return HtmlResponse(BuildSettingsHtml(updated, message, true));
  }

  std::string error;
  if (!SaveSettings(updated, &error)) {
    return HtmlResponse(BuildSettingsHtml(updated, error, true));
  }

  SettingsListener listener;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = updated;
    listener = listener_;
  }
  if (listener) {
    CefPostTask(TID_UI, new CallbackTask([listener, updated] {
                  listener(updated);
                }));
  }
  return RedirectResponse("pawnet://settings?saved=1");
}
