#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "NineHandsFolder.h"

using namespace juce;

//==============================================================================
// Presets d'usine (inspires des sons de Soklo et des extraits analyses)
//==============================================================================
static const SynthPreset factoryPresets[] =
{
    //  nom                    in   mix  out | synth mix  eng i1   i2  | dist gain mode pos | mod dpt mix | ring Hz   mix | crush %  bits | verb ms mix | order
    { "Future Funk", 0, 100, 0, true, 100, 0, 7, 12, true, 25, 0, 0, false, 30, 50, false, 440, 50, false, 0, 24, false, 220, 22, 0 },
    { "Soklo Bass",            0, 100,  0,  true, 100, 0,  7,  12,   false, 30, 0, 0,   false, 30, 50,  false, 440, 50,  false, 0, 24,   false, 300, 30, 0 },
    { "Fifths & Octave", 0, 100, 0, true, 100, 4, 7, 12, false, 30, 0, 0, true, 35, 60, false, 440, 50, false, 0, 24, true, 380, 20, 0 },
    { "Analog Lead", 0, 100, 0, true, 100, 2, 7, 12, true, 32, 2, 1, true, 18, 40, false, 440, 50, false, 0, 24, true, 320, 16, 0 },
    { "Vintage Keys", 0, 100, 0, true, 90, 1, 7, 12, false, 30, 0, 0, true, 25, 50, false, 440, 50, false, 0, 24, false, 200, 20, 0 },
    { "String Machine",        0, 100,  0,  true, 100, 3,  7,  12,   false, 30, 0, 0,   true,  45, 65,  false, 440, 50,  false, 0, 24,   true, 650, 30,  0 },
    { "Octave Stack", 0, 100, 0, true, 100, 4, -12, 12, true, 15, 0, 0, false, 20, 40, false, 440, 50, false, 0, 24, false, 300, 25, 0 },
    { "Broken Radio", 0, 100, 0, true, 100, 2, 7, 12, true, 48, 1, 0, false, 30, 50, false, 440, 50, true, 62, 6, false, 260, 20, 0 },
    { "Ring Bell", 0, 100, 0, true, 100, 1, 7, 12, false, 30, 0, 0, true, 20, 40, true, 870, 60, false, 0, 24, true, 450, 22, 2 },
    { "80s Gated Lead",        0, 100,  0,  true, 100, 2,  7,  12,   true,  20, 0, 1,   true,  28, 55,  false, 440, 50,  false, 0, 24,   true, 480, 55,  0 },
    { "Wavefold Bass",         0, 100,  0,  true, 100, 0,  7,  12,   true,  45, 3, 0,   false, 30, 50,  false, 440, 50,  false, 0, 24,   false, 300, 30, 0 },
    { "Guitar + Synth", 0, 100, 0, true, 50, 0, 7, 12, false, 30, 0, 0, true, 25, 45, false, 440, 50, false, 0, 24, false, 260, 18, 0 },
    { "Bit Choir",             0, 100,  0,  true, 100, 3,  7,  12,   false, 30, 0, 0,   true,  40, 60,  false, 440, 50,  true, 40, 10,   true, 700, 35,  4 },
    { "Dry Guitar FX", 0, 100, 0, false, 100, 0, 7, 12, true, 35, 2, 0, false, 30, 50, true, 120, 35, false, 0, 24, true, 300, 18, 0 },
};

int SynthetikaProcessor::getNumFactoryPresets() { return (int) std::size (factoryPresets); }
const SynthPreset& SynthetikaProcessor::getFactoryPreset (int i) { return factoryPresets[(size_t) jlimit (0, getNumFactoryPresets() - 1, i)]; }

