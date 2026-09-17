/*
 * swarmfx_clap.cpp — SWARMFX: the effects product line as a standalone
 * AUDIO-EFFECT plugin (Track E1.3). External audio in → the swarm-herded
 * frequency engines (resonator bank / notch swarm) → out. Separate target from
 * the instrument (its own factory/entry) so the instrument shell stays
 * untouched; it shares the parity-proven cores (filter_core.h, notch_core.h)
 * via their processExternal() path. Engine-select hosts all the swarm FX in
 * one plugin so experimental engines can be auditioned and discarded cheaply.
 *
 * Deliberately minimal for a first test: generic host-drawn params (no webview
 * yet), block-start param application, MIDI notes move the gravity center.
 */

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <clap/clap.h>
#include <algorithm>
#include <cstdlib>

#include "filter_core.h"
#include "notch_core.h"
#include "time_core.h"
#include "swarmfx_entry.h"

namespace
{

static const char *s_fx_features[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_FILTER,
                                      CLAP_PLUGIN_FEATURE_STEREO, nullptr};

static const clap_plugin_descriptor_t s_desc = {
    CLAP_VERSION_INIT,
    "com.lifted-truck.swarmfx",
    "SWARM-FX",
    "Lifted Truck",
    "https://github.com/Lifted-Truck/HYPERSAW",
    "",
    "",
    "0.1.0",
    "Coupled-oscillator swarm effects: resonator bank / notch swarm on external audio",
    &s_fx_features[0]};

static const char *const kEngineLabels[] = {"Resonator Bank", "Notch Voice", "Tap Delay",
                                            "FDN Room"};
static const char *const kDistLabels[] = {"even", "gaussian", "cauchy"};
static const char *const kPlaceLabels[] = {"ERB", "log", "harmonic"};

struct ParamDef
{
  clap_id id;
  const char *name;
  double minV, maxV, defV;
  bool stepped;
  const char *const *labels;
};

// One unified surface; applyParam maps each to whichever core is active.
static const ParamDef kParams[] = {
    {1, "Engine", 0, 3, 0, true, kEngineLabels},
    {2, "Pull K", -1, 1, 0, false, nullptr},
    {3, "Gravity", 0, 1, 0, false, nullptr},
    {4, "Basin (c)", 15, 90, 45, false, nullptr},
    {5, "Drift Depth (c)", 0, 200, 0, false, nullptr},
    {6, "Drift Rate", 0, 1, 0.4, false, nullptr},
    {7, "Inertia", 0, 1, 0, false, nullptr},
    {8, "Center", 7.0, 11.3, 9.64, false, nullptr},  // log2 Hz
    {9, "Spread", 0, 1, 0.5, false, nullptr},
    {10, "Distribution", 0, 2, 0, true, kDistLabels},
    {11, "Placement", 0, 2, 0, true, kPlaceLabels},
    {12, "Bands", 4, 12, 8, true, nullptr},
    {13, "Resonance", 0.05, 1, 0.5, false, nullptr},
    {14, "Feedback", 0, 0.85, 0, false, nullptr},  // notch only
    {15, "Mix", 0, 1, 1, false, nullptr},
    {16, "Volume", 0, 1, 0.7, false, nullptr},
    {17, "Stereo Width", 0, 1, 0.7, false, nullptr},  // time engines (2/3)
};
constexpr uint32_t kNumParams = sizeof(kParams) / sizeof(kParams[0]);

const ParamDef *findParam(clap_id id)
{
  for (const auto &d : kParams)
    if (d.id == id) return &d;
  return nullptr;
}

struct Plugin
{
  clap_plugin_t plugin{};
  const clap_host_t *host = nullptr;
  double sampleRate = 44100.0;
  hypersaw::FilterCore filter{44100.0};
  hypersaw::NotchCore notch{44100.0};
  hypersaw::TimeCore time{44100.0};

  double values[kNumParams] = {0};  // by index, kept for get_value/state
  double engine = 0;  // 0 bank · 1 notch · 2 tap delay (time echo) · 3 FDN room

  void initDefaults()
  {
    for (uint32_t i = 0; i < kNumParams; i++) values[i] = kParams[i].defV;
    for (uint32_t i = 0; i < kNumParams; i++) applyParam(kParams[i].id, kParams[i].defV);
  }

  int indexOf(clap_id id) const
  {
    for (uint32_t i = 0; i < kNumParams; i++)
      if (kParams[i].id == id) return (int)i;
    return -1;
  }

