#pragma once

#include "app/GameClock.hpp"
#include "game/IWorldObserver.hpp"
#include "game/World.hpp"
#include "net/IConnectionHandler.hpp"
#include <string>
#include <unordered_map>

namespace zappy::app {

// Application layer: turns connections into players or GUI viewers and
// translates between the network (IConnectionHandler / IClientSink) and the
// game (World / IWorldObserver). Neither side knows the other.
class GameSession : public net::IConnectionHandler, public IWorldObserver {
public:
    GameSession(net::IClientSink &clients, int freq);

    void attach(World &world) { _world = &world; }

    // Runs every time unit elapsed since the last call.
    void advanceClock();
    int msUntilNextTick() const { return _clock.msUntilNextTick(); }

    // net::IConnectionHandler
    void onConnect(net::ConnectionId id) override;
    void onLine(net::ConnectionId id, const std::string &line) override;
    void onDisconnect(net::ConnectionId id) override;

    // IWorldObserver
    void onReply(int playerId, const std::string &line) override;
    void onPlayerDied(const Player &p) override;
    void onPlayerJoined(const Player &p) override;
    void onPlayerMoved(const Player &p) override;
    void onPlayerLevel(const Player &p) override;
    void onPlayerInventory(const Player &p) override;
    void onPlayerLeft(const Player &p) override;
    void onResourceTaken(const Player &p, Resource r) override;
    void onResourceDropped(const Player &p, Resource r) override;
    void onTileChanged(Position pos) override;
    void onPlayerExpelled(const Player &p) override;
    void onBroadcast(const Player &p, const std::string &message) override;
    void onIncantationStart(Position pos, int level, const std::vector<int> &playerIds) override;
    void onIncantationEnd(Position pos, bool success) override;
    void onEggLaid(const Player &p, const Egg &egg) override;
    void onEggHatched(const Egg &egg) override;
    void onEggDestroyed(const Egg &egg) override;
    void onGameWon(const std::string &team) override;

private:
    enum class Role { Pending, Ai, Gui };
    struct ClientInfo {
        Role role = Role::Pending;
        int playerId = 0;
    };

    net::IClientSink &_clients;
    World *_world = nullptr;
    GameClock _clock;
    std::unordered_map<net::ConnectionId, ClientInfo> _info;
    std::unordered_map<int, net::ConnectionId> _connectionOf;   // player id -> connection

    void handshake(net::ConnectionId id, ClientInfo &info, const std::string &line);
    void guiRequest(net::ConnectionId id, const std::string &line);
    void sendGuiBootstrap(net::ConnectionId id);
    void toGui(const std::string &text);
    void toPlayer(int playerId, const std::string &text);
};

} // namespace zappy::app
