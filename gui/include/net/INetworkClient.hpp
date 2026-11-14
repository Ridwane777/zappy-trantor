#pragma once

#include "net/GuiState.hpp"
#include <string>

// What the rest of the GUI may do with the server. Rendering and input code
// depend on this interface only, so the network can be replaced (a test double,
// a replay of a recorded game...) without touching them.
class INetworkClient {
public:
    virtual ~INetworkClient() = default;

    virtual bool connect(const std::string &host, int port, std::string &err) = 0;
    virtual void disconnect() = 0;
    virtual bool connected() const = 0;

    // Exchanges data with the server without blocking; call once per frame.
    virtual void poll() = 0;
    // Advances local timers (resource reset countdown) by `dt` seconds.
    virtual void tick(float dt) = 0;

    // Read-only view of the world as last described by the server.
    virtual const GuiState &state() const = 0;
    // True once after tile contents changed (the caller rebuilds its resource meshes).
    virtual bool consumeResourcesDirty() = 0;

    virtual void requestInventory(int playerId) = 0;
    virtual void requestTimeUnit(int timeUnit) = 0;
};
