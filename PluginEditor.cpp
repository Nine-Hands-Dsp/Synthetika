#include "PluginEditor.h"
#include "BinaryData.h"
#include "WigaWindowSize.h"
#include "NineHandsGlow.h"

using namespace juce;

namespace
{
    const Colour ink (0xff1c1a20), silk (0xffece6d8), amber (0xffffb040), pink (0xffff54a0);
    const Colour onText (0xffc8c4bc);
    Image png (const void* d, int n) { return ImageCache::getFromMemory (d, n); }

    // potards de la facade : identifiant dans Layout.h -> parametre, colonne (pour le reordonnancement)
    struct KnobMap { const char* layoutId; const char* param; int module; };
    const KnobMap knobMap[] = {
        { "synthMix", "synthMix", -1 }, { "gain", "distGain", -1 }, { "modDepth", "modDepth", -1 }, { "modMix", "modMix", -1 },
        { "ringFreq", "ringFreq", 3 }, { "ringMix", "ringMix", 3 }, { "crush", "crush", 4 }, { "bits", "bits", 4 },
        { "verbTime", "verbTime", 5 }, { "verbMix", "verbMix", 5 }, { "input", "input", -1 }, { "mix", "mix", -1 }, { "output", "output", -1 } };

    const char* const onIds[6] = { "synthOn", "distOn", "modOn", "ringOn", "crushOn", "verbOn" };
    // ordre des effets : 0 bitcrush, 1 ring mod, 2 reverb -> colonne
    const int effectModule[3] = { 4, 3, 5 };

    // identifiants suivis pour la ligne d'etat et les redessins
    const char* const watchIds[] = { "input", "mix", "output", "synthOn", "synthMix", "engine", "int1", "int2", "distOn", "distGain", "distMode", "distPos",
                                     "modOn", "modDepth", "modMix", "ringOn", "ringFreq", "ringMix", "crushOn", "crush", "bits", "verbOn", "verbTime", "verbMix",
                                     "order", "footswitch" };
    constexpr int numWatch = (int) (sizeof (watchIds) / sizeof (watchIds[0]));

    Rectangle<float> centred (Point<float> c, float w, float h) { return { c.x - w * 0.5f, c.y - h * 0.5f, w, h }; }
}

//==============================================================================
// TAMBOUR
//==============================================================================
DrumSelector::DrumSelector (StringArray it, int cur, Rectangle<float> a, Colour acc, Typeface::Ptr f, std::function<void (int)> cb)
    : items (std::move (it)), sel (cur), scroll ((float) cur), anchor (a), accent (acc), face (f), onChoose (std::move (cb))
{
    setWantsKeyboardFocus (true);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void DrumSelector::paint (Graphics& g)
{
    g.setColour (Colours::black.withAlpha (0.985f));
    const float rowH = 22.0f * unit, visible = 7.0f;
    const float w = jmax (anchor.getWidth() + 30.0f * unit, 120.0f * unit), h = rowH * visible;
    auto box = Rectangle<float> (w, h).withCentre (anchor.getCentre());
    if (box.getY() < 2) box.setY (2);
    if (box.getBottom() > getHeight() - 30.0f * unit) box.setY (getHeight() - 30.0f * unit - h);
    g.fillRoundedRectangle (box.expanded (6.0f * unit), 8.0f * unit);
    ColourGradient halo (accent.withAlpha (0.10f), box.getCentreX(), box.getCentreY(), accent.withAlpha (0.0f), box.getCentreX() + w * 0.6f, box.getCentreY(), true);
    g.setGradientFill (halo); g.fillRect (box);
    const auto band = Rectangle<float> (w, rowH).withCentre (box.getCentre());
    g.setColour (accent.withAlpha (0.18f)); g.fillRoundedRectangle (band.reduced (4.0f * unit, 1.0f), 4.0f * unit);
    g.setColour (accent.withAlpha (0.7f)); g.drawRoundedRectangle (band.reduced (4.0f * unit, 1.0f), 4.0f * unit, 1.0f);
    g.saveState(); g.reduceClipRegion (box.toNearestInt());
    for (int i = 0; i < items.size(); ++i)
    {
        const float off = (float) i - scroll;
        if (std::abs (off) > visible * 0.5f + 1) continue;
        const float y = box.getCentreY() + off * rowH;
        const float k = 1.0f - jmin (1.0f, std::abs (off) / (visible * 0.5f));
        g.setFont (Font (FontOptions (face).withHeight (rowH * (0.62f + 0.18f * k))));
        g.setColour ((std::abs (off) < 0.5f ? accent : Colours::white).withAlpha (0.25f + 0.75f * k));
        g.drawText (items[i], Rectangle<float> (w, rowH).withCentre ({ box.getCentreX(), y }), Justification::centred);
    }
    g.restoreState();
    g.setColour (Colours::white.withAlpha (0.55f));
    g.setFont (Font (FontOptions (face).withHeight (11.0f * unit)));
    g.drawText ("SCROLL  \xc2\xb7  CLICK TO CHOOSE", Rectangle<float> (w + 60 * unit, 16 * unit).withCentre ({ box.getCentreX(), box.getBottom() + 16.0f * unit }), Justification::centred);
}

void DrumSelector::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wd)
{
    scroll = jlimit (0.0f, (float) items.size() - 1, scroll - wd.deltaY * 4.0f);
    sel = roundToInt (scroll); scroll = (float) sel;
    repaint();
}
void DrumSelector::mouseDown (const MouseEvent& e) { dragY0 = e.position.y; scroll0 = scroll; dragged = false; }
void DrumSelector::mouseDrag (const MouseEvent& e)
{
    const float d = (e.position.y - dragY0) / (22.0f * unit);
    if (std::abs (d) > 0.2f) dragged = true;
    scroll = jlimit (0.0f, (float) items.size() - 1, scroll0 - d);
    sel = roundToInt (scroll); repaint();
}
void DrumSelector::mouseUp (const MouseEvent& e)
{
    if (! dragged)
    {
        const float rowH = 22.0f * unit;
        const auto box = Rectangle<float> (jmax (anchor.getWidth() + 30.0f * unit, 120.0f * unit), rowH * 7).withCentre (anchor.getCentre());
        if (! box.expanded (6.0f * unit).contains (e.position)) { if (onClose) onClose(); return; }
        const int clicked = sel + roundToInt ((e.position.y - box.getCentreY()) / rowH);
        if (isPositiveAndBelow (clicked, items.size())) sel = clicked;
    }
    choose();
}
bool DrumSelector::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    if (k == KeyPress::returnKey) { choose(); return true; }
    if (k == KeyPress::upKey)   { sel = jmax (0, sel - 1); scroll = (float) sel; repaint(); return true; }
    if (k == KeyPress::downKey) { sel = jmin (items.size() - 1, sel + 1); scroll = (float) sel; repaint(); return true; }
    return false;
}
void DrumSelector::choose() { if (onChoose) onChoose (sel); if (onClose) onClose(); }

