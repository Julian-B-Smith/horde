/*
 * embedded_page_policy_check — the GUI's page-only rule, by BEHAVIOUR (B446, ADR-194 D-S5).
 *
 * WIRED: ./verify fast, compiled and run by tools/embedded_page_policy_check.py
 *
 * WHAT. Drives hypersaw::detail::embeddedPagePolicy (src/gui/embedded_page_policy.h),
 * the function the Windows GUI hands to choc's allowNavigation, through fixed
 * request sequences and asserts each answer:
 *   - the first page load at the embedded URI is admitted; a second is refused;
 *   - a wrong URI first is refused WITHOUT using up the admission (the embedded
 *     page is still admitted after it);
 *   - a message before admission is refused; a message from any other URI is
 *     refused, including a URI that merely STARTS with the embedded one;
 *   - a message whose source differs only by a #fragment is delivered;
 *   - every frame and new-window request is refused, at any URI.
 * The enum is a stand-in with choc's four enumerator names
 * (tools/choc_patch_check verifies choc has exactly those), so this runs with no
 * choc checkout and no build tree.
 *
 * MUST-FAIL CONTROLS. The same sequences run against four faulty policies, and
 * each must FAIL at least one row: accept-any-source (messages not tied to the
 * page), dropped latch (the page admitted every time), prefix matching, and
 * exact-match messages (a #fragment change cuts the page off). A sequence a
 * faulty policy passes proves nothing, so a control that passes is RED.
 *
 * PERMISSIONS (ADR-196). Drives hypersaw::detail::webPermissionPolicy, the
 * function the Windows GUI hands to choc's allowPermission, with every one of
 * choc's seven PermissionKind names (a stand-in enum again; choc_patch_check
 * verifies the names) and asserts each is refused, clipboard reads first. Two
 * more must-fail controls: upstream choc's own rule (clipboard reads granted)
 * and a deny-list of the known kinds (one WebView2 adds later is granted).
 *
 * NOT SHOWN. That WebView2 obeys the answer at run time (see the traces,
 * traces/2026-10-08-b446-choc-win-nav.md and
 * traces/2026-10-08-b446-choc-win-clipboard.md).
 */
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "../src/gui/embedded_page_policy.h"

