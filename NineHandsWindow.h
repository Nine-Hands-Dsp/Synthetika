#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "WigaWindowSize.h"
#include "NineHandsGlow.h"
#include "WindowMinSize.h"

//==============================================================================
// NINE HANDS | DSP : tronc commun des menus clic droit et de la taille de fenetre (fichier commun, nov. 2026).
//
// Taille de fenetre :
//  - trois tailles au menu (60 / 75 / 100 % sur les pedales), la taille par defaut des nouvelles fenetres est
//    choisie dans « Default size (new windows) » (settings.xml, commun a toutes les instances du plugin) ;
//  - chaque instance garde la taille choisie dans son menu (indice 0..n-1, enregistre dans l'etat du projet) ;
//    -1 = suit la taille par defaut. Choisir une taille par defaut remet l'instance a -1 (elle ne se fige pas).
//    Les tailles obtenues en tirant le coin ne sont pas enregistrees.
//  - « (default) » suit la taille par defaut choisie ;
//  - la fenetre ne descend jamais sous 60 % (fenetre du DAW comprise, voir WindowMinSize) et monte a la main
//    jusqu'a 150 % (coin de redimensionnement, proportions fixes).
//
// Menus : identifiants reserves 7000-7099 (aucune pedale ne les utilise).
//==============================================================================
namespace NineHandsWindow
{
    using namespace juce;

    constexpr int idSize = 7001, idDefault = 7021, idGlow = 7061, idLatch = 7081, idMomentary = 7082, idProgInfo = 7090;

    // taille d'ouverture : celle de l'instance, sinon la taille par defaut du plugin
    inline int openIndex (const String& plugin, int instanceIdx, int numSizes)
    {
        return (instanceIdx >= 0 && instanceIdx < numSizes) ? instanceIdx : WigaWindowSize::defaultIndex (plugin, numSizes);
    }

    // sous-menu « Plugin size » ; heights = hauteurs de fenetre de chaque taille (pour cocher la taille en cours)
    inline PopupMenu sizeMenu (const String& plugin, int numSizes, const char* const* names, const int* heights, int currentHeight)
    {
        const int def = WigaWindowSize::defaultIndex (plugin, numSizes);
        PopupMenu sizes, defSizes;
        for (int i = 0; i < numSizes; ++i)
        {
            sizes.addItem (idSize + i, String (names[i]) + (i == def ? "  (default)" : ""), true, std::abs (currentHeight - heights[i]) <= 2);
            defSizes.addItem (idDefault + i, names[i], true, i == def);
        }
        sizes.addSeparator();
        sizes.addSubMenu ("Default size (new windows)", defSizes);
        return sizes;
    }

    // r = resultat du menu. Renvoie l'indice de taille a appliquer (-1 si r n'est pas un choix de taille) ;
    // instanceIdx recoit la nouvelle valeur de l'instance (indice choisi, ou -1 = suit la taille par defaut).
    inline int handleSize (int r, const String& plugin, int numSizes, int& instanceIdx)
    {
        if (r >= idSize && r < idSize + numSizes)       { instanceIdx = r - idSize; return r - idSize; }
        if (r >= idDefault && r < idDefault + numSizes)
        {
            WigaWindowSize::setDefaultIndex (plugin, r - idDefault);     // toutes les nouvelles fenetres du plugin
            instanceIdx = -1;                                           // cette instance suit la taille par defaut
            return r - idDefault;
        }
        return -1;
    }

    // sous-menu « Glow intensity »
    inline PopupMenu glowMenu (int glowPc)
    {
        PopupMenu g;
        for (int i = 0; i < NineHandsGlow::numSteps; ++i)
        {
            const int v = NineHandsGlow::steps[i];
            g.addItem (idGlow + i, "Glow " + String (v) + " %" + (v == 100 ? "  (default)" : ""), true, glowPc == v);
        }
        return g;
    }

    // renvoie le pourcentage choisi (et l'enregistre pour toutes les fenetres du plugin), 0 sinon
    inline int handleGlow (int r, const String& plugin)
    {
        if (r < idGlow || r >= idGlow + NineHandsGlow::numSteps) return 0;
        const int pc = NineHandsGlow::steps[r - idGlow];
        NineHandsGlow::setPercent (plugin, pc);
        return pc;
    }

    // fin commune du sous-menu MIDI : latching / momentary des footswitchs MIDI, puis rappel Program Change
    inline void addMidiFootswitchMode (PopupMenu& midi, bool latching)
    {
        midi.addItem (idLatch, "MIDI footswitch latching (127 = on / 0 = off)", true, latching);
        midi.addItem (idMomentary, "MIDI footswitch momentary (toggle each press)", true, ! latching);
    }
    inline void addProgramChangeInfo (PopupMenu& midi)
    {
        midi.addItem (idProgInfo, "Program Change = factory presets", false);
    }

    //==========================================================================
    // Verrou de la fenetre du DAW (WindowMinSize) : la fenetre de l'hote ne peut pas etre reduite sous la taille
    // minimale du plugin, ni agrandie au-dela de la taille maximale. A appeler depuis parentHierarchyChanged()
    // et le timer de l'editeur.
    class HostLock
    {
    public:
        void setLimits (int minW, int minH, int maxW, int maxH)
        {
            mnW = minW; mnH = minH;
            lock.setMinimumSize (minW, minH);
            lock.setMaximumSize (maxW, maxH);
        }
        void track (Component& editor)
        {
            if (auto* peer = editor.getPeer())
            {
                void* handle = peer->getNativeHandle();
                if (handle != nullptr && ! lock.isAttachedTo (handle))
                {
                    auto* top = &peer->getComponent();
                    lock.attach (handle, [safe = Component::SafePointer<Component> (top)] { return safe != nullptr ? safe->getWidth() : 0; });
                    lock.enforceNow();
                }
                if (editor.getWidth() < mnW - 2 || editor.getHeight() < mnH - 2) lock.enforceNow();
            }
            else lock.detach();
        }
        void enforce() { lock.enforceNow(); }
    private:
        wiga::WindowMinSize lock;
        int mnW = 0, mnH = 0;
    };
}