//==============================================================================
// EDITEUR
//==============================================================================
SynthetikaEditor::SynthetikaEditor (SynthetikaProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    background = png (BinaryData::background_png, BinaryData::background_pngSize);
    columns[0] = png (BinaryData::column3_png, BinaryData::column3_pngSize);
    columns[1] = png (BinaryData::column4_png, BinaryData::column4_pngSize);
    columns[2] = png (BinaryData::column5_png, BinaryData::column5_pngSize);
    const void* up[5] = { BinaryData::key_eng0_up_png, BinaryData::key_eng1_up_png, BinaryData::key_eng2_up_png, BinaryData::key_eng3_up_png, BinaryData::key_eng4_up_png };
    const int upN[5] = { BinaryData::key_eng0_up_pngSize, BinaryData::key_eng1_up_pngSize, BinaryData::key_eng2_up_pngSize, BinaryData::key_eng3_up_pngSize, BinaryData::key_eng4_up_pngSize };
    const void* dn[5] = { BinaryData::key_eng0_dn_png, BinaryData::key_eng1_dn_png, BinaryData::key_eng2_dn_png, BinaryData::key_eng3_dn_png, BinaryData::key_eng4_dn_png };
    const int dnN[5] = { BinaryData::key_eng0_dn_pngSize, BinaryData::key_eng1_dn_pngSize, BinaryData::key_eng2_dn_pngSize, BinaryData::key_eng3_dn_pngSize, BinaryData::key_eng4_dn_pngSize };
    for (int i = 0; i < 5; ++i) { keyEngUp[i] = png (up[i], upN[i]); keyEngDn[i] = png (dn[i], dnN[i]); }
    keyModeUp = png (BinaryData::key_mode_up_png, BinaryData::key_mode_up_pngSize);
    keyModeDn = png (BinaryData::key_mode_dn_png, BinaryData::key_mode_dn_pngSize);
    swOnOff = png (BinaryData::sw_on_off_png, BinaryData::sw_on_off_pngSize);
    swOnOn  = png (BinaryData::sw_on_on_png, BinaryData::sw_on_on_pngSize);
    swPPOff = png (BinaryData::sw_pp_off_png, BinaryData::sw_pp_off_pngSize);
    swPPOn  = png (BinaryData::sw_pp_on_png, BinaryData::sw_pp_on_pngSize);
    fsRefl  = png (BinaryData::fs_refl_png, BinaryData::fs_refl_pngSize);
    boldFace = Typeface::createSystemTypefaceFor (BinaryData::PoppinsBold_ttf, BinaryData::PoppinsBold_ttfSize);
    medFace  = Typeface::createSystemTypefaceFor (BinaryData::PoppinsMedium_ttf, BinaryData::PoppinsMedium_ttfSize);
    if (auto xml = XmlDocument::parse (String::fromUTF8 (BinaryData::NineHandsLogoWhite_svg, BinaryData::NineHandsLogoWhite_svgSize)))
        logo = Drawable::createFromSVG (*xml);
    menuLnf.reg = medFace; menuLnf.med = medFace; menuLnf.bold = boldFace;
    setLookAndFeel (&menuLnf);
    glowPercent = NineHandsGlow::percent (JucePlugin_Name);
    buildPrint();

    nh::Knob::Layers L;
    L.shadow    = png (BinaryData::knob_shadow_png, BinaryData::knob_shadow_pngSize);
    L.crown     = png (BinaryData::knob_crown_png, BinaryData::knob_crown_pngSize);
    L.cap       = png (BinaryData::knob_cap_png, BinaryData::knob_cap_pngSize);
    L.marker    = png (BinaryData::knob_marker_png, BinaryData::knob_marker_pngSize);
    L.highlight = png (BinaryData::knob_highlight_png, BinaryData::knob_highlight_pngSize);
    for (const auto& km : knobMap)
    {
        auto* prm = proc.apvts.getParameter (km.param);
        jassert (prm != nullptr);
        auto k = std::make_unique<nh::Knob> (*prm, L);
        const String pid (km.param);
        k->onRightClick = [this, pid] (const MouseEvent&) { showContextMenu (SynthetikaProcessor::learnIndexOf (pid)); };
        k->onUserChange = [this, pid] { showStatus (pid); };
        addAndMakeVisible (*k);
        knobs.push_back (std::move (k));
        knobModule.push_back (km.module);
        knobParam.push_back (pid);
    }

    for (int i = 0; i < numWatch; ++i) lastVals[i] = proc.plain (watchIds[i]);

    const int defaultW = roundToInt ((float) defaultH * baseW / baseH);
    setResizable (true, true);
    setResizeLimits (defaultW, defaultH, roundToInt ((float) maxH * baseW / baseH), maxH);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    setOpaque (true);
    setSize (defaultW, defaultH);
    const int saved = proc.uiHeight.load();
    setScaledHeight (saved > 0 ? saved : sizeHeight (WigaWindowSize::defaultIndex (JucePlugin_Name, numSizes)));
    rebuildCaches();
    startTimerHz (30);
}

SynthetikaEditor::~SynthetikaEditor()
{
    stopTimer();
    drum.reset();
    setLookAndFeel (nullptr);
}

int SynthetikaEditor::sizeHeight (int idx)
{
    const int h[numSizes] = { defaultH, roundToInt (defaultH * 1.25f), roundToInt (defaultH * 1.5f), defaultH * 2 };
    return h[jlimit (0, numSizes - 1, idx)];
}

void SynthetikaEditor::setScaledHeight (int h)
{
    h = jlimit (defaultH, maxH, h);
    setSize (roundToInt ((float) h * baseW / baseH), h);
}

Rectangle<int> SynthetikaEditor::scaled (Rectangle<float> r) const
{
    return r.transformedBy (AffineTransform::scale (unitScale())).getSmallestIntegerContainer().expanded (1);
}

float SynthetikaEditor::physicalScale() const
{
    if (forcedScale > 0.0f) return forcedScale;
    return jlimit (1.0f, 4.0f, (float) Component::getApproximateScaleFactorForComponent (this));
}

//==============================================================================
// colonnes reordonnables
//==============================================================================
int SynthetikaEditor::moduleAtSlot (int slot) const
{
    int ord[3]; synthetika::orderOf (roundToInt (proc.plain ("order")), ord);
    return effectModule[ord[jlimit (0, 2, slot - 3)]];
}
int SynthetikaEditor::slotOfModule (int m) const
{
    for (int s = 3; s <= 5; ++s) if (moduleAtSlot (s) == m) return s;
    return m;
}
float SynthetikaEditor::columnDx (int m) const
{
    if (m < 3) return 0.0f;
    float dx = layout::modX[slotOfModule (m)] - layout::modX[m];
    if (m == dragModule) dx += dragOffset;
    return dx;
}

void SynthetikaEditor::layoutKnobs()
{
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        for (const auto& k : layout::knobs)
            if (String (k.id) == knobMap[i].layoutId)
            {
                const float dx = columnDx (knobModule[i]);
                knobs[i]->setBounds (scaled ({ k.x + dx - k.size * 0.5f, k.y - k.size * 0.5f, k.size, k.size }).reduced (1));
            }
    }
}

void SynthetikaEditor::resized()
{
    layoutKnobs();
    if (drum != nullptr) drum->setBounds (getLocalBounds());
    proc.uiHeight = getHeight();
    resizeSettle = 4;
}