  // Map the unified surface onto both cores (defaults harmless for the other).
  void applyParam(clap_id id, double v)
  {
    const ParamDef *d = findParam(id);
    if (!d) return;
    v = std::max(d->minV, std::min(d->maxV, v));
    const double applied = d->stepped ? std::round(v) : v;
    const int idx = indexOf(id);
    if (idx >= 0) values[idx] = applied;
    // Shared-name keys drive all three cores; the time engine reuses the
    // frequency surface where it can and remaps where it differs
    // (Center→size, Resonance→regen, Feedback→damp). Engine 2/3 = time
    // echo/room via the mode param.
    auto three = [&](const char *k, double val) {
      filter.setParam(k, val);
      notch.setParam(k, val);
      time.setParam(k, val);
    };
    switch (id)
    {
      case 1:
        engine = applied;
        if (applied >= 2) time.setParam("mode", applied == 3 ? 1 : 0);
        break;
      case 2: three("K", applied); break;
      case 3: three("grav", applied); break;
      case 4: three("basin", applied); break;
      case 5: three("driftDepth", applied); break;
      case 6: three("driftRate", applied); break;
      case 7: three("inertia", applied); break;
      case 8:
        filter.setParam("center", applied);
        notch.setParam("center", applied);
        time.setParam("size", (applied - 7.0) / 4.3);  // log2-Hz knob → 0..1 size
        break;
      case 9: three("spread", applied); break;
      case 10: three("dist", applied); break;
      case 11:
        filter.setParam("place", applied);
        notch.setParam("place", applied);  // time is log-time placement only
        break;
      case 12: three("nb", applied); break;
      case 13:
        filter.setParam("qbase", applied);   // 0..1 → 2..40
        notch.setParam("stageQ", applied);    // 0.05..1 (k = 1/stageQ)
        time.setParam("regen", applied);      // resonance → feedback/decay
        break;
      case 14:
        notch.setParam("feedback", applied);
        time.setParam("damp", applied);       // tail damping
        break;
      case 15:
        filter.setParam("wet", applied);
        notch.setParam("mix", applied);
        time.setParam("mix", applied);
        break;
      case 16: three("vol", applied); break;
      case 17: time.setParam("stereo", applied); break;
      default: break;
    }
  }

