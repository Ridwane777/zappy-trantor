#include "net/GuiClient.hpp"
#include "net/GuiProtocolParser.hpp"

#include <algorithm>
#include <iostream>

namespace {

constexpr int CONNECT_TIMEOUT_MS = 5000;
constexpr std::size_t MAX_LINES_PER_FRAME = 3000;
constexpr std::size_t MAX_FEED = 200;

// Tile and position updates arrive in bulk; they are left out of the visible feed.
bool isHighFrequency(const std::string &line)
{
    return line.rfind("bct", 0) == 0 || line.rfind("ppo", 0) == 0;
}

} // namespace

bool GuiClient::connect(const std::string &host, int port, std::string &err)
{
    _state = GuiState{};
    if (!_conn.open(host, port, CONNECT_TIMEOUT_MS, err))
        return false;
    std::string welcome;
    if (!_conn.readLineBlocking(welcome, CONNECT_TIMEOUT_MS, err))
        return false;
    if (welcome.rfind("WELCOME", 0) != 0) {
        err = "unexpected greeting: '" + welcome + "'";
        _conn.close();
        return false;
    }
    record(_state.inboundFeed, welcome);
    send("GRAPHIC");
    return true;
}

void GuiClient::disconnect()
{
    _conn.close();
}

void GuiClient::poll()
{
    if (!_conn.pump()) {
        if (!_conn.lastError().empty())
            std::cerr << "[net] " << _conn.lastError() << "\n";
        return;
    }
    for (const auto &line : _conn.takeLines(MAX_LINES_PER_FRAME)) {
        if (line.empty())
            continue;
        if (!isHighFrequency(line))
            record(_state.inboundFeed, line);
        if (!GuiProtocolParser::apply(line, _state))
            std::cerr << "[net] unknown message ignored: " << line << "\n";
    }
}

void GuiClient::tick(float dt)
{
    if (_state.secondsToReset > 0.f)
        _state.secondsToReset = std::max(0.f, _state.secondsToReset - dt);
}

bool GuiClient::consumeResourcesDirty()
{
    bool dirty = _state.resDirty;
    _state.resDirty = false;
    return dirty;
}

void GuiClient::requestInventory(int playerId)
{
    send("pin #" + std::to_string(playerId));
}

void GuiClient::requestTimeUnit(int timeUnit)
{
    send("sst " + std::to_string(timeUnit));
}

void GuiClient::send(const std::string &line)
{
    _conn.sendLine(line);
    record(_state.outboundFeed, line);
}

void GuiClient::record(std::deque<std::string> &feed, const std::string &line)
{
    feed.push_back(line);
    if (feed.size() > MAX_FEED)
        feed.pop_front();
}
