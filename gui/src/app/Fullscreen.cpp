#include "app/Fullscreen.hpp"
#include <X11/Xlib.h>
#include <X11/Xatom.h>

// Demande au window manager de (dé)maximiser la fenêtre — le même effet que
// cliquer sur le bouton "agrandir/restaurer" d'une barre de titre. On envoie
// un ClientMessage _NET_WM_STATE avec les deux atomes MAXIMIZED_VERT/HORZ à
// la racine. Le WM redimensionne la fenêtre existante (décoration conservée) :
// le contexte GL n'est PAS recréé.
void setWindowFullscreen(unsigned long x11Window, bool on)
{
    Display* d = XOpenDisplay(nullptr);
    if (!d) return;

    Atom wmState   = XInternAtom(d, "_NET_WM_STATE", False);
    Atom maxVert   = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom maxHorz   = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_HORZ", False);

    XEvent e{};
    e.type                 = ClientMessage;
    e.xclient.window       = (Window)x11Window;
    e.xclient.message_type = wmState;
    e.xclient.format       = 32;
    e.xclient.data.l[0]    = on ? 1 : 0;       // 1 = _NET_WM_STATE_ADD, 0 = _REMOVE
    e.xclient.data.l[1]    = (long)maxVert;
    e.xclient.data.l[2]    = (long)maxHorz;
    e.xclient.data.l[3]    = 1;                 // source : application normale

    XSendEvent(d, DefaultRootWindow(d), False,
               SubstructureNotifyMask | SubstructureRedirectMask, &e);
    XFlush(d);
    XCloseDisplay(d);
}
