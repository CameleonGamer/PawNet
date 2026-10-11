#include "src/home_page.h"

#include <cctype>

#include "include/wrapper/cef_stream_resource_handler.h"

namespace {

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

// Letter shown in a shortcut's circle. Non-ASCII names get a neutral dot.
std::string Initial(const std::string& name) {
  if (name.empty() || static_cast<unsigned char>(name[0]) >= 0x80) {
    return "\xE2\x80\xA2";  // U+2022 BULLET
  }
  return std::string(1, static_cast<char>(std::toupper(name[0])));
}

std::string BuildHomeHtml(const Settings& settings) {
  std::string html = R"(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Home</title>
<style>
  :root { color-scheme: light; }
  html, body { margin: 0; min-height: 100%; }
  body {
    box-sizing: border-box;
    min-height: 100vh;
    padding: 16vh 16px 32px;
    display: flex;
    flex-direction: column;
    align-items: center;
    font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
    color: #202124;
    background-color: #FAFAFA;
  }
  h1 { font-size: 48px; font-weight: 600; margin: 0 0 24px; letter-spacing: -0.5px; }
  form { width: min(560px, 100%); }
  input {
    box-sizing: border-box;
    width: 100%;
    padding: 14px 20px;
    font: inherit;
    font-size: 17px;
    color: inherit;
    background: #FFFFFF;
    border: 1px solid #DADCE0;
    border-radius: 24px;
    outline: none;
    box-shadow: 0 1px 6px rgba(32, 33, 36, 0.15);
  }
  input:focus { border-color: #8AB4F8; box-shadow: 0 1px 8px rgba(32, 33, 36, 0.25); }
  .shortcuts {
    width: min(560px, 100%);
    margin-top: 40px;
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
    gap: 16px;
  }
  .shortcut {
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 16px 8px;
    border-radius: 12px;
    color: inherit;
    text-decoration: none;
    text-align: center;
    overflow: hidden;
  }
  .shortcut:hover, .shortcut:focus { background: rgba(32, 33, 36, 0.06); }
  .circle {
    width: 48px;
    height: 48px;
    margin-bottom: 8px;
    display: flex;
    align-items: center;
    justify-content: center;
    border-radius: 50%;
    background: #E8EAED;
    font-size: 20px;
    font-weight: 600;
  }
  .name {
    max-width: 100%;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font-size: 14px;
  }
</style>
</head>
<body style="background: )";
  html += HtmlEscape(settings.home_background);
  html += R"(">
<h1>PawNet</h1>
<form id="search" data-search=")";
  html += HtmlEscape(settings.search_url);
  html += R"(">
  <input name="q" type="search" placeholder="Search or enter a query" autocomplete="off" autofocus>
</form>
)";

  if (!settings.shortcuts.empty()) {
    html += "<nav class=\"shortcuts\">\n";
    for (const Shortcut& shortcut : settings.shortcuts) {
      html += "  <a class=\"shortcut\" href=\"" + HtmlEscape(shortcut.url) +
              "\"><span class=\"circle\">" + HtmlEscape(Initial(shortcut.name)) +
              "</span><span class=\"name\">" + HtmlEscape(shortcut.name) +
              "</span></a>\n";
    }
    html += "</nav>\n";
  }

  html += R"(<script>
  const form = document.getElementById('search');
  form.addEventListener('submit', (event) => {
    event.preventDefault();
    const query = form.q.value.trim();
    if (query) {
      window.location.href = form.dataset.search + encodeURIComponent(query);
    }
  });
</script>
</body>
</html>
)";
  return html;
}

}  // namespace

PawNetSchemeHandlerFactory::PawNetSchemeHandlerFactory(const Settings& settings)
    : home_html_(BuildHomeHtml(settings)) {}

CefRefPtr<CefResourceHandler> PawNetSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& scheme_name,
    CefRefPtr<CefRequest> request) {
  // Only pawnet://home is registered, so every request here is for that page.
  return new CefStreamResourceHandler(
      "text/html",
      CefStreamReader::CreateForData(const_cast<char*>(home_html_.data()),
                                     home_html_.size()));
}
