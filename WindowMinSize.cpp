#include "WindowMinSize.h"

#if defined (_WIN32)
//==============================================================================
//  Windows
//==============================================================================
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <algorithm>

#if defined (_MSC_VER)
 #pragma comment (lib, "comctl32.lib")
 #pragma comment (lib, "user32.lib")
#endif

namespace wiga
{
    static const UINT_PTR kSubclassId = 0x57494741;   // 'WIGA'

    struct WindowMinSize::Impl
    {
        HWND child = nullptr;      // fenêtre JUCE du plugin
        HWND root  = nullptr;      // fenêtre du DAW qui la contient
        std::function<int()> logicalWidth;
        int minW = 0, minH = 0;    // en points JUCE
        int maxW = 0, maxH = 0;    // 0 = pas de maximum

        // taille minimale de la fenêtre du DAW, en pixels physiques
        bool computeTrack (int logW, int logH, int& outW, int& outH) const
        {
            if (child == nullptr || root == nullptr || ! IsWindow (child) || ! IsWindow (root))
                return false;

            // GetWindowRect est exprimé dans le repère (DPI) de la fenêtre du DAW
            RECT cr {};
            GetWindowRect (child, &cr);
            const int lw = logicalWidth ? logicalWidth() : 0;
            const int pw = (int) (cr.right - cr.left);
            if (lw <= 0 || pw <= 0)
                return false;

            const double ratio = (double) pw / (double) lw;          // pixels physiques par point JUCE

            // position de la pédale dans la fenêtre du DAW (barres d'outils, liste d'effets…)
            RECT rw {}, cw {}, rc {};
            GetWindowRect (root, &rw);
            GetWindowRect (child, &cw);
            GetClientRect (root, &rc);
            POINT br { rc.right, rc.bottom };
            ClientToScreen (root, &br);

            const int left   = std::max (0, (int) (cw.left - rw.left));
            const int top    = std::max (0, (int) (cw.top  - rw.top));
            const int right  = std::max (0, (int) (rw.right  - br.x));   // bordure droite de la fenêtre
            const int bottom = std::max (0, (int) (rw.bottom - br.y));   // bordure basse

            outW = left + (int) (logW * ratio + 0.999) + right;
            outH = top  + (int) (logH * ratio + 0.999) + bottom;
            return true;
        }

        bool computeMinTrack (int& w, int& h) const { return computeTrack (minW, minH, w, h); }

        static LRESULT CALLBACK proc (HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR ref)
        {
            const LRESULT r = DefSubclassProc (h, msg, wp, lp);

            if (msg == WM_GETMINMAXINFO)
            {
                auto* self = reinterpret_cast<Impl*> (ref);
                int w = 0, hh = 0;
                if (self != nullptr && self->computeMinTrack (w, hh))
                {
                    auto* mmi = reinterpret_cast<MINMAXINFO*> (lp);
                    mmi->ptMinTrackSize.x = std::max ((LONG) w,  mmi->ptMinTrackSize.x);
                    mmi->ptMinTrackSize.y = std::max ((LONG) hh, mmi->ptMinTrackSize.y);

                    // et pas plus grande que la pédale à 100 %
                    int mw = 0, mh = 0;
                    if (self->maxW > 0 && self->maxH > 0 && self->computeTrack (self->maxW, self->maxH, mw, mh))
                    {
                        mmi->ptMaxTrackSize.x = std::max ((LONG) mw, mmi->ptMinTrackSize.x);
                        mmi->ptMaxTrackSize.y = std::max ((LONG) mh, mmi->ptMinTrackSize.y);
                    }
                }
            }
            return r;
        }

        void unhook()
        {
            if (root != nullptr && IsWindow (root))
                RemoveWindowSubclass (root, &Impl::proc, kSubclassId);
            root = nullptr;
            child = nullptr;
        }
    };

    WindowMinSize::WindowMinSize() : impl (new Impl()) {}
    WindowMinSize::~WindowMinSize() { detach(); delete impl; }

    bool WindowMinSize::isAttachedTo (void* nativeHandle) const
    {
        HWND c = (HWND) nativeHandle;
        return c != nullptr && impl->child == c && impl->root == GetAncestor (c, GA_ROOT);
    }

    void WindowMinSize::attach (void* nativeHandle, std::function<int()> currentLogicalWidth)
    {
        detach();
        HWND c = (HWND) nativeHandle;
        if (c == nullptr || ! IsWindow (c))
            return;

        HWND r = GetAncestor (c, GA_ROOT);
        impl->child = c;
        impl->logicalWidth = std::move (currentLogicalWidth);

        // fenêtre principale du DAW (plugin ancré dans le dock) : on n'y touche pas
        if (r == nullptr || r == c || GetMenu (r) != nullptr)
            return;

        if (SetWindowSubclass (r, &Impl::proc, kSubclassId, (DWORD_PTR) impl))
            impl->root = r;
    }

    void WindowMinSize::detach()
    {
        impl->unhook();
    }

    void WindowMinSize::setMinimumSize (int logicalW, int logicalH)
    {
        impl->minW = logicalW;
        impl->minH = logicalH;
    }

    void WindowMinSize::setMaximumSize (int logicalW, int logicalH)
    {
        impl->maxW = logicalW;
        impl->maxH = logicalH;
    }

    void WindowMinSize::enforceNow()
    {
        int w = 0, h = 0;
        if (impl->root == nullptr || ! impl->computeMinTrack (w, h))
            return;
        if (IsIconic (impl->root) || IsZoomed (impl->root))
            return;

        RECT rr {};
        GetWindowRect (impl->root, &rr);
        const int cw = (int) (rr.right - rr.left), ch = (int) (rr.bottom - rr.top);
        // tolérance de 2 px : à 125 / 150 % l'hôte arrondit la taille de sa fenêtre ; sans marge,
        // on la ré-agrandirait à chaque tic du timer et les deux se la renverraient en boucle
        if (cw < w - 2 || ch < h - 2)
            SetWindowPos (impl->root, nullptr, 0, 0, std::max (cw, w), std::max (ch, h),
                          SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
}

#elif defined (__APPLE__)
// implémentation macOS : WindowMinSize.mm

#else
//==============================================================================
//  autres systèmes : rien à faire
//==============================================================================
namespace wiga
{
    struct WindowMinSize::Impl {};
    WindowMinSize::WindowMinSize() : impl (new Impl()) {}
    WindowMinSize::~WindowMinSize() { delete impl; }
    bool WindowMinSize::isAttachedTo (void*) const { return true; }
    void WindowMinSize::attach (void*, std::function<int()>) {}
    void WindowMinSize::detach() {}
    void WindowMinSize::setMinimumSize (int, int) {}
    void WindowMinSize::setMaximumSize (int, int) {}
    void WindowMinSize::enforceNow() {}
}
#endif
