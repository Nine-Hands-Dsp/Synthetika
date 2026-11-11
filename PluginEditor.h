#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "Layout.h"
#include "NineHandsMenu.h"
#include "NHKnob.h"

//==============================================================================
// Tambour de selection (intervalles) : cylindre vertical centre sur l'ecran qui l'ouvre, molette puis clic,
// Entree / Echap ; fond presque opaque qui masque tout ce qui est dessous, halo de la couleur, aide en dessous.
//==============================================================================
class DrumSelector : public juce::Component
{
public:
    DrumSelector (juce::StringArray items, int current, juce::Rectangle<float> anchor, juce::Colour accent, juce::Typeface::Ptr face,
                  std::function<void (int)> onChoose);
    void paint (juce::Graphics&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    std::function<void()> onClose;
    float unit = 1.0f;                 // pixels par unite du plugin
private:
    void choose();
    juce::StringArray items; int sel; float scroll; juce::Rectangle<float> anchor; juce::Colour accent; juce::Typeface::Ptr face;
    std::function<void (int)> onChoose;
    float dragY0 = 0, scroll0 = 0; bool dragged = false;
};

//==============================================================================
class SynthetikaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SynthetikaEditor (SynthetikaProcessor&);
    ~SynthetikaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr float baseW = layout::frameW, baseH = layout::frameH;
    static constexpr int defaultH = 495, maxH = 990;     // format double : hauteur de Ring Think = taille par defaut « 100 % », jusqu'a 200 %
    static constexpr int numSizes = 4;          // 100 % (defaut) / 125 % / 150 % / 200 %
    static int sizeHeight (int idx);
    void setScaledHeight (int h);
    void forceRenderScale (float sc) { forcedScale = sc; rebuildCaches (sc); }

private:
    enum HitKind { hNone, hPresetPrev, hPresetName, hPresetNext, hSave, hDelete, hEngine, hMode, hPrePost, hOn, hIntArrow, hIntDisp, hFs, hHandle };
    struct Hit { HitKind kind = hNone; int a = -1, b = 0; };
    Hit hitAt (juce::Point<float> u) const;
    int learnIndexForHit (const Hit&) const;
    int moduleAtSlot (int slot) const;                 // slot 3-5 -> module 3 (ring), 4 (crush), 5 (verb)
    int slotOfModule (int m) const;
    float columnDx (int m) const;                      // decalage horizontal d'une colonne deplacee (ordre + glisser)
    void layoutKnobs();

    void timerCallback() override;
    void useSoftwareRenderer();
    void askRepaint (juce::Rectangle<int> r);
    void askRepaintUnits (juce::Rectangle<float> r) { askRepaint (scaled (r)); }
    void rebuildCaches (float scale = 0.0f);
    float physicalScale() const;
    juce::Rectangle<int> scaled (juce::Rectangle<float> r) const;
    float unitScale() const { return (float) getHeight() / baseH; }

    void buildPrint();
    juce::Path textPath (const juce::String& s, float em, int font, float track, int anchor, float sy = 1.0f, bool ink = false, float rot = 0.0f) const;
    void drawPrint (juce::Graphics&, int mod, float dx);
    void drawModuleControls (juce::Graphics&, int m, float dx);
    void drawPresetBar (juce::Graphics&);
    void drawFootswitch (juce::Graphics&);

    void showContextMenu (int learnIdx);
    void showPresetMenu();
    void askSavePreset();
    void askDeletePreset();
    void openDrum (int which);
    void toggleParam (const char* id);
    void setChoice (const char* id, int idx);
    juce::String statusText (const juce::String& id) const;
    void showStatus (const juce::String& id);

    SynthetikaProcessor& proc;
    NineHandsMenu::MenuLnf menuLnf;
    juce::Typeface::Ptr boldFace, medFace;
    juce::Image background, bgScaled, columns[3], columnsScaled[3];
    juce::Image keyEngUp[5], keyEngDn[5], keyModeUp, keyModeDn, swOnOff, swOnOn, swPPOff, swPPOn, fsRefl;
    std::unique_ptr<juce::Drawable> logo;

    struct PrintItem { juce::Path path; juce::Colour col; int mod; };
    std::vector<PrintItem> print;
    struct FrameItem { juce::Rectangle<float> r; float rad, w; juce::Colour col; bool hasGap; juce::Rectangle<float> gap; };
    std::vector<FrameItem> frames;
    juce::Path engLabel[5], modeLabel[4], onLabel;
    struct EmblemBar { juce::Path path; juce::Colour col; };
    std::vector<EmblemBar> emblem;          // arc-en-ciel : allume quand la pedale est active, eteint en bypass
    juce::Rectangle<float> emblemArea;
    void drawEmblem (juce::Graphics&);

    std::vector<std::unique_ptr<nh::Knob>> knobs;
    std::vector<int> knobModule;                       // colonne qui porte le potard (-1 = fixe)
    std::vector<juce::String> knobParam;

    int cacheW = 0, cacheH = 0, resizeSettle = 0;
    float cacheScale = 0.0f, forcedScale = 0.0f;
    void* lastPeer = nullptr;
    double lastPaintMs = 0, repaintAskedMs = 0, lastScaleCheckMs = 0, lastRendererCheckMs = 0;

    // etat affiche
    float lastVals[40] {};
    int lastPresetCounter = -1;
    juce::String presetLine, statusLine;
    double statusUntil = 0;
    float statusAlpha = 0, drawnStatus = -1;
    float ledIn = 0, ledOut = 0, fsLevel = 0, drawnLedIn = -1, drawnLedOut = -1, drawnFs = -1;
    double clipUntil = 0;
    int hoverPreset = 0, glowPercent = 100;
    Hit pressedHit;
    // glisser une colonne (Ring Mod, Bitcrush, Gated Reverb)
    int dragModule = -1; float dragStartX = 0, dragOffset = 0;
    std::unique_ptr<DrumSelector> drum;
    juce::String learnLine; double learnUntil = 0;
    int lastMidi = 0, midiFlash = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthetikaEditor)
};
