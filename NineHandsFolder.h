#pragma once
// NINE HANDS | DSP (ex-Studio Wiga) - dossier de reglages commun aux plugins.
// Le dossier s'appelle "Nine Hands DSP" ('|' est interdit dans les noms Windows).
// Au premier appel, l'ancien dossier "Studio Wiga" est recopie s'il existe,
// pour garder presets utilisateur et taille de fenetre.
#include <juce_core/juce_core.h>
#include <mutex>

namespace nhd
{
    inline juce::String folderName()
    {
        static std::once_flag once;
        std::call_once (once, []
        {
            const auto uad = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
            for (auto parent : { uad, uad.getChildFile ("Application Support") })
            {
                const auto oldDir = parent.getChildFile ("Studio Wiga");
                const auto newDir = parent.getChildFile ("Nine Hands DSP");
                if (oldDir.isDirectory() && ! newDir.exists())
                    oldDir.copyDirectoryTo (newDir);
            }
        });
        return "Nine Hands DSP";
    }
}
