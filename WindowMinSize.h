#pragma once

#include <functional>

//==============================================================================
//  WindowMinSize : empêche la fenêtre du DAW qui contient le plugin d'être
//  réduite en dessous de la taille de la pédale.
//
//  Le format VST3 ne permet pas à un plugin d'interdire au DAW de rétrécir sa
//  propre fenêtre (Reaper, par exemple, laisse toujours tirer le bord et coupe
//  alors l'interface). On agit donc directement sur la fenêtre du DAW :
//   - Windows : la fenêtre parente reçoit une taille minimale (WM_GETMINMAXINFO),
//   - macOS   : la fenêtre parente reçoit une taille de contenu minimale.
//  La taille minimale = bordures et barres du DAW + taille de la pédale.
//  Si le DAW rouvre la fenêtre trop petite (taille mémorisée), elle est agrandie.
//==============================================================================
namespace wiga
{
    class WindowMinSize
    {
    public:
        WindowMinSize();
        ~WindowMinSize();

        // nativeHandle = getPeer()->getNativeHandle() (HWND sous Windows, NSView* sous macOS)
        // currentLogicalWidth : largeur actuelle (en points JUCE) du composant qui possède ce peer
        void attach (void* nativeHandle, std::function<int()> currentLogicalWidth);
        void detach();
        bool isAttachedTo (void* nativeHandle) const;

        // taille minimale de la pédale, en points JUCE
        void setMinimumSize (int logicalW, int logicalH);

        // taille maximale de la pédale (100 %), en points JUCE
        void setMaximumSize (int logicalW, int logicalH);

        // agrandit tout de suite la fenêtre du DAW si elle est plus petite que le minimum
        void enforceNow();

        struct Impl;
    private:
        Impl* impl = nullptr;
    };
}