  double readParam(clap_id id) const
  {
    const int idx = ((Plugin *)this)->indexOf(id);
    return idx >= 0 ? values[idx] : 0.0;
  }
};

Plugin *self(const clap_plugin_t *p) { return static_cast<Plugin *>(p->plugin_data); }

/* ---- lifecycle ---- */
bool plug_init(const clap_plugin_t *) { return true; }
void plug_destroy(const clap_plugin_t *p) { delete self(p); }
bool plug_activate(const clap_plugin_t *p, double sr, uint32_t, uint32_t)
{
  auto *pl = self(p);
  pl->sampleRate = sr;
  hypersaw::FilterCore::FParams fp = pl->filter.p;
  hypersaw::NotchCore::NParams np = pl->notch.p;
  hypersaw::TimeCore::TParams tp = pl->time.p;
  pl->filter = hypersaw::FilterCore(sr);
  pl->notch = hypersaw::NotchCore(sr);
  pl->time = hypersaw::TimeCore(sr);
  pl->filter.p = fp;
  pl->notch.p = np;
  pl->time.p = tp;
  pl->filter.setParam("seed", fp.seed);  // trigger rebuild at the new rate
  pl->notch.setParam("seed", np.seed);
  pl->time.setParam("seed", tp.seed);
  return true;
}
void plug_deactivate(const clap_plugin_t *) {}
bool plug_start_processing(const clap_plugin_t *) { return true; }
void plug_stop_processing(const clap_plugin_t *) {}
void plug_reset(const clap_plugin_t *p) { self(p)->initDefaults(); }
void plug_on_main_thread(const clap_plugin_t *) {}

void handleEvent(Plugin *pl, const clap_event_header_t *ev)
{
  if (ev->space_id != CLAP_CORE_EVENT_SPACE_ID) return;
  if (ev->type == CLAP_EVENT_PARAM_VALUE)
  {
    auto *pv = reinterpret_cast<const clap_event_param_value_t *>(ev);
    pl->applyParam(pv->param_id, pv->value);
  }
  else if (ev->type == CLAP_EVENT_NOTE_ON)
  {
    auto *n = reinterpret_cast<const clap_event_note_t *>(ev);
    if (n->velocity > 0.0)
    {
      const double f = 440.0 * std::pow(2.0, (n->key - 69) / 12.0);
      pl->filter.setNoteFreq(f);  // move the gravity center; no exciter voice
      pl->notch.setNoteFreq(f);
      pl->time.setNoteFreq(f);
    }
  }
}

clap_process_status plug_process(const clap_plugin_t *p, const clap_process_t *proc)
{
  auto *pl = self(p);
  const uint32_t n = proc->frames_count;
  // Block-start event application (params + gravity notes) — sample-accurate
  // scheduling is a later refinement; adequate for auditioning.
  const uint32_t nev = proc->in_events ? proc->in_events->size(proc->in_events) : 0;
  for (uint32_t i = 0; i < nev; i++) handleEvent(pl, proc->in_events->get(proc->in_events, i));

  if (proc->audio_inputs_count < 1 || proc->audio_outputs_count < 1) return CLAP_PROCESS_CONTINUE;
  const float *inL = proc->audio_inputs[0].data32[0];
  const float *inR = proc->audio_inputs[0].channel_count > 1 ? proc->audio_inputs[0].data32[1] : inL;
  float *outL = proc->audio_outputs[0].data32[0];
  float *outR = proc->audio_outputs[0].data32[1];

  const int eng = (int)std::round(pl->engine);
  if (eng >= 2)
    pl->time.processExternalStereo(inL, inR, outL, outR, (int)n);  // 2 tap delay · 3 FDN room
  else if (eng == 1)
    pl->notch.processExternal(inL, inR, outL, outR, (int)n);
  else
    pl->filter.processExternal(inL, inR, outL, outR, (int)n);
  return CLAP_PROCESS_CONTINUE;
}

/* ---- extensions ---- */
uint32_t aports_count(const clap_plugin_t *, bool) { return 1; }
bool aports_get(const clap_plugin_t *, uint32_t index, bool, clap_audio_port_info_t *info)
{
  if (index != 0) return false;
  info->id = 0;
  std::snprintf(info->name, sizeof(info->name), "%s", "Main");
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = 0;  // in-place capable (same id in/out)
  return true;
}
const clap_plugin_audio_ports_t s_audio_ports = {aports_count, aports_get};

uint32_t nports_count(const clap_plugin_t *, bool is_input) { return is_input ? 1 : 0; }
bool nports_get(const clap_plugin_t *, uint32_t index, bool is_input, clap_note_port_info_t *info)
{
  if (!is_input || index != 0) return false;
  info->id = 0;
  std::snprintf(info->name, sizeof(info->name), "%s", "Gravity Note In");
  info->supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI;
  info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
  return true;
}
const clap_plugin_note_ports_t s_note_ports = {nports_count, nports_get};

uint32_t params_count(const clap_plugin_t *) { return kNumParams; }
bool params_get_info(const clap_plugin_t *, uint32_t index, clap_param_info_t *info)
{
  if (index >= kNumParams) return false;
  const ParamDef &d = kParams[index];
  std::memset(info, 0, sizeof(*info));
  info->id = d.id;
  info->min_value = d.minV;
  info->max_value = d.maxV;
  info->default_value = d.defV;
  info->flags = CLAP_PARAM_IS_AUTOMATABLE | (d.stepped ? CLAP_PARAM_IS_STEPPED : 0);
  std::snprintf(info->name, sizeof(info->name), "%s", d.name);
  return true;
}
bool params_get_value(const clap_plugin_t *p, clap_id id, double *out)
{
  if (!findParam(id)) return false;
  *out = self(p)->readParam(id);
  return true;
}
bool params_value_to_text(const clap_plugin_t *, clap_id id, double value, char *out, uint32_t cap)
{
  const ParamDef *d = findParam(id);
  if (!d) return false;
  if (d->labels)
  {
    const int i = (int)std::round(value);
    std::snprintf(out, cap, "%s", d->labels[i < 0 ? 0 : i]);
  }
  else if (d->stepped)
    std::snprintf(out, cap, "%d", (int)std::round(value));
  else
    std::snprintf(out, cap, "%.3f", value);
  return true;
}
bool params_text_to_value(const clap_plugin_t *, clap_id, const char *text, double *out)
{
  *out = std::atof(text);
  return true;
}
void params_flush(const clap_plugin_t *p, const clap_input_events_t *in, const clap_output_events_t *)
{
  auto *pl = self(p);
  const uint32_t n = in ? in->size(in) : 0;
  for (uint32_t i = 0; i < n; i++)
  {
    const clap_event_header_t *ev = in->get(in, i);
    if (ev->type == CLAP_EVENT_PARAM_VALUE)
    {
      auto *pv = reinterpret_cast<const clap_event_param_value_t *>(ev);
      pl->applyParam(pv->param_id, pv->value);
    }
  }
}
const clap_plugin_params_t s_params = {params_count,          params_get_info, params_get_value,
                                       params_value_to_text,  params_text_to_value, params_flush};

/* ---- state (versioned key=value, mirrors the instrument's format) ---- */
bool state_save(const clap_plugin_t *p, const clap_ostream_t *stream)
{
  auto *pl = self(p);
  std::string blob = "swarmfx-state 1\n";
  char line[64];
  for (uint32_t i = 0; i < kNumParams; i++)
  {
    std::snprintf(line, sizeof(line), "%u=%.17g\n", (unsigned)kParams[i].id, pl->values[i]);
    blob += line;
  }
  uint64_t off = 0;
  while (off < blob.size())
  {
    const int64_t w = stream->write(stream, blob.data() + off, blob.size() - off);
    if (w <= 0) return false;
    off += (uint64_t)w;
  }
  return true;
}
bool state_load(const clap_plugin_t *p, const clap_istream_t *stream)
{
  auto *pl = self(p);
  std::string blob;
  char buf[256];
  int64_t r;
  while ((r = stream->read(stream, buf, sizeof(buf))) > 0) blob.append(buf, (size_t)r);
  if (r < 0) return false;
  size_t pos = blob.find('\n');
  if (pos == std::string::npos || blob.compare(0, pos, "swarmfx-state 1") != 0) return false;
  size_t i = pos + 1;
  while (i < blob.size())
  {
    size_t nl = blob.find('\n', i);
    if (nl == std::string::npos) nl = blob.size();
    const std::string ln = blob.substr(i, nl - i);
    const size_t eq = ln.find('=');
    if (eq != std::string::npos)
      pl->applyParam((clap_id)std::atoi(ln.substr(0, eq).c_str()),
                     std::atof(ln.substr(eq + 1).c_str()));
    i = nl + 1;
  }
  return true;
}
const clap_plugin_state_t s_state = {state_save, state_load};

const void *plug_get_extension(const clap_plugin_t *, const char *id)
{
  if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &s_audio_ports;
  if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS)) return &s_note_ports;
  if (!std::strcmp(id, CLAP_EXT_PARAMS)) return &s_params;
  if (!std::strcmp(id, CLAP_EXT_STATE)) return &s_state;
  return nullptr;
}

