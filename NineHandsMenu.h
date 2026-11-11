#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// NINE HANDS | DSP : menus clic droit au design des menus d'Octopus (MenuStyle.h d'Octopus v1.17.0) :
// fond sombre arrondi, icones devant les commandes, coche bleue, sous-menus marques par un chevron.
namespace NineHandsMenu
{
    using namespace juce;

    const Colour kBg (0xff13171d), kBorder (0xff262c36), kHover (0xff1c222b), kSep (0xff20252e),
                 kText (0xffeef1f5), kDim (0xff8d96a4), kFaint (0xff3a414d), kAccent (0xff4fc3f7);

    inline Font fnt (const Typeface::Ptr& tf, float h) { return tf != nullptr ? Font (FontOptions (tf).withHeight (h)) : Font (FontOptions (h)); }

    enum Ico { icoNone = 0, icoSave, icoLoad, icoStomp, icoClear, icoMidi, icoLatch, icoSize, icoParams, icoPreset, icoBypass };

    inline int iconFor (const String& t)
    {
        const auto s = t.toLowerCase();
        if (s.startsWith ("save")) return icoSave;
        if (s.startsWith ("load")) return icoLoad;
        if (s.contains ("clear") || s.startsWith ("reset") || s.startsWith ("cancel")) return icoClear;
        if (s.startsWith ("footswitch") || s.contains ("momentary")) return icoStomp;
        if (s.contains ("latching") || s.contains ("toggles")) return icoLatch;
        if (s.startsWith ("midi") || s.startsWith ("learn") || s.startsWith ("program change") || s.startsWith ("cc ")) return icoMidi;
        if (s.contains ("size") || s.endsWith ("%")) return icoSize;
        if (s.startsWith ("settings") || s.contains ("sensitivity") || s.contains ("gate")) return icoParams;
        if (s.contains ("preset")) return icoPreset;
        if (s.contains ("bypass")) return icoBypass;
        return icoNone;
    }

    inline void drawIco (Graphics& g, int ico, Rectangle<float> r, Colour c)
    {
        if (ico == icoNone) return;
        const float k = r.getWidth() / 16.0f;
        auto P = [&] (float x, float y) { return Point<float> (r.getX() + x * k, r.getY() + y * k); };
        Path p;
        auto line = [&] (float x1, float y1, float x2, float y2) { p.startNewSubPath (P (x1, y1)); p.lineTo (P (x2, y2)); };
        auto circ = [&] (float cx, float cy, float rad) { p.addEllipse (P (cx - rad, cy - rad).x, P (cx - rad, cy - rad).y, rad * 2 * k, rad * 2 * k); };
        auto rect = [&] (float x, float y, float w, float h, float rr) { p.addRoundedRectangle (P (x, y).x, P (x, y).y, w * k, h * k, rr * k); };
        bool fillDots = false;
        switch (ico)
        {
            case icoSave:   p.startNewSubPath (P (2.5f, 2.5f)); p.lineTo (P (11, 2.5f)); p.lineTo (P (13.5f, 5)); p.lineTo (P (13.5f, 13.5f)); p.lineTo (P (2.5f, 13.5f)); p.closeSubPath();
                            p.startNewSubPath (P (5, 2.5f)); p.lineTo (P (5, 6)); p.lineTo (P (10, 6)); p.lineTo (P (10, 2.5f)); break;
            case icoLoad:   line (8, 2, 8, 10); line (4.5f, 6.5f, 8, 10); line (11.5f, 6.5f, 8, 10); line (3, 13.5f, 13, 13.5f); break;
            case icoStomp:  circ (8, 8, 5.5f); circ (8, 8, 2.0f); break;
            case icoClear:  line (4, 4, 12, 12); line (12, 4, 4, 12); break;
            case icoMidi:   circ (8, 8, 6.0f); fillDots = true; break;
            case icoLatch:  p.startNewSubPath (P (2, 11)); p.lineTo (P (5, 11)); p.lineTo (P (5, 5)); p.lineTo (P (8, 5)); p.lineTo (P (8, 11)); p.lineTo (P (11, 11)); p.lineTo (P (11, 5)); p.lineTo (P (14, 5)); break;
            case icoSize:   line (3, 3, 13, 13); line (3, 3, 3, 7); line (3, 3, 7, 3); line (13, 13, 13, 9); line (13, 13, 9, 13); break;
            case icoParams: line (3, 4, 13, 4); line (3, 8, 13, 8); line (3, 12, 13, 12); circ (6, 4, 1.4f); circ (10, 8, 1.4f); circ (5, 12, 1.4f); break;
            case icoPreset: rect (3, 2.5f, 10, 11, 1.5f); line (5.5f, 6, 10.5f, 6); line (5.5f, 9, 9, 9); break;
            case icoBypass: p.addCentredArc (P (8, 8.5f).x, P (8, 8.5f).y, 5.5f * k, 5.5f * k, 0.0f, 0.75f, MathConstants<float>::twoPi - 0.75f, true); line (8, 2, 8, 8); break;
            default: break;
        }
        g.setColour (c);
        g.strokePath (p, PathStrokeType (1.5f * k, PathStrokeType::curved, PathStrokeType::rounded));
        if (fillDots)
            for (auto d : { Point<float> (5, 7), Point<float> (11, 7), Point<float> (6.3f, 10.3f), Point<float> (9.7f, 10.3f), Point<float> (8, 4.7f) })
                g.fillEllipse (Rectangle<float> (1.8f * k, 1.8f * k).withCentre (P (d.x, d.y)));
    }