//==============================================================================
// Caches (timer seulement)
//==============================================================================
void SynthetikaEditor::rebuildCaches (float sc)
{
    if (sc <= 0.0f) sc = cacheScale > 0.0f ? cacheScale : physicalScale();
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    if (w == cacheW && h == cacheH && std::abs (sc - cacheScale) < 0.01f && bgScaled.isValid()) return;
    cacheW = w; cacheH = h; cacheScale = sc;
    const int pw = roundToInt ((float) w * sc), ph = roundToInt ((float) h * sc);
    bgScaled = pw >= background.getWidth() ? background : background.rescaled (pw, ph, Graphics::highResamplingQuality);
    const float k = (float) ph / (float) background.getHeight();
    for (int i = 0; i < 3; ++i)
    {
        const int cw = roundToInt ((float) columns[i].getWidth() * k), ch = roundToInt ((float) columns[i].getHeight() * k);
        columnsScaled[i] = k >= 1.0f ? columns[i] : columns[i].rescaled (jmax (1, cw), jmax (1, ch), Graphics::highResamplingQuality);
    }
    repaint();
}

//==============================================================================
// Serigraphie vectorielle (Layout.h) : chemins en unites, groupes par couleur et par colonne
//==============================================================================
Path SynthetikaEditor::textPath (const String& s, float em, int fontIdx, float track, int anchor, float sy, bool inkCentre, float rot) const
{
    const Font font (FontOptions (fontIdx == 1 ? medFace : boldFace).withPointHeight (em));
    const float tr = track * em;
    float total = 0.0f;
    for (int i = 0; i < s.length(); ++i) total += GlyphArrangement::getStringWidth (font, String::charToString (s[i])) + (i > 0 ? tr : 0.0f);
    const float base = (font.getAscent() - font.getDescent()) * 0.5f;
    GlyphArrangement ga;
    float x = anchor == 0 ? -total * 0.5f : (anchor == 1 ? 0.0f : -total);
    for (int i = 0; i < s.length(); ++i)
    {
        const String ch = String::charToString (s[i]);
        ga.addLineOfText (font, ch, x, base);
        x += GlyphArrangement::getStringWidth (font, ch) + tr;
    }
    Path p; ga.createPath (p);
    if (sy != 1.0f) p.applyTransform (AffineTransform::scale (1.0f, sy));
    if (inkCentre) { const auto b = p.getBounds(); p.applyTransform (AffineTransform::translation (-b.getCentreX(), -b.getCentreY())); }
    if (rot != 0.0f) p.applyTransform (AffineTransform::rotation (-rot * MathConstants<float>::pi / 180.0f));
    return p;
}

void SynthetikaEditor::buildPrint()
{
    print.clear(); frames.clear();
    auto itemFor = [this] (Colour c, int mod) -> Path&
    {
        for (auto& it : print) if (it.col == c && it.mod == mod) return it.path;
        print.push_back ({ Path(), c, mod });
        return print.back().path;
    };
    for (const auto& t : layout::texts)
    {
        auto p = textPath (t.s, t.em, t.font, t.track, t.anchor, t.sy, t.ink, t.rot);
        p.applyTransform (AffineTransform::translation (t.x, t.y));
        itemFor (Colour (t.col), t.mod).addPath (p);
    }
    for (const auto& l : layout::lines)
    {
        Path seg; seg.startNewSubPath (l.x0, l.y0); seg.lineTo (l.x1, l.y1);
        Path stroked; PathStrokeType (l.w, PathStrokeType::mitered, PathStrokeType::butt).createStrokedPath (stroked, seg);
        itemFor (Colour (l.col), l.mod).addPath (stroked);
    }
    for (const auto& d : layout::dots) itemFor (Colour (d.col), d.mod).addEllipse (d.x - d.r, d.y - d.r, d.r * 2, d.r * 2);
    emblem.clear(); emblemArea = {};
    for (const auto& q : layout::polys)
    {
        Path p; p.startNewSubPath (q.p[0], q.p[1]); p.lineTo (q.p[2], q.p[3]); p.lineTo (q.p[4], q.p[5]); p.lineTo (q.p[6], q.p[7]); p.closeSubPath();
        emblem.push_back ({ p, Colour (q.col) });
        emblemArea = emblemArea.isEmpty() ? p.getBounds() : emblemArea.getUnion (p.getBounds());
    }
    emblemArea = emblemArea.expanded (16.0f);
    for (const auto& f : layout::frames)
        frames.push_back ({ { f.x0, f.y0, f.x1 - f.x0, f.y1 - f.y0 }, f.rad, f.w, Colour (f.col), f.hasGap, { f.gx, f.gy, f.gw, f.gh } });

    const float emKey = 13.0f * 1.12f, emOn = 12.5f * 1.12f;
    for (int i = 0; i < 5; ++i) engLabel[i] = textPath (layout::engNames[i], emKey, 0, 0.03f, 0);
    for (int i = 0; i < 4; ++i) modeLabel[i] = textPath (layout::modeNames[i], emKey, 0, 0.03f, 0);
    onLabel = textPath ("ON", emOn, 0, 0.06f, 0);
}

void SynthetikaEditor::drawPrint (Graphics& g, int mod, float dx)
{
    for (const auto& it : print)
        if (it.mod == mod)
        {
            g.setColour (it.col);
            g.fillPath (it.path, AffineTransform::translation (dx, 0.0f));
        }
}