/* ---- factory ---- */
uint32_t factory_get_plugin_count(const clap_plugin_factory *) { return 1; }
const clap_plugin_descriptor_t *factory_get_plugin_descriptor(const clap_plugin_factory *,
                                                              uint32_t index)
{
  return index == 0 ? &s_desc : nullptr;
}
const clap_plugin_t *factory_create_plugin(const clap_plugin_factory *, const clap_host_t *host,
                                           const char *plugin_id)
{
  if (std::strcmp(plugin_id, s_desc.id) != 0) return nullptr;
  auto *pl = new Plugin();
  pl->host = host;
  pl->initDefaults();
  pl->plugin.desc = &s_desc;
  pl->plugin.plugin_data = pl;
  pl->plugin.init = plug_init;
  pl->plugin.destroy = plug_destroy;
  pl->plugin.activate = plug_activate;
  pl->plugin.deactivate = plug_deactivate;
  pl->plugin.start_processing = plug_start_processing;
  pl->plugin.stop_processing = plug_stop_processing;
  pl->plugin.reset = plug_reset;
  pl->plugin.process = plug_process;
  pl->plugin.get_extension = plug_get_extension;
  pl->plugin.on_main_thread = plug_on_main_thread;
  return &pl->plugin;
}
const clap_plugin_factory_t s_factory = {factory_get_plugin_count, factory_get_plugin_descriptor,
                                         factory_create_plugin};

}  // namespace

extern "C"
{
  bool swarmfx_entry_init(const char *) { return true; }
  void swarmfx_entry_deinit(void) {}
  const void *swarmfx_entry_get_factory(const char *factory_id)
  {
    if (!std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID)) return &s_factory;
    return nullptr;
  }
}