    inline void drawCheck (Graphics& g, Rectangle<float> r, Colour c, float w)
    {
        Path p;
        p.startNewSubPath (r.getX() + r.getWidth() * 0.18f, r.getY() + r.getHeight() * 0.52f);
        p.lineTo (r.getX() + r.getWidth() * 0.42f, r.getY() + r.getHeight() * 0.76f);
        p.lineTo (r.getX() + r.getWidth() * 0.84f, r.getY() + r.getHeight() * 0.26f);
        g.setColour (c);
        g.strokePath (p, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    }

    struct MenuLnf : public LookAndFeel_V4
    {
        Typeface::Ptr reg, med, bold;
        MenuLnf()
        {
            setColour (PopupMenu::backgroundColourId, Colours::transparentBlack);       // fenetre transparente : coins arrondis
            setColour (PopupMenu::textColourId, kText);
            setColour (PopupMenu::headerTextColourId, kDim);
            setColour (PopupMenu::highlightedBackgroundColourId, kHover);
            setColour (PopupMenu::highlightedTextColourId, Colours::white);
        }
        Font getPopupMenuFont() override { return fnt (med, 15.0f); }
        int getPopupMenuBorderSize() override { return 7; }
        void drawPopupMenuBackground (Graphics& g, int w, int h) override
        {
            const auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
            g.setColour (kBg);
            g.fillRoundedRectangle (r, 10.0f);
            g.setColour (kBorder);
            g.drawRoundedRectangle (r, 10.0f, 1.0f);
        }
        void getIdealPopupMenuItemSize (const String& text, bool isSeparator, int standardHeight, int& w, int& h) override
        {
            if (isSeparator) { w = 50; h = 11; return; }
            h = standardHeight > 0 ? standardHeight : 32;
            w = (int) std::ceil (GlyphArrangement::getStringWidth (getPopupMenuFont(), text)) + 34 + 40;
        }
        void drawPopupMenuSectionHeader (Graphics& g, const Rectangle<int>& area, const String& name) override
        {
            g.setFont (fnt (bold, 11.5f).withExtraKerningFactor (0.14f));
            g.setColour (kDim);
            g.drawFittedText (name.toUpperCase(), area.withTrimmedLeft (14).withTrimmedRight (10).withTrimmedTop (6), Justification::centredLeft, 1);
        }
        void drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                bool hasSubMenu, const String& text, const String& shortcut, const Drawable* icon, const Colour* textColour) override
        {
            ignoreUnused (shortcut, icon);
            const auto a = area.toFloat();
            if (isSeparator)
            {
                g.setColour (kSep);
                g.fillRect (a.withSizeKeepingCentre (a.getWidth() - 20.0f, 1.0f));
                return;
            }
            const bool hi = isHighlighted && isActive;
            if (hi)
            {
                g.setColour (kHover);
                g.fillRoundedRectangle (a.reduced (5.0f, 1.0f), 7.0f);
            }
            const auto ib = Rectangle<float> (15.0f, 15.0f).withCentre ({ a.getX() + 22.0f, a.getCentreY() });
            if (isTicked) drawCheck (g, ib.expanded (1.0f), kAccent, 2.0f);
            else drawIco (g, iconFor (text), ib, ! isActive ? kFaint : hi ? kAccent : kDim);
            auto tr = a.withTrimmedLeft (38.0f).withTrimmedRight (hasSubMenu ? 30.0f : 14.0f);
            g.setFont (getPopupMenuFont());
            g.setColour (textColour != nullptr ? *textColour : ! isActive ? kDim.withAlpha (0.55f) : hi ? Colours::white : kText);
            g.drawFittedText (text, tr.toNearestInt(), Justification::centredLeft, 1, 0.9f);
            if (hasSubMenu)
            {
                const float cx = a.getRight() - 18.0f, cy = a.getCentreY();
                Path p; p.startNewSubPath (cx - 2.5f, cy - 5.0f); p.lineTo (cx + 2.5f, cy); p.lineTo (cx - 2.5f, cy + 5.0f);
                g.setColour (hi ? Colours::white : kDim);
                g.strokePath (p, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
            }
        }
    };
}