//==============================================================================
// Commandes peintes (touches, interrupteurs) d'une colonne
//==============================================================================
void SynthetikaEditor::drawModuleControls (Graphics& g, int m, float dx)
{
    const bool on = proc.plain (onIds[m]) > 0.5f;
    const auto onC = Point<float> (layout::modC[m] + dx, layout::onY);
    g.drawImage (on ? swOnOn : swOnOff, centred (onC, layout::onSprW, layout::onSprH));
    g.setColour (onText);
    g.fillPath (onLabel, AffineTransform::translation (onC.x + 7.0f, onC.y));

    if (m == 0)
    {
        const int eng = roundToInt (proc.plain ("engine"));
        for (int i = 0; i < 5; ++i)
        {
            const bool sel = i == eng;
            const auto c = layout::engPos[i];
            g.drawImage (sel ? keyEngDn[i] : keyEngUp[i], centred (c, layout::keySprW, layout::keySprH));
            g.setColour (ink);
            g.fillPath (engLabel[i], AffineTransform::translation (c.x + (sel ? 1.0f : 0.0f), c.y + (sel ? 1.2f : 0.0f)));
        }
        // intervalles (ecrans encastres dans le fond)
        const bool harm = eng == 4;
        const int i1 = roundToInt (proc.plain ("int1")) - 24;
        const int i2 = roundToInt (proc.plain ("int2"));
        const String v[2] = { (i1 > 0 ? "+" : "") + String (i1), i2 == 0 ? String ("OFF") : ((i2 - 25 > 0 ? "+" : "") + String (i2 - 25)) };
        g.setFont (Font (FontOptions (medFace).withPointHeight (15.0f)));
        for (int r = 0; r < 2; ++r)
        {
            const float y = layout::intRows[r];
            const auto disp = Rectangle<float> (layout::intX0, y - layout::intH * 0.5f, layout::intX1 - layout::intX0, layout::intH);
            g.setColour (amber.withAlpha (harm ? 1.0f : 0.32f));
            g.drawText (v[r], disp, Justification::centred);
            for (int s = -1; s <= 1; s += 2)
            {
                const float ax = s < 0 ? layout::intX0 - layout::intArrowDx : layout::intX1 + layout::intArrowDx;
                Path tri; tri.addTriangle (ax + s * 4.5f, y, ax - s * 4.5f, y - 5.0f, ax - s * 4.5f, y + 5.0f);
                g.setColour (Colour (0xff1e1c22).withAlpha (harm ? 1.0f : 0.45f));
                g.fillPath (tri);
            }
        }
    }
    else if (m == 1)
    {
        const int mode = roundToInt (proc.plain ("distMode"));
        for (int i = 0; i < 4; ++i)
        {
            const bool sel = i == mode;
            const auto c = Point<float> (layout::modeX, layout::modeY[i]);
            g.drawImage (sel ? keyModeDn : keyModeUp, centred (c, layout::modeSprW, layout::modeSprH));
            g.setColour (ink);
            g.fillPath (modeLabel[i], AffineTransform::translation (c.x + (sel ? 1.0f : 0.0f), c.y + (sel ? 1.2f : 0.0f)));
        }
        const int pos = roundToInt (proc.plain ("distPos"));
        const bool pre = pos != 1, post = pos != 0;
        g.drawImage (pre ? swPPOn : swPPOff, centred (layout::ppPos[0], layout::ppSprW, layout::ppSprH));
        g.drawImage (post ? swPPOn : swPPOff, centred (layout::ppPos[1], layout::ppSprW, layout::ppSprH));
    }
}

//==============================================================================
void SynthetikaEditor::drawPresetBar (Graphics& g)
{
    const auto r = layout::presetRect;
    const float a = statusAlpha, pa = 1.0f - a;
    g.setColour (Colour (0xff0e0c12)); g.fillRoundedRectangle (r.expanded (1.5f), r.getHeight() * 0.5f + 1.5f);
    g.setColour (Colour (0xff040406)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Colour (0xff9691a0).withAlpha (hoverPreset != 0 ? 0.85f : 0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
    const Font f (FontOptions (medFace).withPointHeight (layout::presetFont));
    const float aw = 34.0f;
    if (pa > 0.0f)
    {
        for (int s = 0; s < 2; ++s)
        {
            const auto ar = s == 0 ? r.withWidth (aw) : r.withTrimmedLeft (r.getWidth() - aw);
            const auto c = ar.getCentre(); const float sz = 5.5f;
            Path p;
            if (s == 0) p.addTriangle (c.x - sz, c.y, c.x + sz, c.y - 6, c.x + sz, c.y + 6);
            else        p.addTriangle (c.x + sz, c.y, c.x - sz, c.y - 6, c.x - sz, c.y + 6);
            const bool hot = hoverPreset == (s == 0 ? 1 : 3);
            g.setColour (Colours::white.withAlpha ((hot ? 1.0f : 0.6f) * pa));
            g.fillPath (p);
        }
        g.setFont (f);
        g.setColour ((midiFlash > 0 ? Colour (0xffff8fc4) : Colour (0xffebebf0)).withAlpha (pa * (hoverPreset == 2 ? 1.0f : 0.94f)));
        g.drawFittedText (presetLine, r.reduced (aw, 0).toNearestInt(), Justification::centred, 1, 0.6f);
    }
    if (a > 0.0f)
    {
        g.setFont (f);
        g.setColour (Colour (0xffebebf0).withAlpha (a));
        g.drawFittedText (statusLine, r.reduced (r.getHeight() * 0.5f, 0).toNearestInt(), Justification::centred, 1, 0.6f);
    }
    // icones enregistrer / supprimer sur leurs touches
    const Colour ic (0xffdcdad4);
    {
        const auto c = layout::saveC;
        Path p; p.addRoundedRectangle (c.x - 8, c.y - 8, 16, 16, 2);
        g.setColour (ic); g.fillPath (p);
        g.setColour (Colour (0xff36363c));
        g.fillRect (c.x - 4.5f, c.y - 8.0f, 9.0f, 5.0f);
        g.fillEllipse (c.x - 3.2f, c.y + 0.5f, 6.4f, 6.0f);
    }
    {
        const auto c = layout::delC;
        const bool canDelete = ! proc.presetIsFactory();
        g.setColour (ic.withAlpha (canDelete ? 1.0f : 0.35f));
        g.fillRect (c.x - 6.0f, c.y - 4.0f, 12.0f, 13.0f);
        g.fillRect (c.x - 8.0f, c.y - 7.5f, 16.0f, 2.0f);
        g.fillRect (c.x - 2.5f, c.y - 9.5f, 5.0f, 2.0f);
        g.setColour (Colour (0xff36363c));
        for (int d = -3; d <= 3; d += 3) g.fillRect (c.x + d - 0.6f, c.y - 2.0f, 1.2f, 9.0f);
    }
}

void SynthetikaEditor::drawEmblem (Graphics& g)
{
    // pedale active : bandes retro-eclairees (couleur vive + halo dose par Glow) ; bypass : bandes eteintes, sombres
    const float lv = fsLevel, glow = (float) glowPercent / 100.0f;
    const Colour off (0xff34333a);
    if (lv > 0.003f)
        for (const auto& b : emblem)
            for (float w : { 22.0f, 17.0f, 13.0f, 9.5f, 6.5f, 3.5f })
            {
                g.setColour (b.col.withAlpha (0.06f * lv * glow));
                g.strokePath (b.path, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
            }
    for (const auto& b : emblem)
    {
        g.setColour (off.interpolatedWith (b.col, 0.38f + 0.62f * lv));   // eteint : couleurs sourdes mais lisibles
        g.fillPath (b.path);
    }
}

void SynthetikaEditor::drawFootswitch (Graphics& g)
{
    const auto c = layout::fsPos;
    const float lv = fsLevel;
    if (lv > 0.003f)
    {
        g.setOpacity (0.45f * lv); g.drawImage (fsRefl, centred (c, layout::fsReflS, layout::fsReflS)); g.setOpacity (1.0f);
        // anneau de lumiere dans la gorge (comme Octopus) : halo dose par Glow, trait net, coeur clair
        const float Rr = layout::fsBtnR * 1.0775f, Wr = layout::fsBtnR * 0.0674f, glow = (float) glowPercent / 100.0f;
        for (float w = 20.0f; w > 3.0f; w -= 1.5f)
        {
            // traits vers l'interieur de la gorge (comme l'apercu valide) : le halo ne deborde pas sur l'ecrou
            const float sw = w * Rr / 33.2f * 0.5f, rr = Rr - sw * 0.5f;
            g.setColour (pink.withAlpha (0.075f * 0.75f * glow * lv));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, sw);
        }
        g.setColour (pink.withAlpha (0.85f * lv)); g.drawEllipse (c.x - Rr, c.y - Rr, Rr * 2, Rr * 2, Wr);
        g.setColour (pink.interpolatedWith (Colours::white, 0.6f).withAlpha (0.9f * lv)); g.drawEllipse (c.x - Rr, c.y - Rr, Rr * 2, Rr * 2, Wr * 0.42f);
    }
    if (pressedHit.kind == hFs)
    {
        const float r = layout::fsBtnR * 0.98f;
        g.setColour (Colours::black.withAlpha (0.25f));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    }
}

//==============================================================================
void SynthetikaEditor::paint (Graphics& g)
{
    lastPaintMs = Time::getMillisecondCounterHiRes();
    repaintAskedMs = 0.0;
    g.fillAll (Colour (0xff111113));
    const Image& bg = bgScaled.isValid() ? bgScaled : background;
    g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
    if (bg.isValid()) g.drawImage (bg, getLocalBounds().toFloat());

    const float s = unitScale();
    g.addTransform (AffineTransform::scale (s));

    // colonnes reordonnables : plaque (decoupe du fond) a sa place dans l'ordre choisi ; celle qu'on glisse en dernier
    auto drawColumn = [&] (int m)
    {
        const int ci = m - 3;
        const auto r = layout::columnRect[ci];
        const float dx = columnDx (m);
        const Image& im = columnsScaled[ci].isValid() ? columnsScaled[ci] : columns[ci];
        if (m == dragModule)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r.translated (dx + 4, 6).reduced (2), 10);
        }
        g.drawImage (im, r.translated (dx, 0));
        drawPrint (g, m, dx);
        drawModuleControls (g, m, dx);
    };
    drawPrint (g, -1, 0.0f);
    for (const auto& f : frames)
    {
        g.saveState();
        if (f.hasGap) g.excludeClipRegion (f.gap.toNearestInt());
        g.setColour (f.col); g.drawRoundedRectangle (f.r, f.rad, f.w);
        g.restoreState();
    }
    for (int m = 0; m < 3; ++m) drawModuleControls (g, m, 0.0f);
    for (int m = 3; m < 6; ++m) if (m != dragModule) drawColumn (m);
    if (dragModule >= 3) drawColumn (dragModule);

    // LED d'entree (verte, niveau) et d'ecretage de sortie (rouge)
    auto led = [&] (Point<float> c, Colour col, float lv)
    {
        const float r = 5.5f;
        g.setColour (Colour (0xff0c0c0e)); g.fillEllipse (c.x - r - 1.5f, c.y - r - 1.5f, (r + 1.5f) * 2, (r + 1.5f) * 2);
        g.setColour (col.withMultipliedBrightness (0.28f + 0.72f * lv)); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        if (lv > 0.01f)
        {
            ColourGradient h (col.withAlpha (0.35f * lv * glowPercent / 100.0f), c.x, c.y, col.withAlpha (0.0f), c.x + 16, c.y, true);
            g.setGradientFill (h); g.fillEllipse (c.x - 16, c.y - 16, 32, 32);
        }
        g.setColour (Colours::white.withAlpha (0.45f)); g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.65f, r * 0.5f, r * 0.4f);
    };
    led (layout::ledIn, Colour (0xff50e65a), ledIn);
    led (layout::ledOut, Colour (0xffff3c32), ledOut);

    drawPresetBar (g);
    drawEmblem (g);
    drawFootswitch (g);
    if (logo != nullptr)
    {
        const auto vb = logo->getDrawableBounds();
        const float w = layout::logoW, h = w * vb.getHeight() / jmax (1.0f, vb.getWidth());
        logo->drawWithin (g, centred (layout::logoC, w, h), RectanglePlacement::centred, 1.0f);
    }
}