//==============================================================================
// MIDI Learn
//==============================================================================
const SynthetikaProcessor::LearnDef& SynthetikaProcessor::learnDef (int i)
{
    static const LearnDef defs[numLearn] =
    {
        { "input",     "Input",          learnContinuous },
        { "mix",       "Mix",            learnContinuous },
        { "output",    "Output",         learnContinuous },
        { "synthMix",  "Synth Mix",      learnContinuous },
        { "distGain",  "Drive Gain",     learnContinuous },
        { "modDepth",  "Mod Depth",      learnContinuous },
        { "modMix",    "Mod Mix",        learnContinuous },
        { "ringFreq",  "Ring Freq",      learnContinuous },
        { "ringMix",   "Ring Mix",       learnContinuous },
        { "crush",     "Crush",          learnContinuous },
        { "bits",      "Resolution",     learnContinuous },
        { "verbTime",  "Reverb Time",    learnContinuous },
        { "verbMix",   "Reverb Mix",     learnContinuous },
        { "synthOn",   "Synth On",       learnToggle },
        { "distOn",    "Distortion On",  learnToggle },
        { "modOn",     "Modulation On",  learnToggle },
        { "ringOn",    "Ring Mod On",    learnToggle },
        { "crushOn",   "Bitcrush On",    learnToggle },
        { "verbOn",    "Reverb On",      learnToggle },
        { "engine",    "Engine",         learnSelector },
        { "distMode",  "Drive Mode",     learnSelector },
        { "distPos",   "Pre / Post",     learnSelector },
        { "int1",      "Interval 1",     learnSelector },
        { "int2",      "Interval 2",     learnSelector },
        { "order",     "Effect Order",   learnSelector },
        { "footswitch","Footswitch",     learnFootswitch },
    };
    return defs[jlimit (0, numLearn - 1, i)];
}

int SynthetikaProcessor::learnIndexOf (const String& id)
{
    for (int i = 0; i < numLearn; ++i) if (id == learnDef (i).id) return i;
    return -1;
}

//==============================================================================
static String pct (float v, int) { return String (roundToInt (v)) + " %"; }
static String dbTxt (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }

