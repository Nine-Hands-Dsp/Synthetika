#pragma once
#include <juce_core/juce_core.h>
#include "NineHandsFolder.h"

// NINE HANDS | DSP (fichier commun a toutes les pedales, nov. 2026) : reglage « Glow intensity » du clic droit.
// Dose le halo des lumieres (LED, anneau, footswitch eclaire) sur la facade : 25, 50, 75 ou 100 % (defaut) ;
// la lumiere elle-meme reste toujours a fond. Commun a toutes les fenetres du plugin, enregistre a cote de la
// taille par defaut : attribut glowPercent de
//   Windows : %APPDATA%\Nine Hands DSP\<Plugin>\settings.xml   macOS : ~/Library/Application Support/Nine Hands DSP/<Plugin>/settings.xml
namespace NineHandsGlow
{
    constexpr int numSteps = 4;
    constexpr int steps[numSteps]  = { 25, 50, 75, 100 };
    constexpr int values[numSteps] = { 25, 50, 75, 100 };    // ancien nom, garde pour les pedales qui l'utilisent

    inline int sanitize (int pc)
    {
        int best = 100;
        for (int v : values) if (std::abs (v - pc) < std::abs (best - pc)) best = v;
        return best;
    }

    inline juce::File file (const juce::String& plugin)
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile (nhd::folderName()).getChildFile (plugin).getChildFile ("settings.xml");
    }

    inline int percent (const juce::String& plugin)
    {
        const auto f = file (plugin);
        if (f.existsAsFile())
            if (auto x = juce::XmlDocument::parse (f))
                return sanitize (x->getIntAttribute ("glowPercent", 100));
        return 100;
    }

    inline void setPercent (const juce::String& plugin, int pc)
    {
        const auto f = file (plugin);
        juce::XmlElement x ("StudioWigaSettings");
        if (f.existsAsFile())
            if (auto old = juce::XmlDocument::parse (f))
                x = *old;
        x.setAttribute ("glowPercent", sanitize (pc));
        f.getParentDirectory().createDirectory();
        x.writeTo (f);
    }
}
