#pragma once
// TCP client that speaks the Zappy GRAPHIC protocol to the server.
//
// Usage:
//   GuiClient c;
//   if (!c.connect("127.0.0.1", 4242, err)) { ...show err... }
//   // each frame:
//   c.poll();                 // non-blocking: drains socket, updates state()
//   const GuiState& s = c.state();
//
// connect() blocks briefly to establish the TCP session and perform the
// WELCOME/GRAPHIC handshake, then switches the socket to non-blocking so poll()
// never stalls the render loop. Partial lines are buffered between polls.

