/*
 * statefix_common.h — the ONE stub host, ONE loader per transport and ONE
 * render shared by gen_state_fixtures (writes the corpus) and statefix_check
 * (asserts it). Single-sourced on purpose (L0005): a golden rendered by one
 * render and checked by another would certify the difference between the two
 * renders, not the patch. Not a library — two host-side tools' common scaffold.
 *
 * Transports:
 *   chunk — the CLAP state extension's `hypersaw-state N` text (what a DAW
 *           session stores). Loaded IDLE, so the shell applies directly.
 *   json  — the preset path (`{"plugin":"HYPERSAW","schema":N,...}`), reached
 *           through the headless debug export exactly as the GUI's load button
 *           reaches it. The apply is QUEUED; an empty params.flush drains it,
 *           which is what a host's request_flush round trip does.
 *
 * Render: 1 s of a held A3 (172 blocks of 256 at 44.1 kHz — the same numbers
 * state_check's renderHash uses), interleaved L/R float32. The golden IS this
 * stream; bit-identity is the assertion, so the numbers here are frozen.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <clap/clap.h>

#include "../src/hypersaw_clap_entry.h"

extern "C" bool hypersaw_debug_apply(const clap_plugin_t *, const char *);
extern "C" void hypersaw_debug_state(const clap_plugin_t *, char *, uint32_t);
extern "C" int hypersaw_debug_engine_revision(const clap_plugin_t *);

namespace statefix
{

constexpr double kSampleRate = 44100.0;
constexpr uint32_t kBlock = 256;
constexpr int kBlocks = 172;   // ~1 s
constexpr int16_t kKey = 57;   // A3
constexpr size_t kFrames = (size_t)kBlocks * kBlock;
constexpr size_t kFloats = kFrames * 2;   // interleaved L/R

/* ---- stub host ---- */
inline const void *host_get_extension(const clap_host_t *, const char *) { return nullptr; }
inline void host_noop(const clap_host_t *) {}
inline const clap_host_t kHost = {CLAP_VERSION, nullptr, "statefix", "", "", "1.0",
                                  host_get_extension, host_noop, host_noop, host_noop};

/* ---- string-backed streams ---- */
struct OStr
{
  clap_ostream_t s;
  std::string data;
};
inline int64_t ostr_write(const clap_ostream_t *s, const void *buf, uint64_t n)
{
  auto *o = (OStr *)s;
  o->data.append((const char *)buf, (size_t)n);
  return (int64_t)n;
}
struct IStr
{
  clap_istream_t s;
  std::string data;
  size_t pos = 0;
};
inline int64_t istr_read(const clap_istream_t *s, void *buf, uint64_t n)
{
  auto *i = (IStr *)s;
  const size_t take = std::min((size_t)n, i->data.size() - i->pos);
  std::memcpy(buf, i->data.data() + i->pos, take);
  i->pos += take;
  return (int64_t)take;
}

/* ---- event lists ---- */
struct EvList
{
  clap_input_events_t list;
  std::vector<clap_event_param_value_t> evs;
  EvList()
  {
    list.ctx = this;
    list.size = [](const clap_input_events_t *l) -> uint32_t {
      return (uint32_t)((EvList *)l->ctx)->evs.size();
    };
    list.get = [](const clap_input_events_t *l, uint32_t i) -> const clap_event_header_t * {
      return &((EvList *)l->ctx)->evs[i].header;
    };
  }
  void push(clap_id id, double v)
  {
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.param_id = id;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = v;
    evs.push_back(ev);
  }
};
inline bool oev_try_push(const clap_output_events_t *, const clap_event_header_t *) { return true; }
inline const clap_output_events_t kOut = {nullptr, oev_try_push};

inline const clap_plugin_t *makePlugin()
{
  auto *factory =
      (const clap_plugin_factory_t *)hypersaw_entry_get_factory(CLAP_PLUGIN_FACTORY_ID);
  const clap_plugin_t *p =
      factory->create_plugin(factory, &kHost, "com.lifted-truck.hypersaw");
  p->init(p);
  return p;
}

inline const clap_plugin_params_t *paramsOf(const clap_plugin_t *p)
{
  return (const clap_plugin_params_t *)p->get_extension(p, CLAP_EXT_PARAMS);
}
inline const clap_plugin_state_t *stateOf(const clap_plugin_t *p)
{
  return (const clap_plugin_state_t *)p->get_extension(p, CLAP_EXT_STATE);
}