namespace
{
enum class Nav { page, frame, newWindow, message };
using hypersaw::detail::EmbeddedPageState;
using hypersaw::detail::kEmbeddedPage;
using Policy = std::function<bool(EmbeddedPageState &, Nav, std::string_view)>;

struct Step
{
  Nav type;
  std::string uri;
  bool want;
  const char *what;
};

const std::string E(kEmbeddedPage);
const std::string kOther = "https://example.invalid/";

// Each sequence starts from a fresh state.
const std::vector<std::vector<Step>> kSequences = {
    {
        {Nav::page, E, true, "first page at the embedded URI"},
        {Nav::page, E, false, "a second page load (reload)"},
        {Nav::page, kOther, false, "a later page elsewhere"},
        {Nav::message, E, true, "a message from the admitted page"},
        {Nav::message, E + "#tab", true, "a message whose source differs by a #fragment"},
        {Nav::message, kOther, false, "a message from another URI"},
        {Nav::message, E + "X", false, "a message from a URI the embedded one prefixes"},
        {Nav::message, E + "?q=1", false, "a message from the embedded URI plus a query"},
        {Nav::message, "", false, "a message with no source"},
        {Nav::frame, E, false, "a frame at the embedded URI"},
        {Nav::frame, kOther, false, "a frame elsewhere"},
        {Nav::newWindow, E, false, "a new window at the embedded URI"},
        {Nav::newWindow, kOther, false, "a new window elsewhere"},
    },
    {
        {Nav::message, E, false, "a message before admission"},
        {Nav::page, kOther, false, "a wrong URI first"},
        {Nav::page, E + "X", false, "a prefixed URI first"},
        {Nav::page, E + "#x", false, "the embedded URI with a fragment, as a page load"},
        {Nav::message, E, false, "a message after refused loads only"},
        {Nav::page, E, true, "the embedded page, after refused loads (no latch spent)"},
        {Nav::message, E, true, "a message once admitted"},
    },
    {
        {Nav::frame, E, false, "a frame before any page"},
        {Nav::newWindow, E, false, "a new window before any page"},
        {Nav::page, E, true, "the page, after a frame and a new window were refused"},
    },
};

// The number of rows `policy` gets wrong; prints them when `loud`.
int run(const Policy &policy, bool loud)
{
  int wrong = 0;
  for (const auto &seq : kSequences)
  {
    EmbeddedPageState state;
    for (const auto &s : seq)
    {
      const bool got = policy(state, s.type, s.uri);
      if (got != s.want)
      {
        ++wrong;
        if (loud)
          std::printf("  FAIL  %s: got %s, want %s\n", s.what, got ? "allow" : "refuse",
                      s.want ? "allow" : "refuse");
      }
    }
  }
  return wrong;
}

size_t rowCount()
{
  size_t n = 0;
  for (const auto &seq : kSequences) n += seq.size();
  return n;
}

// Faulty policies, each one defect away from the real rule.
bool acceptAnySource(EmbeddedPageState &s, Nav t, std::string_view uri)
{
  if (t == Nav::message) return s.pageAdmitted;
  return hypersaw::detail::embeddedPagePolicy(s, t, uri);
}
bool droppedLatch(EmbeddedPageState &s, Nav t, std::string_view uri)
{
  if (t == Nav::page)
  {
    s.pageAdmitted = uri == kEmbeddedPage;   // admitted again on every load
    return uri == kEmbeddedPage;
  }
  return hypersaw::detail::embeddedPagePolicy(s, t, uri);
}
bool prefixMatch(EmbeddedPageState &s, Nav t, std::string_view uri)
{
  const bool prefixed = uri.substr(0, kEmbeddedPage.size()) == kEmbeddedPage;
  if (t == Nav::page)
  {
    if (s.pageAdmitted || !prefixed) return false;
    s.pageAdmitted = true;
    return true;
  }
  if (t == Nav::message) return s.pageAdmitted && prefixed;
  return false;
}
bool exactMessage(EmbeddedPageState &s, Nav t, std::string_view uri)
{
  if (t == Nav::message) return s.pageAdmitted && uri == kEmbeddedPage;
  return hypersaw::detail::embeddedPagePolicy(s, t, uri);
}
// Permissions: choc's seven PermissionKind names, every one refused.
enum class Perm { clipboardRead, microphone, camera, geolocation, notifications, otherSensors, other };
using PermPolicy = bool (*)(Perm);
const struct { Perm kind; const char *what; } kPermRows[] = {
    {Perm::clipboardRead, "clipboard read"}, {Perm::microphone, "microphone"},
    {Perm::camera, "camera"},                {Perm::geolocation, "geolocation"},
    {Perm::notifications, "notifications"},  {Perm::otherSensors, "other sensors"},
    {Perm::other, "a kind choc does not list"},
};
constexpr size_t kPermRowCount = sizeof(kPermRows) / sizeof(kPermRows[0]);

int runPerm(PermPolicy policy, bool loud)
{
  int wrong = 0;
  for (const auto &r : kPermRows)
    if (policy(r.kind))
    {
      ++wrong;
      if (loud) std::printf("  FAIL  permission %s: granted, want refused\n", r.what);
    }
  return wrong;
}

bool realPermPolicy(Perm k) { return hypersaw::detail::webPermissionPolicy(k); }
bool upstreamChoc(Perm k) { return k == Perm::clipboardRead; }   // choc's handler, option unset
bool denyListOnly(Perm k) { return k == Perm::other; }           // known kinds refused, new ones granted
}  // namespace

int main()
{
  int failures = 0;
  std::printf("embedded_page_policy_check:\n");
  const int wrong = run([](EmbeddedPageState &s, Nav t, std::string_view u) {
    return hypersaw::detail::embeddedPagePolicy(s, t, u);
  }, true);
  std::printf("  %s  the rule: %zu rows, %d wrong\n", wrong ? "FAIL" : "PASS", rowCount(), wrong);
  failures += wrong;

  const struct { const char *name; Policy p; } controls[] = {
      {"accept-any-source", acceptAnySource},
      {"dropped latch", droppedLatch},
      {"prefix matching", prefixMatch},
      {"exact-match messages (fragment)", exactMessage},
  };
  for (const auto &c : controls)
  {
    const int w = run(c.p, false);
    std::printf("  %s  control %s: %d row(s) caught it\n", w ? "PASS" : "FAIL", c.name, w);
    if (w == 0) ++failures;
  }
  const int permWrong = runPerm(realPermPolicy, true);
  std::printf("  %s  the permission rule: %zu rows, %d wrong\n", permWrong ? "FAIL" : "PASS",
              kPermRowCount, permWrong);
  failures += permWrong;
  const struct { const char *name; PermPolicy p; } permControls[] = {
      {"upstream choc (clipboard reads granted)", upstreamChoc},
      {"deny-list of known kinds", denyListOnly},
  };
  for (const auto &c : permControls)
  {
    const int w = runPerm(c.p, false);
    std::printf("  %s  control %s: %d row(s) caught it\n", w ? "PASS" : "FAIL", c.name, w);
    if (w == 0) ++failures;
  }

  if (failures)
  {
    std::printf("embedded_page_policy_check: RED (%d)\n", failures);
    return 1;
  }
  std::printf("embedded_page_policy_check: GREEN (%zu + %zu rows; 6 controls red as designed)\n", rowCount(),
              kPermRowCount);
  return 0;
}