AudioProcessorValueTreeState::ParameterLayout SynthetikaProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const auto& d = factoryPresets[0];
    auto fl = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> txt)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (txt)));
    };
    auto bl = [&] (const char* id, const char* name, bool def) { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto ch = [&] (const char* id, const char* name, StringArray items, int def) { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    fl ("input",  "Input",  { -24.0f, 24.0f, 0.01f }, d.input,  dbTxt);
    fl ("mix",    "Mix",    { 0.0f, 100.0f, 0.01f },  d.mix,    pct);
    fl ("output", "Output", { -24.0f, 24.0f, 0.01f }, d.output, dbTxt);
    bl ("synthOn", "Synth On", d.synthOn);
    fl ("synthMix", "Synth Mix", { 0.0f, 100.0f, 0.01f }, d.synthMix, pct);
    ch ("engine", "Engine", { "Deep", "Vintage", "Solo", "Strings", "Harmony" }, d.engine);
    StringArray semis; for (int s = -24; s <= 24; ++s) semis.add ((s > 0 ? "+" : "") + String (s));
    ch ("int1", "Interval 1", semis, d.int1 + 24);
    StringArray semis2 { "Off" }; semis2.addArray (semis);
    ch ("int2", "Interval 2", semis2, d.int2 + 25);
    bl ("distOn", "Distortion On", d.distOn);
    fl ("distGain", "Distortion Gain", { 0.0f, 100.0f, 0.01f }, d.distGain, pct);
    ch ("distMode", "Distortion Mode", { "Soft", "Hard", "Rich", "Wavefold" }, d.distMode);
    ch ("distPos", "Distortion Pre / Post", { "Pre", "Post", "Pre + Post" }, d.distPos);
    bl ("modOn", "Modulation On", d.modOn);
    fl ("modDepth", "Modulation Depth", { 0.0f, 100.0f, 0.01f }, d.modDepth, pct);
    fl ("modMix", "Modulation Mix", { 0.0f, 100.0f, 0.01f }, d.modMix, pct);
    bl ("ringOn", "Ring Mod On", d.ringOn);
    fl ("ringFreq", "Ring Mod Freq", NormalisableRange<float> (20.0f, 5000.0f,
            [] (float a, float b, float v) { return a * std::pow (b / a, v); },
            [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); }), d.ringFreq,
        [] (float v, int) { return v < 1000 ? String (roundToInt (v)) + " Hz" : String (v / 1000.0f, 2) + " kHz"; });
    fl ("ringMix", "Ring Mod Mix", { 0.0f, 100.0f, 0.01f }, d.ringMix, pct);
    bl ("crushOn", "Bitcrush On", d.crushOn);
    fl ("crush", "Bitcrush Crush", { 0.0f, 100.0f, 0.01f }, d.crush,
        [] (float v, int) { const float hz = 44100.0f * std::pow (0.01f, v / 100.0f); return hz < 1000 ? String (roundToInt (hz)) + " Hz" : String (hz / 1000.0f, 1) + " kHz"; });
    fl ("bits", "Bitcrush Resolution", { 1.0f, 24.0f, 0.01f }, d.bits, [] (float v, int) { return String (v, 1) + " bits"; });
    bl ("verbOn", "Gated Reverb On", d.verbOn);
    fl ("verbTime", "Gated Reverb Time", { 0.0f, 1000.0f, 0.1f }, d.verbTime, [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    fl ("verbMix", "Gated Reverb Mix", { 0.0f, 100.0f, 0.01f }, d.verbMix, pct);
    ch ("order", "Effect Order", { "Bitcrush > Ring Mod > Reverb", "Bitcrush > Reverb > Ring Mod", "Ring Mod > Bitcrush > Reverb",
                                   "Ring Mod > Reverb > Bitcrush", "Reverb > Bitcrush > Ring Mod", "Reverb > Ring Mod > Bitcrush" }, d.order);
    bl ("footswitch", "Footswitch", true);
    ch ("midiFsMode", "MIDI Footswitch Mode", { "Latching (127 / 0)", "Momentary (toggle)" }, 0);
    return { p.begin(), p.end() };
}

static const char* const rawIds[] = { "input", "mix", "output", "synthOn", "synthMix", "engine", "int1", "int2", "distOn", "distGain", "distMode", "distPos",
                                      "modOn", "modDepth", "modMix", "ringOn", "ringFreq", "ringMix", "crushOn", "crush", "bits", "verbOn", "verbTime", "verbMix",
                                      "order", "footswitch", "midiFsMode" };
enum { rIn, rMix, rOut, rSynOn, rSynMix, rEng, rInt1, rInt2, rDistOn, rDistGain, rDistMode, rDistPos, rModOn, rModDepth, rModMix, rRingOn, rRingFreq, rRingMix,
       rCrushOn, rCrush, rBits, rVerbOn, rVerbTime, rVerbMix, rOrder, rFs, rMidiFs, numRaw };

SynthetikaProcessor::SynthetikaProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SYNTHETIKA", createLayout())
{
    for (int i = 0; i < numRaw; ++i) raw[i] = apvts.getRawParameterValue (rawIds[i]);
    for (int i = 0; i < numLearn; ++i) { pendingValue[i] = 0.0f; pendingSet[i] = false; learnParam[i] = apvts.getParameter (learnDef (i).id); }
    resetMidiMap();
    startTimerHz (60);
}

SynthetikaProcessor::~SynthetikaProcessor() { stopTimer(); }

bool SynthetikaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
}

void SynthetikaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    for (auto& r : ramps) { r.active = false; r.left = 0; }
}

void SynthetikaProcessor::setParamValue (const String& id, float v)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        prm->endChangeGesture();
    }
}

// Anti-gel : jamais d'appel a l'hote depuis le thread audio : file vidée par le timer (thread de messages)
void SynthetikaProcessor::queueParam (int i, float norm01)
{
    pendingValue[i].store (norm01, std::memory_order_relaxed);
    pendingSet[i].store (true, std::memory_order_release);
}

void SynthetikaProcessor::timerCallback()
{
    bool any = false;
    for (int i = 0; i < numLearn; ++i)
        if (pendingSet[i].exchange (false, std::memory_order_acquire))
            if (auto* prm = learnParam[i]) { prm->setValueNotifyingHost (pendingValue[i].load (std::memory_order_relaxed)); any = true; }
    if (any) { modified = true; ++presetCounter; }
    const int pp = pendingProgram.exchange (-1);
    if (pp >= 0) applyFactoryPreset (pp);
}

