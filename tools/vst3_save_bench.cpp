/*
 * vst3_save_bench — the SAME save, through the VST3 wrapper instead of CLAP.
 *
 * B204. Ableton loaded horde over VST3, not CLAP, when the save stalled
 * (Live's log: the last two entries before it ends are VST3 controller-state
 * notices four and a half minutes apart). save_bench times the CLAP factory
 * path; substituting that number for the VST3 one would be assuming away the
 * exact layer the evidence points at. So this loads the BUILT .vst3 bundle the
 * way a host does — CFBundle, bundleEntry, GetPluginFactory, createInstance —
 * and times Steinberg::Vst::IComponent::getState, which is what Live calls.
 *
 * ARGUMENT: the path to the .vst3 bundle. Optional second argument: a file
 * holding a host chunk to setState first, so the adversarial patch can be
 * measured through this path too (the VST3 chunk IS our CLAP chunk — the
 * wrapper hands the stream straight to clap_plugin_state, libs/clap-wrapper/
 * src/wrapasvst3.cpp:276 — which is why a blob from save_bench loads here).
 *
 * DIAGNOSTIC, NOT A GATE: prints numbers, asserts nothing, exits 0 unless the
 * bundle could not be loaded at all (which is a measurement that did not
 * happen, and is worth a nonzero exit so it cannot be read as a green run).
 * macOS only — the bundle-loading half is CoreFoundation.
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "pluginterfaces/base/ftypes.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"   // kVstAudioEffectClass
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsthostapplication.h"

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace Steinberg;

namespace
{

/* A memory IBStream. Hand-rolled rather than pulled from public.sdk so this
   tool links against pluginterfaces alone: public.sdk's main/ sources define a
   PLUGIN's entry points, and linking those into a HOST is how you get two
   GetPluginFactory definitions in one binary. */
class MemStream : public IBStream
{
 public:
  std::string data;
  int64 pos = 0;

