// ADR-109: does an edit land where the human's model says it lands?
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <clap/clap.h>
#include "../src/hypersaw_clap_entry.h"
#include "../src/hypersaw_debug.h"
#include <string>
#include <cstdlib>
namespace { 
#include "notefuzz_scaffold.inc"
}
int main()
{
  hypersaw_entry_init("");
  auto *factory=(const clap_plugin_factory_t*)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p=factory->create_plugin(factory,&kHost,"com.lifted-truck.hypersaw");
  p->init(p); p->activate(p,44100.0,32,256); p->start_processing(p);
  std::vector<float> L(256),R(256);
  float*chans[2]={L.data(),R.data()};
  clap_audio_buffer_t out{}; out.data32=chans; out.channel_count=2;
  clap_process_t proc{}; proc.frames_count=256; proc.audio_outputs=&out;
  proc.audio_outputs_count=1; proc.out_events=&kOut;
  auto ev=[&](clap_id id,double v){ EvList e; e.params.push_back(mkParam(id,v));
    e.finalize(); proc.in_events=&e.list; p->process(p,&proc); };
  auto pump=[&](int n){ for(int b=0;b<n;b++){ EvList e; e.finalize(); proc.in_events=&e.list; p->process(p,&proc);} };
  auto *par=(const clap_plugin_params_t*)p->get_extension(p, CLAP_EXT_PARAMS);
  auto get=[&](clap_id id){ double v=-1; par->get_value(p,id,&v); return v; };
  int bad=0;
  const clap_id DET=4;                      // detune, morphable + continuous

  ev(151,1);                                 // morph ON, corners hold defaults
  ev(152,0); ev(153,0);                      // pad at corner A
  pump(120);
  // 1. UNARMED edit must STICK (the v1 seam this closes)
  ev(DET,0.61); pump(200);
  const double stuck=get(DET);
  std::printf("unarmed edit sticks:      %.3f (want 0.610)  %s\n", stuck,
              std::fabs(stuck-0.61)<0.02?"OK":"FAIL");
  if(std::fabs(stuck-0.61)>0.02) bad++;

  // 2. ARMED at corner D, edit, then move the pad to D: the value must appear
  ev(159,4); ev(DET,0.23); pump(60);
  ev(152,1); ev(153,1);                      // pad to corner D
  pump(400);
  const double atD=get(DET);
  std::printf("armed edit lands in D:    %.3f (want 0.230)  %s\n", atD,
              std::fabs(atD-0.23)<0.03?"OK":"FAIL");
  if(std::fabs(atD-0.23)>0.03) bad++;

  // 3. EXEMPT: the field must stop touching it
  ev(159,0);
  hypersaw_debug_exempt(p, DET);
  ev(DET,0.42); 
  ev(152,0); ev(153,0);                      // slam the pad to the far corner
  pump(500);
  const double held=get(DET);
  std::printf("exempt holds its value:   %.3f (want 0.420)  %s\n", held,
              std::fabs(held-0.42)<0.01?"OK":"FAIL");
  if(std::fabs(held-0.42)>0.01) bad++;
  // 4. ADR-109 A1: the globals the human's scan found must now toggle
  const clap_id SCAN[] = {75, 11, 70, 32, 34, 38, 90, 116};
  const char *NAMES[] = {"freqGlide","inertia","inertiaCurve","voiceMono",
                         "voiceLegato","pitchBend","glideMode","scaleRoot"};
  int notToggling = 0;
  for (int i = 0; i < 8; i++)
  {
    const bool on = hypersaw_debug_exempt(p, SCAN[i]);
    if (!on) { std::printf("  %s did NOT toggle\n", NAMES[i]); notToggling++; }
    else hypersaw_debug_exempt(p, SCAN[i]);   // put it back
  }
  std::printf("scanned globals toggle:   %d/8 %s\n", 8-notToggling, notToggling?"FAIL":"OK");
  bad += notToggling ? 1 : 0;

  // 5. the scale exempts COLLECTIVELY: one degree toggles all thirteen
  hypersaw_debug_exempt(p, 120);            // a middle degree
  const std::string ex = hypersaw_debug_exemptjson(p);
  int inScale = 0;
  for (int id = 116; id <= 128; id++)
    if (ex.find("\"" + std::to_string(id) + "\"") != std::string::npos) inScale++;
  std::printf("scale exempts as a unit:  %d/13 %s\n", inScale, inScale==13?"OK":"FAIL");
  if (inScale != 13) bad++;

  // 6. ADR-110: the owners map that drives the GUI's corner colours.
  //    MUST-READ-ZERO CONTROL FIRST (L0024/L0032): every later assertion here
  //    is satisfied by an EMPTY map, so without this the whole case is a check
  //    that cannot fail. The map has to be big before its contents mean
  //    anything.
  auto ownerOf = [&](const std::string &js, clap_id id) -> int {
    const std::string key = "\"" + std::to_string(id) + "\":";
    const size_t at = js.find(key);
    return at == std::string::npos ? -99 : std::atoi(js.c_str() + at + key.size());
  };
  {
    // clear the exemptions case 4/5 left behind, so this case reads a clean field
    hypersaw_debug_exempt(p, 120);          // scale back in (toggles all 13)
    hypersaw_debug_exempt(p, DET);          // detune back in
    ev(151, 1); pump(200);                  // morph ON

    const std::string ow = hypersaw_debug_ownersjson(p);
    int members = 0;
    for (char c : ow) if (c == ':') members++;
    std::printf("owners map is populated:  %d entries %s\n", members,
                members > 40 ? "OK" : "FAIL");
    if (members <= 40) bad++;

    // a live parameter names a REAL corner, not the "nobody" sentinel
    const int k = ownerOf(ow, DET);
    std::printf("live param names a corner: %d (want 0..3)  %s\n", k,
                (k >= 0 && k <= 3) ? "OK" : "FAIL");
    if (k < 0 || k > 3) bad++;

    // ...and an exempt one names nobody: the GUI must not tint it as owned
    hypersaw_debug_exempt(p, DET);
    const int ke = ownerOf(hypersaw_debug_ownersjson(p), DET);
    std::printf("exempt param owned by -1: %d (want -1)     %s\n", ke,
                ke == -1 ? "OK" : "FAIL");
    if (ke != -1) bad++;

    // with the field OFF nobody owns anything, but MEMBERSHIP survives: the
    // menu decides whether "Exempt" applies from the key's presence, so an
    // empty map here would silently hide the item whenever morph is off.
    hypersaw_debug_exempt(p, DET);
    ev(151, 0); pump(200);
    const std::string off = hypersaw_debug_ownersjson(p);
    int offMembers = 0;
    for (char c : off) if (c == ':') offMembers++;
    const int ko = ownerOf(off, DET);
    std::printf("field off: %d keys, own %d (want %d keys, -1)  %s\n",
                offMembers, ko, members,
                (offMembers == members && ko == -1) ? "OK" : "FAIL");
    if (offMembers != members || ko != -1) bad++;
  }

  // 7. THE COLOUR MUST NOT LIE. Cases 6's range check (0..3) would pass on a
  //    constant, a stale read, or an uninitialised zero — it inherits whatever
  //    that one sample happened to be (L0039). The claim the stripe actually
  //    makes to the eye is stronger and testable: "this row is showing corner
  //    k's value". So author two corners apart, then at each pad corner require
  //    the reported owner AND the sounding value to agree.
  {
    ev(151, 1); pump(100);
    ev(159, 1); ev(DET, 0.11); pump(60);      // author corner A
    ev(159, 4); ev(DET, 0.88); pump(60);      // author corner D
    ev(159, 0);
    struct { double x, y; int want; double val; } spot[] = {
      {0, 0, 0, 0.11}, {1, 1, 3, 0.88},
    };
    int lying = 0;
    for (auto &t : spot)
    {
      ev(152, t.x); ev(153, t.y); pump(500);
      const int k = ownerOf(hypersaw_debug_ownersjson(p), DET);
      const double v = get(DET);
      const bool ok = (k == t.want) && std::fabs(v - t.val) < 0.03;
      std::printf("  pad(%.0f,%.0f): owner %d val %.3f (want %d / %.2f)  %s\n",
                  t.x, t.y, k, v, t.want, t.val, ok ? "OK" : "FAIL");
      if (!ok) lying++;
    }
    std::printf("colour matches the sound: %d/2 %s\n", 2 - lying, lying ? "FAIL" : "OK");
    if (lying) bad++;
  }

  // 8. ADR-111: the corner-vals surface the armed view paints from must
  //    report what case 7 just authored — 0.11 into A, 0.88 into D. Binds the
  //    GUI's data source to the authored values, not merely to "some JSON".
  {
    auto valOf = [&](int corner) -> double {
      const std::string j = hypersaw_debug_cornervals(p, corner);
      const std::string key = "\"" + std::to_string(DET) + "\":";
      const size_t at = j.find(key);
      return at == std::string::npos ? -99.0 : std::atof(j.c_str() + at + key.size());
    };
    const double a = valOf(0), d = valOf(3);
    const bool ok = std::fabs(a-0.11) < 0.03 && std::fabs(d-0.88) < 0.03;
    std::printf("corner vals report edits: A %.3f D %.3f (want 0.110 / 0.880)  %s\n",
                a, d, ok?"OK":"FAIL");
    if(!ok) bad++;
  }

  p->stop_processing(p); p->deactivate(p); p->destroy(p);
  return bad;
}