//==============================================================================
void SynthetikaProcessor::handleMidi (MidiBuffer& midi)
{
    const bool latching = raw[rMidiFs]->load() < 0.5f;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int64 when = sampleClock + meta.samplePosition;
        if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            const int lt = learnTarget.load();
            if (lt >= 0 && lt < numLearn && cc < 120)
            {
                if (learnDef (lt).type == learnFootswitch && v < 64) continue;
                for (int j = 0; j < numLearn; ++j) if (j != lt && ccMap[j].load() == cc) ccMap[j].store (-1);
                ccMap[lt].store (cc); ramps[lt].msb = -1; prevCC[lt] = v;
                lastLearned.store (lt); learnTarget.store (-1); ++midiCounter;
                continue;
            }
            int ctl = -1;
            for (int j = 0; j < numLearn; ++j) if (ccMap[j].load (std::memory_order_relaxed) == cc) { ctl = j; break; }
            int lsbOf = -1;
            if (ctl < 0)
                for (int k = 0; k < numLearn; ++k)
                {
                    const int msbCC = ccMap[k].load (std::memory_order_relaxed);
                    if (learnDef (k).type == learnContinuous && msbCC >= 0 && msbCC < 32 && cc == msbCC + 32) { lsbOf = k; break; }
                }
            auto continuous = [&] (int k, float raw01)
            {
                auto& r = ramps[k];
                if (raw01 < r.lo) r.lo = raw01;
                if (raw01 > r.hi) r.hi = raw01;
                float v01 = (r.hi - r.lo > 0.4f) ? (raw01 - r.lo) / (r.hi - r.lo) : raw01;     // course apprise
                v01 = jlimit (0.0f, 1.0f, (v01 - 0.02f) / 0.96f);                               // zones mortes aux butees
                if (! r.active) r.value = learnParam[k]->getValue();
                r.left = (int) jlimit (0.002 * sr, 0.040 * sr, (double) (when - r.lastTime));
                r.lastTime = when; r.target = v01; r.step = (r.target - r.value) / (float) r.left; r.active = true;
                queueParam (k, v01);
            };
            if (ctl >= 0)
            {
                const auto& d = learnDef (ctl);
                if (d.type == learnContinuous) { ramps[ctl].msb = v; continuous (ctl, v / 127.0f); }
                else if (d.type == learnSelector)
                {
                    if (auto* c = dynamic_cast<AudioParameterChoice*> (learnParam[ctl]))
                    {
                        const int nItems = c->choices.size();
                        const int pos = jmin (nItems - 1, v * nItems / 128);
                        queueParam (ctl, c->convertTo0to1 ((float) pos));
                    }
                }
                else   // bascule / footswitch : accrochage 127 / 0 ou momentane
                {
                    int& prev = prevCC[ctl];
                    const float cur = pendingSet[ctl].load() ? pendingValue[ctl].load() : learnParam[ctl]->getValue();
                    if (latching)                   queueParam (ctl, v >= 64 ? 1.0f : 0.0f);
                    else if (v >= 64 && prev < 64)  queueParam (ctl, cur > 0.5f ? 0.0f : 1.0f);
                    prev = v;
                }
                ++midiCounter;
            }
            else if (lsbOf >= 0 && ramps[lsbOf].msb >= 0)
            {
                continuous (lsbOf, (float) (ramps[lsbOf].msb * 128 + v) / 16383.0f);       // CC 14 bits
                ++midiCounter;
            }
        }
        else if (m.isProgramChange())
        {
            const int pc = m.getProgramChangeNumber();
            if (pc < getNumFactoryPresets()) pendingProgram = pc;
            ++midiCounter;
        }
    }
    midi.clear();
}

