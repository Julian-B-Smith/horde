/*
 * embedded_page_policy.h — the GUI web view's page-only rule as a pure function
 * (B446, ADR-194 D-S5). Standard library only, so its behavioural check
 * (tools/embedded_page_policy_check) compiles in `./verify fast` with no choc
 * checkout and no build tree. Included through hypersaw_gui_common.h.
 *
 * THE RULE. The view shows one document, the embedded page, and only that page
 * reaches the bridge:
 *   page       exactly one top-level load is admitted, at kEmbeddedPage exactly;
 *              every later one is refused, a reload included. A refused first
 *              load (another URI) does not use up the admission.
 *   message    delivered only once the page is admitted, and only from it.
 *   frame      always refused.
 *   newWindow  always refused.
 * URIs compare EXACTLY, never by prefix: "…getHTMLInternalX" and
 * "…getHTMLInternal?q" are other documents.
 *
 * FRAGMENT. A message's source is the sending document's URL as WebView2
 * reports it, and a same-document change (location.hash) alters that URL
 * without changing the document; so a message's source is compared with its
 * #fragment removed. The page itself never makes such changes
 * (tools/gui_sink_check forbids them in src/gui/gui*.html), so this is a margin,
 * not a feature. macOS checks the main frame instead (hypersaw_gui.mm).
 *
 * Templated on the kind enum so the check can drive it with a stand-in that has
 * choc's four enumerator names; Windows instantiates it with
 * choc::ui::WebView::Options::NavigationType (the patched option, libs/patches).
 * tools/choc_patch_check verifies those names and that kEmbeddedPage is where
 * choc serves the page on Windows.
 */
#pragma once

#include <string_view>

namespace hypersaw::detail
{

// Where choc's setHTML serves the page on Windows: getURIHome() with no custom
// scheme ("https://choc.localhost/") plus "getHTMLInternal".
constexpr std::string_view kEmbeddedPage = "https://choc.localhost/getHTMLInternal";

struct EmbeddedPageState
{
  bool pageAdmitted = false;
};

constexpr std::string_view withoutFragment(std::string_view uri)
{
  const auto hash = uri.find('#');
  return hash == std::string_view::npos ? uri : uri.substr(0, hash);
}

template <typename NavigationType>
bool embeddedPagePolicy(EmbeddedPageState &state, NavigationType type, std::string_view uri)
{
  switch (type)
  {
    case NavigationType::page:
      if (state.pageAdmitted || uri != kEmbeddedPage) return false;
      state.pageAdmitted = true;
      return true;
    case NavigationType::message:
      return state.pageAdmitted && withoutFragment(uri) == kEmbeddedPage;
    case NavigationType::frame:
    case NavigationType::newWindow:
      return false;
  }
  return false;   // a kind added upstream later is refused until reviewed
}

}  // namespace hypersaw::detail
