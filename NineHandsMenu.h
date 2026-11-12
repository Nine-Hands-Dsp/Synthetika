#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Menus clic droit NINE HANDS | DSP : design d'Octopus (v1.17, MenuStyle.h), commun a toutes les pedales.
// Fond sombre arrondi (bord fin), police Poppins, icone devant chaque commande (choisie d'apres le texte),
// coche bleue, fleche des sous-menus, titres de section en petites capitales espacees, et en-tete
// (pastille + titre + sous-titre) en haut du menu principal et des menus d'une commande.
// Usage :
//   NineHandsMenu::MenuLnf menuLnf;  menuLnf.f = { poppinsRegular, poppinsMedium, poppinsBold };
//   PopupMenu m; m.setLookAndFeel (&menuLnf);
//   NineHandsMenu::addHeader (m, menuLnf.f, NineHandsMenu::icoPedal, accent, "Strong Pickle", "Fuzz  v1.5.0");
//   (sous-menus : m.addSubMenu (...) herite du LookAndFeel du menu parent)
//==============================================================================
namespace NineHandsMenu
{
    using namespace juce;

    const Colour kBg (0xff13171d), kBorder (0xff262c36), kHover (0xff1c222b), kSep (0xff20252e),
                 kText (0xffeef1f5), kDim (0xff8d96a4), kFaint (0xff3a414d), kAccent (0xff4fc3f7);

    struct Fonts { Typeface::Ptr reg, med, bold; };
    inline Font fnt (const Typeface::Ptr& tf, float h) { return tf != nullptr ? Font (FontOptions (tf).withHeight (h)) : Font (FontOptions (h)); }

    enum Ico { icoNone = 0, icoSave, icoLoad, icoClear, icoMidi, icoLatch, icoSize, icoPreset, icoGate, icoSettings, icoInfo, icoPedal, icoKnob, icoSwitch, icoGlow };

    inline int iconFor (const String& t)
    {
        const auto s = t.toLowerCase();
        if (s.startsWith ("save")) return icoSave;
        if (s.startsWith ("load")) return icoLoad;
        if (s.contains ("clear") || s.startsWith ("reset") || s.startsWith ("cancel")) return icoClear;
        if (s.contains ("glow") || s.contains ("halo")) return icoGlow;             // reglage du halo des lumieres (nov. 2026)
        if (s.contains ("latching") || s.contains ("momentary")) return icoLatch;
        if (s.startsWith ("midi") || s.startsWith ("learn") || s.startsWith ("program change") || s.startsWith ("cc ")) return icoMidi;
        if (s.contains ("size") || s.endsWith ("%") || s.contains ("% ")) return icoSize;
        if (s.contains ("preset") || s.startsWith ("user ")) return icoPreset;
        if (s.contains ("gate")) return icoGate;
        if (s.startsWith ("settings")) return icoSettings;
        if (s.contains (" v") && s.contains ("nine hands")) return icoInfo;
        return icoNone;
    }

    // petites icones sur une grille de 16
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
            case icoSave:     p.startNewSubPath (P (2.5f, 2.5f)); p.lineTo (P (11, 2.5f)); p.lineTo (P (13.5f, 5)); p.lineTo (P (13.5f, 13.5f)); p.lineTo (P (2.5f, 13.5f)); p.closeSubPath();
                              p.startNewSubPath (P (5, 2.5f)); p.lineTo (P (5, 6)); p.lineTo (P (10, 6)); p.lineTo (P (10, 2.5f)); break;
            case icoLoad:     line (8, 2, 8, 10); line (4.5f, 6.5f, 8, 10); line (11.5f, 6.5f, 8, 10); line (3, 13.5f, 13, 13.5f); break;
            case icoClear:    line (4, 4, 12, 12); line (12, 4, 4, 12); break;
            case icoMidi:     circ (8, 8, 6.0f); fillDots = true; break;
            case icoLatch:    p.startNewSubPath (P (2, 11)); p.lineTo (P (5, 11)); p.lineTo (P (5, 5)); p.lineTo (P (8, 5)); p.lineTo (P (8, 11)); p.lineTo (P (11, 11)); p.lineTo (P (11, 5)); p.lineTo (P (14, 5)); break;
            case icoSize:     line (3, 3, 13, 13); line (3, 3, 3, 7); line (3, 3, 7, 3); line (13, 13, 13, 9); line (13, 13, 9, 13); break;
            case icoPreset:   rect (3, 2.5f, 10, 11, 1.5f); line (5.5f, 6, 10.5f, 6); line (5.5f, 9, 9, 9); break;
            case icoGate:     p.startNewSubPath (P (2, 12)); p.lineTo (P (5, 12)); p.lineTo (P (5, 4)); p.lineTo (P (11, 4)); p.lineTo (P (11, 12)); p.lineTo (P (14, 12)); break;
            case icoSettings: circ (8, 8, 2.4f); for (int i = 0; i < 8; ++i) { const float a = (float) i * MathConstants<float>::pi / 4.0f;
                                  line (8 + 4.2f * std::cos (a), 8 + 4.2f * std::sin (a), 8 + 6.2f * std::cos (a), 8 + 6.2f * std::sin (a)); } break;
            case icoInfo:     circ (8, 8, 6.0f); line (8, 7.2f, 8, 11.5f); line (8, 4.6f, 8, 4.7f); break;
            case icoPedal:    rect (3.5f, 1.5f, 9, 13, 1.8f); circ (6, 5, 1.3f); circ (10, 5, 1.3f); circ (8, 11, 1.8f); break;
            case icoKnob:     circ (8, 8, 5.5f); line (8, 8, 11, 4.2f); break;
            case icoSwitch:   circ (8, 10.5f, 3.5f); line (8, 10.5f, 8, 2.5f); break;
            case icoGlow:     circ (8, 8, 2.8f); for (int i = 0; i < 8; ++i) { const float a = (float) i * MathConstants<float>::pi / 4.0f;
                                  line (8 + 4.6f * std::cos (a), 8 + 4.6f * std::sin (a), 8 + 6.6f * std::cos (a), 8 + 6.6f * std::sin (a)); } break;
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