void SynthetikaProcessor::readParams (synthetika::Params& p)
{
    auto rv = [this] (int i) { return raw[i]->load (std::memory_order_relaxed); };
    // valeur normalisee d'une commande continue : rampe MIDI en cours, sinon le parametre
    auto cont = [this, &rv] (int learnIdx, int rawIdx) -> float
    {
        auto& r = ramps[learnIdx];
        if (r.active) return learnParam[learnIdx]->convertFrom0to1 (r.value);
        return rv (rawIdx);
    };
    p.inGain   = synthetika::dbToGain (cont (0, rIn));
    p.mix      = cont (1, rMix) * 0.01f;
    p.outGain  = synthetika::dbToGain (cont (2, rOut));
    p.synthOn  = rv (rSynOn) > 0.5f;
    p.synthMix = cont (3, rSynMix) * 0.01f;
    p.engine   = jlimit (0, 4, roundToInt (rv (rEng)));
    p.int1     = roundToInt (rv (rInt1)) - 24;
    const int i2 = roundToInt (rv (rInt2));
    p.int2On   = i2 > 0; p.int2 = i2 - 25;
    p.distOn   = rv (rDistOn) > 0.5f;
    p.distGain = cont (4, rDistGain) * 0.01f;
    p.distMode = jlimit (0, 3, roundToInt (rv (rDistMode)));
    const int pos = jlimit (0, 2, roundToInt (rv (rDistPos)));
    p.distPre  = pos != 1; p.distPost = pos != 0;
    p.modOn    = rv (rModOn) > 0.5f;
    p.modDepth = cont (5, rModDepth) * 0.01f;
    p.modMix   = cont (6, rModMix) * 0.01f;
    p.ringOn   = rv (rRingOn) > 0.5f;
    p.ringHz   = cont (7, rRingFreq);
    p.ringMix  = cont (8, rRingMix) * 0.01f;
    p.crushOn  = rv (rCrushOn) > 0.5f;
    p.crushHz  = 44100.0f * std::pow (0.01f, cont (9, rCrush) * 0.01f);
    p.crushBits = cont (10, rBits);
    p.verbOn   = rv (rVerbOn) > 0.5f;
    p.verbMs   = cont (11, rVerbTime);
    p.verbMix  = cont (12, rVerbMix) * 0.01f;
    p.order    = jlimit (0, 5, roundToInt (rv (rOrder)));
    p.active   = rv (rFs) > 0.5f;
}

void SynthetikaProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    handleMidi (midi);
    // entree mono, sortie stereo : la guitare va dans les deux canaux (pas de perte de 6 dB, bypass identique)
    const int nIn = getTotalNumInputChannels();
    if (nIn == 1 && buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int c = jmax (nIn, nIn == 1 ? 2 : nIn); c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0) return;

    // rampes MIDI : avancent au rythme des blocs
    for (auto& r : ramps)
        if (r.active)
        {
            const int adv = jmin (r.left, n);
            r.value += r.step * (float) adv; r.left -= adv;
            if (r.left <= 0) { r.value = r.target; r.active = false; }
        }

    synthetika::Params p;
    readParams (p);
    const int nCh = jmin (2, buffer.getNumChannels());
    float* io[2] = { buffer.getWritePointer (0), nCh > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (io, nCh, n, p);
    inLevel.store (engine.inPeak, std::memory_order_relaxed);
    outLevel.store (engine.outPeak, std::memory_order_relaxed);
    trackedHz.store (engine.trackedHz(), std::memory_order_relaxed);
    sampleClock += n;
}

//==============================================================================
// Presets
//==============================================================================
void SynthetikaProcessor::applyPreset (const SynthPreset& p)
{
    setParamValue ("input", p.input); setParamValue ("mix", p.mix); setParamValue ("output", p.output);
    setParamValue ("synthOn", p.synthOn ? 1.0f : 0.0f); setParamValue ("synthMix", p.synthMix); setParamValue ("engine", (float) p.engine);
    setParamValue ("int1", (float) (p.int1 + 24)); setParamValue ("int2", p.int2 < -24 ? 0.0f : (float) (p.int2 + 25));
    setParamValue ("distOn", p.distOn ? 1.0f : 0.0f); setParamValue ("distGain", p.distGain); setParamValue ("distMode", (float) p.distMode); setParamValue ("distPos", (float) p.distPos);
    setParamValue ("modOn", p.modOn ? 1.0f : 0.0f); setParamValue ("modDepth", p.modDepth); setParamValue ("modMix", p.modMix);
    setParamValue ("ringOn", p.ringOn ? 1.0f : 0.0f); setParamValue ("ringFreq", p.ringFreq); setParamValue ("ringMix", p.ringMix);
    setParamValue ("crushOn", p.crushOn ? 1.0f : 0.0f); setParamValue ("crush", p.crush); setParamValue ("bits", p.bits);
    setParamValue ("verbOn", p.verbOn ? 1.0f : 0.0f); setParamValue ("verbTime", p.verbTime); setParamValue ("verbMix", p.verbMix);
    setParamValue ("order", (float) p.order);
}