  tresult PLUGIN_API queryInterface(const TUID iid, void **obj) override
  {
    if (FUnknownPrivate::iidEqual(iid, FUnknown::iid) ||
        FUnknownPrivate::iidEqual(iid, IBStream::iid))
    {
      *obj = this;
      return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }

  tresult PLUGIN_API read(void *buffer, int32 numBytes, int32 *numBytesRead) override
  {
    const int64 take = std::min<int64>(numBytes, (int64)data.size() - pos);
    if (take > 0) std::memcpy(buffer, data.data() + pos, (size_t)take);
    pos += take > 0 ? take : 0;
    if (numBytesRead) *numBytesRead = (int32)(take > 0 ? take : 0);
    return kResultOk;
  }
  tresult PLUGIN_API write(void *buffer, int32 numBytes, int32 *numBytesWritten) override
  {
    data.append((const char *)buffer, (size_t)numBytes);
    pos = (int64)data.size();
    if (numBytesWritten) *numBytesWritten = numBytes;
    return kResultOk;
  }
  tresult PLUGIN_API seek(int64 p, int32 mode, int64 *result) override
  {
    if (mode == kIBSeekSet) pos = p;
    else if (mode == kIBSeekCur) pos += p;
    else pos = (int64)data.size() + p;
    if (pos < 0) pos = 0;
    if (pos > (int64)data.size()) pos = (int64)data.size();
    if (result) *result = pos;
    return kResultOk;
  }
  tresult PLUGIN_API tell(int64 *p) override
  {
    if (p) *p = pos;
    return kResultOk;
  }
};

/* THE MINIMUM HOST CONTEXT. IComponent::initialize(nullptr) died inside the
   wrapper with SIGSEGV and no output, which is why stdout is unbuffered above:
   clap-wrapper's ClapAsVst3 asks its context for IHostApplication and does not
   check the answer, so "no context" is not a lighter host, it is a null
   dereference. An object with a name is the smallest thing that gets past it —
   and it is also what a real host supplies, so the measurement stays on the
   path Live uses rather than on a degraded one. */
class HostApp : public Vst::IHostApplication
{
 public:
  tresult PLUGIN_API queryInterface(const TUID iid, void **obj) override
  {
    if (FUnknownPrivate::iidEqual(iid, FUnknown::iid) ||
        FUnknownPrivate::iidEqual(iid, Vst::IHostApplication::iid))
    {
      *obj = this;
      return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }
  tresult PLUGIN_API getName(Vst::String128 name) override
  {
    const char *n = "vst3_save_bench";
    int i = 0;
    for (; n[i] && i < 127; i++) name[i] = (Vst::TChar)n[i];
    name[i] = 0;
    return kResultOk;
  }
  tresult PLUGIN_API createInstance(TUID, TUID, void **obj) override
  {
    *obj = nullptr;
    return kNotImplemented;
  }
};

struct Timing
{
  double minUs = 0, medUs = 0;
  int reps = 0;
};
template <typename F>
Timing timeIt(int reps, F &&fn)
{
  fn();
  std::vector<double> us;
  us.reserve((size_t)reps);
  for (int i = 0; i < reps; i++)
  {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
  }
  std::sort(us.begin(), us.end());
  return Timing{us.front(), us[us.size() / 2], reps};
}

}   // namespace

int main(int argc, char **argv)
{
#if !defined(__APPLE__)
  std::printf("vst3_save_bench: macOS only (the bundle loader is CoreFoundation).\n");
  return 1;
#else
  /* Unbuffered: a wrapper that dies inside bundleEntry takes a buffered
     stdout with it, and "no output at all" is the least useful failure report
     there is. */
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2)
  {
    std::printf("usage: vst3_save_bench <path/to/horde.vst3> [state-chunk-file]\n");
    return 1;
  }
  CFStringRef path = CFStringCreateWithCString(nullptr, argv[1], kCFStringEncodingUTF8);
  CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, path, kCFURLPOSIXPathStyle, true);
  CFBundleRef bundle = CFBundleCreate(nullptr, url);
  CFRelease(url);
  CFRelease(path);
  if (!bundle)
  {
    std::printf("vst3_save_bench: CFBundleCreate failed for %s\n", argv[1]);
    return 1;
  }
  if (!CFBundleLoadExecutable(bundle))
  {
    std::printf("vst3_save_bench: CFBundleLoadExecutable failed\n");
    return 1;
  }
  auto sym = [&](const char *n) {
    CFStringRef s = CFStringCreateWithCString(nullptr, n, kCFStringEncodingUTF8);
    void *f = CFBundleGetFunctionPointerForName(bundle, s);
    CFRelease(s);
    return f;
  };
  auto *bundleEntry = (bool (*)(CFBundleRef))sym("bundleEntry");
  auto *getFactory = (IPluginFactory * (*)())sym("GetPluginFactory");
  if (!bundleEntry || !getFactory)
  {
    std::printf("vst3_save_bench: bundle is missing bundleEntry/GetPluginFactory\n");
    return 1;
  }
  if (!bundleEntry(bundle))
  {
    std::printf("vst3_save_bench: bundleEntry() refused\n");
    return 1;
  }
  IPluginFactory *factory = getFactory();
  if (!factory)
  {
    std::printf("vst3_save_bench: GetPluginFactory() returned null\n");
    return 1;
  }

  PClassInfo ci{};
  bool found = false;
  for (int32 i = 0, n = factory->countClasses(); i < n && !found; i++)
    if (factory->getClassInfo(i, &ci) == kResultOk &&
        std::strcmp(ci.category, kVstAudioEffectClass) == 0)
      found = true;
  if (!found)
  {
    std::printf("vst3_save_bench: no Audio Module Class in the factory\n");
    return 1;
  }
  std::printf("vst3_save_bench: class \"%s\" (%s)\n", ci.name, ci.category);

  Vst::IComponent *comp = nullptr;
  if (factory->createInstance(ci.cid, Vst::IComponent::iid, (void **)&comp) != kResultOk || !comp)
  {
    std::printf("vst3_save_bench: createInstance(IComponent) failed\n");
    return 1;
  }
  static HostApp hostApp;
  if (comp->initialize(&hostApp) != kResultOk)
  {
    // Not fatal to the measurement, but never silent: a getState on a plugin
    // that refused to initialise is not the number this tool claims to print.
    std::printf("vst3_save_bench: NOTE — IComponent::initialize did not return kResultOk\n");
  }

  /* ACTIVATE BEFORE LOADING. save_bench's CLAP control showed that a chunk
     loaded into an init-but-never-activated instance loses the engine block's
     values, and only that arm does — activated, the round trip is
     byte-identical. A host never presents the inactive rig, so measuring on it
     would be measuring the bench. setupProcessing first, because setActive on
     a processor that was never configured is not a supported order. */
  Vst::IAudioProcessor *ap = nullptr;
  if (comp->queryInterface(Vst::IAudioProcessor::iid, (void **)&ap) == kResultOk && ap)
  {
    Vst::ProcessSetup setup{};
    setup.processMode = Vst::kRealtime;
    setup.symbolicSampleSize = Vst::kSample32;
    setup.maxSamplesPerBlock = 512;
    setup.sampleRate = 44100.0;
    const tresult sr = ap->setupProcessing(setup);
    const tresult ac = comp->setActive(true);
    std::printf("vst3_save_bench: setupProcessing -> %s, setActive(true) -> %s\n",
                sr == kResultOk ? "ok" : "NOT ok", ac == kResultOk ? "ok" : "NOT ok");
  }
  else
  {
    std::printf("vst3_save_bench: NOTE — no IAudioProcessor; measuring an INACTIVE instance\n");
  }

  if (argc >= 3)
  {
    std::ifstream f(argv[2], std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    MemStream in;
    in.data = ss.str();
    in.pos = 0;
    const tresult r = comp->setState(&in);
    std::printf("vst3_save_bench: setState(%zu bytes) -> %s\n", in.data.size(),
                r == kResultOk ? "kResultOk" : "NOT ok");
  }

  MemStream probe;
  const tresult r = comp->getState(&probe);
  std::printf("vst3_save_bench: getState -> %s, %zu bytes\n",
              r == kResultOk ? "kResultOk" : "NOT ok", probe.data.size());
  /* Optional third argument: write what getState produced. A setState/getState
     pair whose byte counts differ is either an expected difference (keys that
     exist only once a stream has drawn) or a state-loss bug, and the only way
     to tell them apart is to diff the two — so the bytes are dumpable rather
     than summarised into a count nobody can check. */
  if (argc >= 4)
    if (std::FILE *f = std::fopen(argv[3], "wb"))
    {
      std::fwrite(probe.data.data(), 1, probe.data.size(), f);
      std::fclose(f);
      std::printf("vst3_save_bench: getState bytes written to %s\n", argv[3]);
    }

  const Timing t = timeIt(400, [&] {
    MemStream s;
    comp->getState(&s);
  });
  std::printf("  %-52s min %9.2f us   median %9.2f us   (n=%d)\n",
              "IComponent::getState (VST3 wrapper -> clap state.save)", t.minUs, t.medUs, t.reps);

  comp->setActive(false);
  if (ap) ap->release();
  comp->terminate();
  comp->release();
  return 0;
#endif
}