//==============================================================================
// Anti-gel v2
//==============================================================================
void SynthetikaEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer != lastPeer) { lastPeer = peer; repaint(); }
   #if JUCE_WINDOWS
    if (peer != nullptr)
    {
        int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
        if (sw < 0) sw = 0;
        if (peer->getCurrentRenderingEngine() != sw) peer->setCurrentRenderingEngine (sw);
    }
   #endif
}
void SynthetikaEditor::parentHierarchyChanged() { useSoftwareRenderer(); }
void SynthetikaEditor::visibilityChanged() { useSoftwareRenderer(); if (isShowing()) repaint(); }
void SynthetikaEditor::askRepaint (Rectangle<int> r)
{
    if (repaintAskedMs <= 0.0) repaintAskedMs = Time::getMillisecondCounterHiRes();
    repaint (r);
}

//==============================================================================
String SynthetikaEditor::statusText (const String& id) const
{
    auto* prm = proc.apvts.getParameter (id);
    if (prm == nullptr) return {};
    const int li = SynthetikaProcessor::learnIndexOf (id);
    const String name = li >= 0 ? SynthetikaProcessor::learnDef (li).name : prm->getName (40);
    if (id == "footswitch") return prm->getValue() > 0.5f ? "SYNTHETIKA ON" : "BYPASS";
    return (name + "  " + prm->getCurrentValueAsText()).toUpperCase();
}

void SynthetikaEditor::showStatus (const String& id)
{
    statusLine = statusText (id);
    statusUntil = Time::getMillisecondCounterHiRes() + 2500.0;
}

