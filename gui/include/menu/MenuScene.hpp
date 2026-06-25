#pragma once
// 3D "alive" startup screen.
//
// Sequence: a small emissive light falls from the top to screen center, bursts
// into a 3D particle explosion that flashes a point light, then the ZAPPY logo
// is revealed with a mirrored reflection beneath it. Two 2D buttons (Start,
// Parameters) sit below. Start returns the chosen connection config; Parameters
// opens an editable host/port panel.
//
// Runs its own GL resources + event loop, so it fully owns the window until it
