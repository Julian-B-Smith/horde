// Where does the SHELL spend its time? cpu_bench times SwarmCore alone (1.8% at
// 7x8), yet the human measures ~40% in the DAW — so the cost lives between
// process() and the cores, and this measures exactly that path with suspects
// toggled one at a time.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <vector>
#include <clap/clap.h>
#include <string>
#include "../src/hypersaw_clap_entry.h"
namespace {
#include "notefuzz_scaffold.inc"
}
int main()
{
  hypersaw_entry_init("");
  auto *factory=(const clap_plugin_factory_t*)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  struct Sc { const char*name; std::vector<std::pair<clap_id,double>> params; };
  const Sc scen[] = {
    {"default patch, idle (no notes)",            {}},
    {"default patch, 8 held notes",               {}},
    {"16 voices/osc, 8 notes",                    {{1,16}}},
    {"+ bend law on (const-time)",                {{1,16},{106,1}}},
    // osc2 needs BOTH enable (1150) and volume (1017): ADR-100 ships it OFF,
    // so a volume write alone still measures one oscillator.
    {"+ osc2 audible (enable + vol 0.4)",         {{1,16},{106,1},{1150,1},{1017,0.4}}},
    {"+ drift + width + tone tilt",               {{1,16},{106,1},{1150,1},{1017,0.4},{9,30},{14,1.5},{66,0.5}}},
    {"osc2 vol=0 (default) but 16v",              {{1,16}}},
    // The human's hypothesis (2026-08-20): the ADR-094 saw-shape section is the
    // eater. Each stage alone, then all four, against the same 16v/8-note base.
    {"16v + sawBase 0.5",                         {{1,16},{129,0.5}}},
    {"16v + roundness 0.5 (profile 0.5)",         {{1,16},{130,0.5},{131,0.5}}},
    {"16v + roundHi 0.5 (round 0.5)",             {{1,16},{131,0.5},{132,0.5}}},
    {"16v + ALL saw-shape at 0.5",                {{1,16},{129,0.5},{130,0.5},{131,0.5},{132,0.5}}},
    {"16v + squareness (id 69) 0.5, for scale",   {{1,16},{69,0.5}}},
  };
  const double sr=44100.0; const uint32_t BLK=128;
  for (const auto &sc : scen)
  {
    const clap_plugin_t *p=factory->create_plugin(factory,&kHost,"com.lifted-truck.hypersaw");
    p->init(p); p->activate(p,sr,32,BLK); p->start_processing(p);
    std::vector<float> L(BLK),R(BLK);
    float*chans[2]={L.data(),R.data()};
    clap_audio_buffer_t out{}; out.data32=chans; out.channel_count=2;
    clap_process_t proc{}; proc.frames_count=BLK; proc.audio_outputs=&out;
    proc.audio_outputs_count=1; proc.out_events=&kOut;
    auto once=[&](EvList&e){e.finalize();proc.in_events=&e.list;p->process(p,&proc);};
    { EvList e; for(auto&pv:sc.params) e.params.push_back(mkParam(pv.first,pv.second)); once(e); }
    const bool notes = std::string(sc.name).find("idle")==std::string::npos;
    if (notes)
      for(int k=0;k<8;k++){ EvList e; e.notes.push_back(mkNote(CLAP_EVENT_NOTE_ON,0,(int16_t)(48+k*3),k+1)); once(e); }
    // warm up, then time 4 seconds of audio
    for(int b=0;b<200;b++){ EvList e; once(e); }
    const int NB=(int)(4.0*sr/BLK);
    auto t0=std::chrono::steady_clock::now();
    for(int b=0;b<NB;b++){ EvList e; once(e); }
    auto t1=std::chrono::steady_clock::now();
    const double cpu=std::chrono::duration<double>(t1-t0).count();
    const double audio=NB*(double)BLK/sr;
    std::printf("%-38s %6.3f s cpu / %.1f s audio  = %5.2f%% of a core\n",
                sc.name, cpu, audio, 100.0*cpu/audio);
    p->stop_processing(p); p->deactivate(p); p->destroy(p);
  }
  return 0;
}