void SynthetikaEditor::timerCallback()
{
    const double now = Time::getMillisecondCounterHiRes();
    if (getPeer() != lastPeer || now - lastRendererCheckMs > 1000.0) { lastRendererCheckMs = now; useSoftwareRenderer(); }
    if (resizeSettle > 0 && --resizeSettle == 0) rebuildCaches();
    else if (resizeSettle == 0 && now - lastScaleCheckMs > 250.0)
    {
        lastScaleCheckMs = now;
        const float sc = physicalScale();
        if (std::abs (sc - cacheScale) > 0.01f) rebuildCaches (sc);
    }
    if (repaintAskedMs > 0.0 && isShowing() && now - repaintAskedMs > 1500.0 && lastPaintMs < repaintAskedMs)
    {
        repaintAskedMs = 0.0; lastPeer = nullptr; useSoftwareRenderer(); repaint();
    }

    // changements de reglages : ligne d'etat et redessin des commandes peintes
    bool discrete = false; int changed = -1, nChanged = 0;
    for (int i = 0; i < numWatch; ++i)
    {
        const float v = proc.plain (watchIds[i]);
        if (std::abs (v - lastVals[i]) > 1.0e-4f)
        {
            lastVals[i] = v; changed = i; ++nChanged;
            const String id (watchIds[i]);
            if (! (id == "input" || id == "mix" || id == "output" || id == "synthMix" || id == "distGain" || id.startsWith ("mod") && ! id.endsWith ("On")
                   || id == "ringFreq" || id == "ringMix" || id == "crush" || id == "bits" || id == "verbTime" || id == "verbMix"))
                discrete = true;
            if (id == "order") layoutKnobs();
        }
    }
    const int pc = proc.presetCounter.load();
    const bool presetChanged = pc != lastPresetCounter;
    if (presetChanged)
    {
        lastPresetCounter = pc;
        presetLine = proc.presetName() + (proc.presetIsModified() ? " *" : "");
        nChanged = 0; statusUntil = 0;
        askRepaintUnits (layout::presetRect.expanded (60, 4));
    }
    if (nChanged == 1 && ! presetChanged) { showStatus (watchIds[changed]); proc.markModified(); }
    if (discrete) repaint();

    // MIDI Learn : attente puis confirmation dans la ligne d'etat
    const int lt = proc.learnTarget.load();
    const int learned = proc.lastLearned.exchange (-1);
    if (learned >= 0) { statusLine = (String (SynthetikaProcessor::learnDef (learned).name) + " = CC " + String (proc.ccMap[learned].load())).toUpperCase(); statusUntil = now + 2000; }
    else if (lt >= 0) { statusLine = "MIDI LEARN \xc2\xb7 " + String (SynthetikaProcessor::learnDef (lt).name).toUpperCase(); statusUntil = now + 250; }
    const int mc = proc.midiCounter.load();
    if (mc != lastMidi) { lastMidi = mc; midiFlash = 6; }
    if (midiFlash > 0 && --midiFlash == 0) askRepaintUnits (layout::presetRect.expanded (3));

    const float target = now < statusUntil ? 1.0f : 0.0f;
    statusAlpha += (target - statusAlpha) * (target > statusAlpha ? 0.45f : 0.25f);
    if (statusAlpha < 0.005f) statusAlpha = 0; if (statusAlpha > 0.995f) statusAlpha = 1;
    if (std::abs (statusAlpha - drawnStatus) > 0.003f || (statusAlpha > 0 && nChanged > 0))
    {
        drawnStatus = statusAlpha;
        askRepaintUnits (layout::presetRect.expanded (3));
    }

    // LED et anneau du footswitch : fondu 0,45 a l'allumage / 0,28 a l'extinction
    const float inL = proc.inLevel.load();
    const float inTarget = jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (inL, -80.0f) + 54.0f) / 40.0f);
    ledIn += (inTarget - ledIn) * (inTarget > ledIn ? 0.45f : 0.28f);
    if (proc.outLevel.load() > 0.944f) clipUntil = now + 300.0;
    const float outTarget = now < clipUntil ? 1.0f : 0.0f;
    ledOut += (outTarget - ledOut) * (outTarget > ledOut ? 0.45f : 0.28f);
    const float fsTarget = proc.plain ("footswitch") > 0.5f ? 1.0f : 0.0f;
    fsLevel += (fsTarget - fsLevel) * (fsTarget > fsLevel ? 0.45f : 0.28f);
    if (std::abs (fsLevel - fsTarget) < 0.003f) fsLevel = fsTarget;
    if (std::abs (ledIn - drawnLedIn) > 0.01f) { drawnLedIn = ledIn; askRepaintUnits (centred (layout::ledIn, 36, 36)); }
    if (std::abs (ledOut - drawnLedOut) > 0.01f) { drawnLedOut = ledOut; askRepaintUnits (centred (layout::ledOut, 36, 36)); }
    if (std::abs (fsLevel - drawnFs) > 0.004f) { drawnFs = fsLevel; askRepaintUnits (centred (layout::fsPos, 120, 120)); askRepaintUnits (emblemArea); }
}

//==============================================================================
// Souris
//==============================================================================
SynthetikaEditor::Hit SynthetikaEditor::hitAt (Point<float> u) const
{
    Hit h;
    const auto pr = layout::presetRect;
    if (pr.contains (u)) { h.kind = u.x < pr.getX() + 34 ? hPresetPrev : (u.x > pr.getRight() - 34 ? hPresetNext : hPresetName); return h; }
    if (centred (layout::saveC, 34, 34).contains (u)) { h.kind = hSave; return h; }
    if (centred (layout::delC, 34, 34).contains (u))  { h.kind = hDelete; return h; }
    for (int i = 0; i < 5; ++i) if (centred (layout::engPos[i], layout::keyW + 6, layout::keyH + 6).contains (u)) { h.kind = hEngine; h.a = i; return h; }
    for (int i = 0; i < 4; ++i) if (centred ({ layout::modeX, layout::modeY[i] }, layout::modeW + 4, layout::modeH + 6).contains (u)) { h.kind = hMode; h.a = i; return h; }
    for (int i = 0; i < 2; ++i) if (centred (layout::ppPos[i], layout::ppW + 6, layout::ppH + 8).contains (u)) { h.kind = hPrePost; h.a = i; return h; }
    for (int m = 0; m < 6; ++m)
        if (centred ({ layout::modC[m] + columnDx (m), layout::onY }, layout::onW + 6, layout::onH + 8).contains (u)) { h.kind = hOn; h.a = m; return h; }
    for (int r = 0; r < 2; ++r)
    {
        const float y = layout::intRows[r];
        if (centred ({ layout::intX0 - layout::intArrowDx, y }, 26, 24).contains (u)) { h.kind = hIntArrow; h.a = r; h.b = -1; return h; }
        if (centred ({ layout::intX1 + layout::intArrowDx, y }, 26, 24).contains (u)) { h.kind = hIntArrow; h.a = r; h.b = 1; return h; }
        if (Rectangle<float> (layout::intX0, y - 13, layout::intX1 - layout::intX0, 26).contains (u)) { h.kind = hIntDisp; h.a = r; return h; }
    }
    if (u.getDistanceFrom (layout::fsPos) < layout::fsBtnR * 1.45f) { h.kind = hFs; return h; }
    for (int s = 3; s <= 5; ++s)
        if (Rectangle<float> (layout::modX[s], layout::titleY - 18, layout::modW[s], 36).contains (u)) { h.kind = hHandle; h.a = moduleAtSlot (s); return h; }
    return h;
}

int SynthetikaEditor::learnIndexForHit (const Hit& h) const
{
    switch (h.kind)
    {
        case hEngine:   return SynthetikaProcessor::learnIndexOf ("engine");
        case hMode:     return SynthetikaProcessor::learnIndexOf ("distMode");
        case hPrePost:  return SynthetikaProcessor::learnIndexOf ("distPos");
        case hOn:       return SynthetikaProcessor::learnIndexOf (onIds[h.a]);
        case hIntArrow:
        case hIntDisp:  return SynthetikaProcessor::learnIndexOf (h.a == 0 ? "int1" : "int2");
        case hFs:       return SynthetikaProcessor::learnIndexOf ("footswitch");
        case hHandle:   return SynthetikaProcessor::learnIndexOf ("order");
        default:        return -1;
    }
}

void SynthetikaEditor::toggleParam (const char* id)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture(); p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f); p->endChangeGesture();
    }
}

void SynthetikaEditor::setChoice (const char* id, int idx)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) idx)); p->endChangeGesture();
    }
}

