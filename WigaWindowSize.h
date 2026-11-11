#pragma once
#include <juce_core/juce_core.h>
#include "NineHandsFolder.h"

// NINE HANDS | DSP : taille par defaut des nouvelles fenetres du plugin, commune a toutes ses instances
// (choisie par clic droit > Plugin size > Default size (new windows), comme dans Octopus).
// Reglage enregistre dans le dossier de l'utilisateur :
//   Windows : %APPDATA%\NINE HANDS | DSP\<Plugin>\settings.xml
//   macOS   : ~/Library/Application Support/NINE HANDS | DSP/<Plugin>/settings.xml  (userApplicationDataDirectory)
// Indice 0 = taille par defaut d'origine (« 60 % »), puis les tailles suivantes du menu.
namespace WigaWindowSize
{
    inline juce::File file (const juce::String& plugin)
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile (nhd::folderName()).getChildFile (plugin).getChildFile ("settings.xml");
    }

    // indice de la taille par defaut (0 si rien n'a ete choisi ou fichier illisible)
    inline int defaultIndex (const juce::String& plugin, int numSizes)
    {
        const auto f = file (plugin);
        if (f.existsAsFile())
            if (auto x = juce::XmlDocument::parse (f))
                return juce::jlimit (0, juce::jmax (0, numSizes - 1), x->getIntAttribute ("defaultSize", 0));
        return 0;
    }

    inline void setDefaultIndex (const juce::String& plugin, int idx)
    {
        const auto f = file (plugin);
        juce::XmlElement x ("StudioWigaSettings");
        if (f.existsAsFile())
            if (auto old = juce::XmlDocument::parse (f))
                x = *old;
        x.setAttribute ("defaultSize", juce::jmax (0, idx));
        f.getParentDirectory().createDirectory();
        x.writeTo (f);
    }
}