void SynthetikaProcessor::applyFactoryPreset (int i)
{
    i = jlimit (0, getNumFactoryPresets() - 1, i);
    applyPreset (factoryPresets[(size_t) i]);
    setName (factoryPresets[(size_t) i].name, i);
}

void SynthetikaProcessor::setCurrentProgram (int i)
{
    if (MessageManager::getInstance()->isThisTheMessageThread()) applyFactoryPreset (i);
    else pendingProgram = i;
}

File SynthetikaProcessor::userPresetFolder()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
             .getChildFile (nhd::folderName()).getChildFile ("Synthetika").getChildFile ("User Presets");
}

StringArray SynthetikaProcessor::userPresetNames() const
{
    StringArray names;
    for (const auto& f : userPresetFolder().findChildFiles (File::findFiles, false, "*.preset"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

static const char* const presetParamIds[] = { "input", "mix", "output", "synthOn", "synthMix", "engine", "int1", "int2", "distOn", "distGain", "distMode", "distPos",
                                              "modOn", "modDepth", "modMix", "ringOn", "ringFreq", "ringMix", "crushOn", "crush", "bits", "verbOn", "verbTime", "verbMix", "order" };

bool SynthetikaProcessor::saveUserPreset (const String& nameIn)
{
    const auto name = File::createLegalFileName (nameIn.trim());
    if (name.isEmpty() || name.equalsIgnoreCase ("Default")) return false;
    auto dir = userPresetFolder(); dir.createDirectory();
    XmlElement x ("SynthetikaPreset");
    x.setAttribute ("name", name);
    for (auto* id : presetParamIds) x.setAttribute (id, (double) plain (id));
    const bool ok = x.writeTo (dir.getChildFile (name + ".preset"));
    if (ok) setName (name, -1);
    return ok;
}

bool SynthetikaProcessor::loadUserPreset (const String& name)
{
    auto x = XmlDocument::parse (userPresetFolder().getChildFile (name + ".preset"));
    if (x == nullptr) return false;
    for (auto* id : presetParamIds) if (x->hasAttribute (id)) setParamValue (id, (float) x->getDoubleAttribute (id));
    setName (name, -1);
    return true;
}

bool SynthetikaProcessor::deleteUserPreset (const String& name)
{
    const bool ok = userPresetFolder().getChildFile (name + ".preset").deleteFile();
    if (ok && presetName() == name && ! presetIsFactory()) { modified = true; ++presetCounter; }
    return ok;
}

void SynthetikaProcessor::stepPreset (int delta)
{
    // liste : presets d'usine puis presets utilisateur
    const auto users = userPresetNames();
    const int nf = getNumFactoryPresets(), total = nf + users.size();
    int cur = factoryIndex.load();
    if (cur < 0) { const int u = users.indexOf (presetName()); cur = u >= 0 ? nf + u : (delta > 0 ? -1 : 0); }
    const int next = ((cur + delta) % total + total) % total;
    if (next < nf) applyFactoryPreset (next); else loadUserPreset (users[next - nf]);
}

//==============================================================================
void SynthetikaProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetName(), nullptr);
    state.setProperty ("presetFactory", factoryIndex.load(), nullptr);
    state.setProperty ("presetModified", modified.load(), nullptr);
    state.setProperty ("uiHeight", uiHeight.load(), nullptr);
    for (int i = 0; i < numLearn; ++i) state.setProperty (String ("cc_") + learnDef (i).id, ccMap[i].load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void SynthetikaProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = ValueTree::fromXml (*xml);
            for (int i = 0; i < numLearn; ++i) ccMap[i].store (jlimit (-1, 127, (int) state.getProperty (String ("cc_") + learnDef (i).id, -1)));
            uiHeight = (int) state.getProperty ("uiHeight", 0);
            apvts.replaceState (state);
            setName (state.getProperty ("presetName", "Future Funk").toString(), (int) state.getProperty ("presetFactory", 0));
            modified = (bool) state.getProperty ("presetModified", false);
        }
}

AudioProcessorEditor* SynthetikaProcessor::createEditor() { return new SynthetikaEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SynthetikaProcessor(); }