void SynthetikaEditor::mouseDown (const MouseEvent& e)
{
    const auto u = e.position / unitScale();
    const auto h = hitAt (u);
    if (e.mods.isPopupMenu()) { showContextMenu (learnIndexForHit (h)); return; }
    if (statusAlpha > 0.0f && layout::presetRect.contains (u)) statusUntil = 0;     // la ligne d'etat ne bloque pas les clics
    pressedHit = h;
    switch (h.kind)
    {
        case hPresetPrev: proc.stepPreset (-1); break;
        case hPresetNext: proc.stepPreset (1); break;
        case hPresetName: showPresetMenu(); break;
        case hSave:       askSavePreset(); break;
        case hDelete:     askDeletePreset(); break;
        case hEngine:     setChoice ("engine", h.a); break;
        case hMode:       setChoice ("distMode", h.a); break;
        case hPrePost:
        {
            // PRE et POST : au moins un reste actif (le dernier utilise reste allume)
            const int pos = roundToInt (proc.plain ("distPos"));
            bool pre = pos != 1, post = pos != 0;
            if (h.a == 0) { if (! (pre && ! post)) pre = ! pre; } else { if (! (post && ! pre)) post = ! post; }
            setChoice ("distPos", pre && post ? 2 : (post ? 1 : 0));
            break;
        }
        case hOn:         toggleParam (onIds[h.a]); break;
        case hIntArrow:
        {
            const char* id = h.a == 0 ? "int1" : "int2";
            const int n = h.a == 0 ? 49 : 50;
            setChoice (id, jlimit (0, n - 1, roundToInt (proc.plain (id)) + h.b));
            break;
        }
        case hIntDisp:    openDrum (h.a); break;
        case hFs:         toggleParam ("footswitch"); repaint (scaled (centred (layout::fsPos, 120, 120))); break;
        case hHandle:     dragModule = h.a; dragStartX = u.x; dragOffset = 0; setMouseCursor (MouseCursor::DraggingHandCursor); break;
        default: break;
    }
}

void SynthetikaEditor::mouseDrag (const MouseEvent& e)
{
    if (dragModule < 3) return;
    const auto u = e.position / unitScale();
    const float lo = layout::modX[3] - layout::modX[slotOfModule (dragModule)], hi = layout::modX[5] - layout::modX[slotOfModule (dragModule)];
    dragOffset = jlimit (lo, hi, u.x - dragStartX);
    layoutKnobs();
    askRepaintUnits ({ layout::modX[3] - 10, layout::modY0 - 10, layout::modX[5] + layout::modW[5] - layout::modX[3] + 20, layout::modY1 - layout::modY0 + 20 });
}

void SynthetikaEditor::mouseUp (const MouseEvent&)
{
    if (dragModule >= 3)
    {
        // colonne deposee sur l'emplacement le plus proche : les autres gardent leur ordre
        const float cx = layout::modC[slotOfModule (dragModule)] + dragOffset;
        int target = 3; float best = 1e9f;
        for (int s = 3; s <= 5; ++s) if (std::abs (layout::modC[s] - cx) < best) { best = std::abs (layout::modC[s] - cx); target = s; }
        int ord[3]; synthetika::orderOf (roundToInt (proc.plain ("order")), ord);
        int eff = -1; for (int e = 0; e < 3; ++e) if (effectModule[e] == dragModule) eff = e;
        std::vector<int> list (ord, ord + 3);
        list.erase (std::find (list.begin(), list.end(), eff));
        list.insert (list.begin() + (target - 3), eff);
        const int newOrd[3] = { list[0], list[1], list[2] };
        dragModule = -1; dragOffset = 0;
        setChoice ("order", synthetika::orderIndex (newOrd));
        setMouseCursor (MouseCursor::NormalCursor);
        layoutKnobs(); repaint();
    }
    if (pressedHit.kind == hFs) repaint (scaled (centred (layout::fsPos, 120, 120)));
    pressedHit = {};
}

void SynthetikaEditor::mouseMove (const MouseEvent& e)
{
    const auto h = hitAt (e.position / unitScale());
    const int hz = h.kind == hPresetPrev ? 1 : h.kind == hPresetName ? 2 : h.kind == hPresetNext ? 3 : 0;
    if (hz != hoverPreset) { hoverPreset = hz; askRepaintUnits (layout::presetRect.expanded (3)); }
    setMouseCursor (h.kind == hHandle ? MouseCursor::DraggingHandCursor : h.kind != hNone ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void SynthetikaEditor::mouseExit (const MouseEvent&) { if (hoverPreset != 0) { hoverPreset = 0; askRepaintUnits (layout::presetRect.expanded (3)); } }

void SynthetikaEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const auto h = hitAt (e.position / unitScale());
    const int d = w.deltaY > 0 ? 1 : (w.deltaY < 0 ? -1 : 0);
    if (d == 0) return;
    if (h.kind == hIntDisp || h.kind == hIntArrow)
    {
        const char* id = h.a == 0 ? "int1" : "int2";
        setChoice (id, jlimit (0, h.a == 0 ? 48 : 49, roundToInt (proc.plain (id)) + d));
    }
    else if (h.kind == hEngine) setChoice ("engine", jlimit (0, 4, roundToInt (proc.plain ("engine")) - d));
    else if (h.kind == hMode)   setChoice ("distMode", jlimit (0, 3, roundToInt (proc.plain ("distMode")) - d));
}

//==============================================================================
void SynthetikaEditor::openDrum (int which)
{
    const char* id = which == 0 ? "int1" : "int2";
    auto* c = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter (id));
    if (c == nullptr) return;
    const float s = unitScale();
    const auto anchor = Rectangle<float> (layout::intX0, layout::intRows[which] - layout::intH * 0.5f, layout::intX1 - layout::intX0, layout::intH)
                            .transformedBy (AffineTransform::scale (s));
    String idStr (id);
    drum = std::make_unique<DrumSelector> (c->choices, c->getIndex(), anchor, amber, medFace,
                                           [this, idStr] (int idx) { setChoice (idStr.toRawUTF8(), idx); });
    drum->unit = s;
    Component::SafePointer<SynthetikaEditor> safeEd (this);
    drum->onClose = [safeEd] { MessageManager::callAsync ([safeEd] { if (auto* ed = safeEd.getComponent()) { ed->drum.reset(); ed->repaint(); } }); };
    drum->setBounds (getLocalBounds());
    addAndMakeVisible (*drum);
    drum->grabKeyboardFocus();
}

void SynthetikaEditor::showPresetMenu()
{
    PopupMenu m;
    m.addSectionHeader ("Factory presets");
    for (int i = 0; i < SynthetikaProcessor::getNumFactoryPresets(); ++i)
        m.addItem (100 + i, SynthetikaProcessor::getFactoryPreset (i).name, true, proc.presetIsFactory() && proc.getCurrentProgram() == i);
    const auto users = proc.userPresetNames();
    if (! users.isEmpty())
    {
        m.addSectionHeader ("User presets");
        for (int i = 0; i < users.size(); ++i) m.addItem (1000 + i, users[i], true, ! proc.presetIsFactory() && proc.presetName() == users[i]);
    }
    m.addSeparator();
    m.addItem (90, "Save preset...");
    m.setLookAndFeel (&menuLnf);
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (scaled (layout::presetRect))),
        [safe = Component::SafePointer<SynthetikaEditor> (this), users] (int r)
        {
            if (safe == nullptr || r == 0) return;
            if (r == 90) safe->askSavePreset();
            else if (r >= 100 && r < 1000) safe->proc.applyFactoryPreset (r - 100);
            else if (r >= 1000) safe->proc.loadUserPreset (users[r - 1000]);
        });
}