    // ------------------------------------------------------------------ apparence de tous les menus
    struct MenuLnf : public LookAndFeel_V4
    {
        Fonts f;
        MenuLnf()
        {
            setColour (PopupMenu::backgroundColourId, Colours::transparentBlack);       // fenetre transparente : coins arrondis
            setColour (PopupMenu::textColourId, kText);
            setColour (PopupMenu::headerTextColourId, kDim);
            setColour (PopupMenu::highlightedBackgroundColourId, kHover);
            setColour (PopupMenu::highlightedTextColourId, Colours::white);
        }
        Font getPopupMenuFont() override { return fnt (f.med, 15.0f); }
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
            g.setFont (fnt (f.bold, 11.5f).withExtraKerningFactor (0.14f));
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
            if (hasSubMenu)                                                           // >
            {
                const float cx = a.getRight() - 18.0f, cy = a.getCentreY();
                Path p; p.startNewSubPath (cx - 2.5f, cy - 5.0f); p.lineTo (cx + 2.5f, cy); p.lineTo (cx - 2.5f, cy + 5.0f);
                g.setColour (hi ? Colours::white : kDim);
                g.strokePath (p, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
            }
        }
    };

    // ------------------------------------------------------------------ en-tete : pastille teintee (icone) + titre + sous-titre
    struct Header : public PopupMenu::CustomComponent
    {
        Fonts f; int ico; Colour col; String title, sub;
        Header (Fonts fo, int icon, Colour c, String t, String s) : CustomComponent (false), f (fo), ico (icon), col (c), title (t), sub (s) {}
        void getIdealSize (int& w, int& h) override
        {
            w = jmax (240, (int) std::ceil (jmax (GlyphArrangement::getStringWidth (fnt (f.bold, 15.5f), title), GlyphArrangement::getStringWidth (fnt (f.reg, 12.5f), sub))) + 72);
            h = 52;
        }
        void paint (Graphics& g) override
        {
            const auto b = Rectangle<float> (30.0f, 30.0f).withPosition (12.0f, 10.0f);
            g.setColour (col.withAlpha (0.16f));
            g.fillRoundedRectangle (b, 8.0f);
            g.setColour (col.withAlpha (0.7f));
            g.drawRoundedRectangle (b.reduced (0.6f), 8.0f, 1.2f);
            drawIco (g, ico, b.reduced (7.0f), col);
            g.setColour (kText);
            g.setFont (fnt (f.bold, 15.5f));
            g.drawText (title, Rectangle<float> (52.0f, sub.isEmpty() ? 15.0f : 8.0f, (float) getWidth() - 60.0f, 19.0f), Justification::centredLeft, true);
            if (sub.isNotEmpty())
            {
                g.setColour (kDim);
                g.setFont (fnt (f.reg, 12.5f));
                g.drawText (sub, Rectangle<float> (52.0f, 27.0f, (float) getWidth() - 60.0f, 16.0f), Justification::centredLeft, true);
            }
        }
    };

    inline void addHeader (PopupMenu& m, const Fonts& f, int ico, Colour c, const String& title, const String& sub = {})
    {
        m.addCustomItem (9001, std::make_unique<Header> (f, ico, c, title, sub), nullptr, title);
        m.addSeparator();
    }
}