/* Drain the shell's param queue the way a host's request_flush does. Four
   passes is state_check's number; one is enough, the rest are free. */
inline void drain(const clap_plugin_t *p)
{
  EvList none;
  for (int i = 0; i < 4; i++) paramsOf(p)->flush(p, &none.list, &kOut);
}

inline bool loadChunk(const clap_plugin_t *p, const std::string &blob)
{
  IStr in;
  in.s.ctx = &in;
  in.s.read = istr_read;
  in.data = blob;
  return stateOf(p)->load(p, &in.s);
}

inline bool loadJson(const clap_plugin_t *p, const std::string &json)
{
  const bool ok = hypersaw_debug_apply(p, json.c_str());
  drain(p);
  return ok;
}

inline std::string saveChunk(const clap_plugin_t *p)
{
  OStr out;
  out.s.ctx = &out;
  out.s.write = ostr_write;
  if (!stateOf(p)->save(p, &out.s)) return "";
  return out.data;
}

inline std::string saveJson(const clap_plugin_t *p)
{
  static char buf[1 << 17];
  hypersaw_debug_state(p, buf, sizeof buf);
  return buf;
}

/* One second of a held A3 through the CLAP process path. The plugin must be
   idle (not activated) on entry; it is idle again on exit. */
inline void render(const clap_plugin_t *p, std::vector<float> &out)
{
  p->activate(p, kSampleRate, 32, 1024);
  p->start_processing(p);
  std::vector<float> L(kBlock), R(kBlock);
  float *chans[2] = {L.data(), R.data()};
  clap_audio_buffer_t ob{};
  ob.data32 = chans;
  ob.channel_count = 2;

  struct NoteList
  {
    clap_input_events_t list;
    clap_event_note_t ev{};
    bool sent = false;
  } notes;
  notes.ev.header.size = sizeof(notes.ev);
  notes.ev.header.type = CLAP_EVENT_NOTE_ON;
  notes.ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  notes.ev.note_id = -1;
  notes.ev.port_index = 0;
  notes.ev.channel = 0;
  notes.ev.key = kKey;
  notes.ev.velocity = 1.0;
  notes.list.ctx = &notes;
  notes.list.size = [](const clap_input_events_t *l) -> uint32_t {
    return ((NoteList *)l->ctx)->sent ? 0u : 1u;
  };
  notes.list.get = [](const clap_input_events_t *l, uint32_t) -> const clap_event_header_t * {
    return &((NoteList *)l->ctx)->ev.header;
  };
  EvList empty;

  clap_process_t proc{};
  proc.frames_count = kBlock;
  proc.audio_outputs = &ob;
  proc.audio_outputs_count = 1;
  proc.out_events = &kOut;

  out.assign(kFloats, 0.0f);
  size_t w = 0;
  for (int block = 0; block < kBlocks; block++)
  {
    proc.in_events = block == 0 ? &notes.list : &empty.list;
    p->process(p, &proc);
    if (block == 0) notes.sent = true;
    for (uint32_t i = 0; i < kBlock; i++)
    {
      out[w++] = L[i];
      out[w++] = R[i];
    }
  }
  p->stop_processing(p);
  p->deactivate(p);
}

inline double rms(const std::vector<float> &v)
{
  double acc = 0;
  for (float x : v) acc += (double)x * x;
  return v.empty() ? 0 : std::sqrt(acc / (double)v.size());
}

/* ---- files ---- */
inline bool readFile(const std::string &path, std::string &out)
{
  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  out.clear();
  char buf[65536];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
  std::fclose(f);
  return true;
}
inline bool writeFile(const std::string &path, const void *data, size_t n)
{
  FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const bool ok = std::fwrite(data, 1, n, f) == n;
  std::fclose(f);
  return ok;
}

/* A fixture's transport is its extension: .txt is a chunk, .json a preset. */
inline bool isChunk(const std::string &name)
{
  return name.size() > 4 && name.compare(name.size() - 4, 4, ".txt") == 0;
}
inline bool isJson(const std::string &name)
{
  return name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0;
}
inline std::string goldenFor(const std::string &fixturePath)
{
  return fixturePath.substr(0, fixturePath.rfind('.')) + ".f32";
}

}  // namespace statefix