void SynthetikaEditor::askSavePreset()
{
    auto* aw = new AlertWindow ("Save preset", "Name of the user preset:", MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", proc.presetIsFactory() ? proc.presetName() + " (user)" : proc.presetName());
    aw->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    aw->enterModalState (true, ModalCallbackFunction::create ([safe = Component::SafePointer<SynthetikaEditor> (this), aw] (int r)
    {
        if (safe == nullptr || r != 1) return;
        const auto name = aw->getTextEditorContents ("name").trim();
        if (name.equalsIgnoreCase ("Default")) { AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Save preset", "\"Default\" is reserved."); return; }
        const auto file = SynthetikaProcessor::userPresetFolder().getChildFile (File::createLegalFileName (name) + ".preset");
        if (file.existsAsFile())
        {
            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Save preset", "Overwrite \"" + name + "\"?", "Overwrite", "Cancel", safe.getComponent(),
                ModalCallbackFunction::create ([safe, name] (int ok) { if (ok && safe != nullptr) safe->proc.saveUserPreset (name); }));
        }
        else safe->proc.saveUserPreset (name);
    }), true);
}

void SynthetikaEditor::askDeletePreset()
{
    if (proc.presetIsFactory()) return;       // les presets d'usine ne se suppriment pas
    const auto name = proc.presetName();
    AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Delete preset", "Delete \"" + name + "\"?", "Delete", "Cancel", this,
        ModalCallbackFunction::create ([safe = Component::SafePointer<SynthetikaEditor> (this), name] (int ok)
        {
            if (ok && safe != nullptr) { safe->proc.deleteUserPreset (name); safe->repaint(); }
        }));
}

//==============================================================================
// Menu clic droit (design d'Octopus, ouvert la ou on clique)
//==============================================================================
void SynthetikaEditor::showContextMenu (int learnIdx)
{
    PopupMenu menu;
    auto ccText = [this] (int i) { const int cc = proc.ccMap[i].load(); return cc >= 0 ? "CC " + String (cc) : String (CharPointer_UTF8 ("\xe2\x80\x94")); };
    const int lt = proc.learnTarget.load();
    if (learnIdx >= 0)
    {
        const String name = SynthetikaProcessor::learnDef (learnIdx).name;
        menu.addItem (1000 + learnIdx, "MIDI Learn: " + name + " (" + ccText (learnIdx) + ")", true, lt == learnIdx);
        menu.addItem (1100 + learnIdx, "Clear MIDI: " + name, proc.ccMap[learnIdx].load() >= 0);
        if (lt >= 0) menu.addItem (1200, "Cancel MIDI learn");
        menu.addSeparator();
    }
    PopupMenu sizes, defSizes;
    const char* names[numSizes] = { "100 %", "125 %", "150 %", "200 %" };
    const int def = WigaWindowSize::defaultIndex (JucePlugin_Name, numSizes);
    for (int i = 0; i < numSizes; ++i)
    {
        sizes.addItem (1 + i, String (names[i]) + (i == def ? "  (default)" : ""), true, std::abs (getHeight() - sizeHeight (i)) <= 2);
        defSizes.addItem (20 + i, names[i], true, i == def);
    }
    sizes.addSeparator(); sizes.addSubMenu ("Default size (new windows)", defSizes);
    menu.addSubMenu ("Plugin size", sizes);
    PopupMenu glow;
    for (int i = 0; i < NineHandsGlow::numSteps; ++i) glow.addItem (40 + i, String (NineHandsGlow::steps[i]) + " %", true, glowPercent == NineHandsGlow::steps[i]);
    menu.addSubMenu ("Glow", glow);
    menu.addSeparator();
    PopupMenu factory;
    for (int i = 0; i < SynthetikaProcessor::getNumFactoryPresets(); ++i)
        factory.addItem (100 + i, SynthetikaProcessor::getFactoryPreset (i).name, true, proc.presetIsFactory() && proc.getCurrentProgram() == i);
    menu.addSubMenu ("Factory presets", factory);
    menu.addItem (90, "Save preset...");
    PopupMenu order;
    if (auto* c = dynamic_cast<AudioParameterChoice*> (proc.apvts.getParameter ("order")))
        for (int i = 0; i < c->choices.size(); ++i) order.addItem (60 + i, c->choices[i], true, c->getIndex() == i);
    menu.addSubMenu ("Effect order", order);
    menu.addSeparator();
    PopupMenu settings;
    settings.addItem (51, "MIDI footswitch latching (127 = on / 0 = off)", true, proc.plain ("midiFsMode") < 0.5f);
    settings.addItem (52, "MIDI footswitch momentary (toggle each press)", true, proc.plain ("midiFsMode") > 0.5f);
    menu.addSubMenu ("Settings", settings);
    PopupMenu midi;
    for (int i = 0; i < SynthetikaProcessor::numLearn; ++i)
        midi.addItem (1000 + i, "Learn: " + String (SynthetikaProcessor::learnDef (i).name) + " (" + ccText (i) + ")", true, lt == i);
    midi.addSeparator();
    if (lt >= 0) midi.addItem (1200, "Cancel MIDI learn");
    midi.addItem (1201, "Clear all MIDI assignments");
    midi.addItem (1202, "CC n+32 = fine (14-bit) for knobs on CC 0-31", false);
    midi.addItem (1203, "Program Change 1-" + String (SynthetikaProcessor::getNumFactoryPresets()) + " = factory presets", false);
    menu.addSubMenu ("MIDI", midi);

    menu.setLookAndFeel (&menuLnf);
    menu.showMenuAsync (PopupMenu::Options().withMousePosition(),
        [safe = Component::SafePointer<SynthetikaEditor> (this)] (int r)
        {
            if (safe == nullptr || r == 0) return;
            auto& ed = *safe; auto& pr = ed.proc;
            if      (r >= 1 && r < 1 + numSizes) ed.setScaledHeight (sizeHeight (r - 1));
            else if (r >= 20 && r < 20 + numSizes) { WigaWindowSize::setDefaultIndex (JucePlugin_Name, r - 20); ed.setScaledHeight (sizeHeight (r - 20)); }
            else if (r >= 40 && r < 40 + NineHandsGlow::numSteps) { ed.glowPercent = NineHandsGlow::steps[r - 40]; NineHandsGlow::setPercent (JucePlugin_Name, ed.glowPercent); ed.repaint(); }
            else if (r == 51 || r == 52) ed.setChoice ("midiFsMode", r - 51);
            else if (r >= 60 && r < 66) ed.setChoice ("order", r - 60);
            else if (r == 90) ed.askSavePreset();
            else if (r >= 100 && r < 1000) pr.applyFactoryPreset (r - 100);
            else if (r >= 1000 && r < 1000 + SynthetikaProcessor::numLearn) pr.learnTarget.store (r - 1000);
            else if (r >= 1100 && r < 1100 + SynthetikaProcessor::numLearn) pr.ccMap[r - 1100].store (-1);
            else if (r == 1200) pr.learnTarget.store (-1);
            else if (r == 1201) { pr.learnTarget.store (-1); pr.resetMidiMap(); }
        });
}
