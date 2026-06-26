#pragma once
// Bascule le mode maximisé d'une fenêtre via le window manager X11
// (_NET_WM_STATE_MAXIMIZED_VERT/_HORZ — comme le bouton "agrandir/restaurer"
// d'une barre de titre, pas un vrai plein écran sans décoration). Aucune
// recréation du contexte GL : toutes les ressources GPU (VAO, textures,
// shaders) restent valides. Linux / X11.
//
// x11Window : handle natif (sf::Window::getSystemHandle()).
void setWindowFullscreen(unsigned long x11Window, bool on);
