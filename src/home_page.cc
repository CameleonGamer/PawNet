#include "src/home_page.h"

#include <cctype>

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

// Styles are kept in one block so the page needs no other files. The accent
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

std::string BuildHomeHtml(const Settings& settings) {
  std::string html = R"(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Home</title>
<style>)";
  html += kStyle;
  // The accent was checked as "#RRGGBB" when the settings were read, so it is
  // safe to put it in the stylesheet.
  html += ":root { --accent: " + settings.accent_color + "; }\n";
  html += R"(</style>
</head>
<body>
<div class="backdrop" style="background: )";
  html += HtmlEscape(settings.home_background);
  html += R"("></div>
<main)";
  html += settings.home_layout == HomeLayout::kTop ? " class=\"top\"" : "";
  html += R"(>
  <header class="brand">
    )";
  html += kPawSvg;
  html += R"(
    <h1>PawNet</h1>
  </header>
)";

  if (settings.home_greeting) {
    html += R"(  <p class="greeting"><span id="greeting"></span><time id="clock" class="clock"></time></p>
)";
  }

  html += R"(  <form id="search" class="search" data-search=")";
  html += HtmlEscape(settings.search_url);
  html += R"(">
    <input name="q" type="search" placeholder="Search or enter a query" autocomplete="off" autofocus aria-label="Search">
  </form>
)";

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

  html += R"(</main>
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
