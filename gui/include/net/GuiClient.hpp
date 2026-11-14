#pragma once

#include "net/INetworkClient.hpp"
#include "net/TcpConnection.hpp"

// INetworkClient over TCP: a TcpConnection for the bytes, GuiProtocolParser for
// their meaning, and the GuiState they build.
class GuiClient : public INetworkClient {
public:
    bool connect(const std::string &host, int port, std::string &err) override;
    void disconnect() override;
    bool connected() const override { return _conn.isOpen(); }

    void poll() override;
    void tick(float dt) override;

    const GuiState &state() const override { return _state; }
    bool consumeResourcesDirty() override;

    void requestInventory(int playerId) override;
    void requestTimeUnit(int timeUnit) override;

private:
    TcpConnection _conn;
    GuiState _state;

    void send(const std::string &line);
    void record(std::deque<std::string> &feed, const std::string &line);
};
