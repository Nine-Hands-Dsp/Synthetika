#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include "SynthDSP.h"

//==============================================================================
// SYNTHETIKA (NINE HANDS | DSP) - synthe guitare dans l'esprit du Soklo GS-5 (Audio Hertz).
// Voir README.md et SynthDSP.h.
//==============================================================================

struct SynthPreset
{
    const char* name;
    // input, mix, output (dB, %, dB) ; synthe
    float input, mix, output;
    bool synthOn; float synthMix; int engine, int1, int2;      // int2 : -99 = coupe
    bool distOn; float distGain; int distMode, distPos;         // distPos : 0 pre, 1 post, 2 les deux
    bool modOn; float modDepth, modMix;
    bool ringOn; float ringFreq, ringMix;
    bool crushOn; float crush, bits;
    bool verbOn; float verbTime, verbMix;
    int order;
};

class SynthetikaProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SynthetikaProcessor();
    ~SynthetikaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Synthetika"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.2; }

    // ---------------- presets ----------------
    static int getNumFactoryPresets();
    static const SynthPreset& getFactoryPreset (int i);
    int getNumPrograms() override { return getNumFactoryPresets(); }
    int getCurrentProgram() override { return juce::jmax (0, factoryIndex.load()); }
    void setCurrentProgram (int i) override;
    const juce::String getProgramName (int i) override { return getFactoryPreset (i).name; }
    void changeProgramName (int, const juce::String&) override {}

    void applyFactoryPreset (int i);
    static juce::File userPresetFolder();
    juce::StringArray userPresetNames() const;                 // noms tries
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);
    void stepPreset (int delta);                                // fleches de la barre : usine puis utilisateur
    juce::String presetName() const { const juce::ScopedLock sl (nameLock); return currentName; }
    bool presetIsFactory() const { return factoryIndex.load() >= 0; }
    bool presetIsModified() const { return modified.load(); }
    void markModified() { modified = true; ++presetCounter; }
    std::atomic<int> presetCounter { 0 };

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParamValue (const juce::String& id, float plain);
    float plain (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    // ---------------- MIDI Learn : toutes les commandes de la facade ----------------
    enum LearnType { learnContinuous, learnToggle, learnSelector, learnFootswitch };
    struct LearnDef { const char* id; const char* name; int type; };
    static constexpr int numLearn = 26;
    static const LearnDef& learnDef (int i);
    static int learnIndexOf (const juce::String& id);
    std::atomic<int> ccMap[numLearn];
    std::atomic<int> learnTarget { -1 }, lastLearned { -1 };
    void resetMidiMap() { for (auto& c : ccMap) c.store (-1); }
    std::atomic<int> midiCounter { 0 };

    // ---------------- affichage ----------------
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, trackedHz { 0.0f };
    std::atomic<int> uiHeight { 0 };

private:
    void timerCallback() override;                 // anti-gel : vide la file des valeurs demandees par le MIDI
    void handleMidi (juce::MidiBuffer&);
    void queueParam (int learnIdx, float norm01);
    void applyPreset (const SynthPreset& p);
    void readParams (synthetika::Params& p);

    synthetika::Engine engine;
    double sr = 48000.0;
    juce::RangedAudioParameter* learnParam[numLearn] {};
    std::atomic<float> pendingValue[numLearn];
    std::atomic<bool>  pendingSet[numLearn];
    int prevCC[numLearn] {};

    // pedale d'expression : rampe lineaire sur l'intervalle mesure entre deux CC, course apprise, zones mortes
    struct Ramp { bool active = false; float value = 0, target = 0, step = 0; int left = 0; juce::int64 lastTime = 0; int msb = -1; float lo = 1, hi = 0; };
    Ramp ramps[numLearn];
    juce::int64 sampleClock = 0;
    std::atomic<int> pendingProgram { -1 };

    std::atomic<int> factoryIndex { 0 };
    std::atomic<bool> modified { false };
    juce::String currentName { "Future Funk" };
    juce::CriticalSection nameLock;
    void setName (const juce::String& n, int factory) { { const juce::ScopedLock sl (nameLock); currentName = n; } factoryIndex = factory; modified = false; ++presetCounter; }

    std::atomic<float>* raw[32] {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthetikaProcessor)
};
