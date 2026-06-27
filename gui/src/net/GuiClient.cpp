#include "net/GuiClient.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <sstream>

// Parse a player/egg id token, tolerating an optional leading '#'.
static int parseId(const std::string &tok)
{
    if (!tok.empty() && tok[0] == '#')
        return std::atoi(tok.c_str() + 1);
    return std::atoi(tok.c_str());
}

static const char *resName(int i)
{
    static const char *N[] = {"food", "linemate", "deraumere", "sibur",
                              "mendiane", "phiras", "thystame"};
    return (i >= 0 && i < 7) ? N[i] : "?";
}

void GuiClient::_logEvent(int id, const std::string &action, int kind)
{
    std::string team;
    auto it = _state.players.find(id);
    if (it != _state.players.end())
        team = it->second.team;
    _state.log.push_back({id, team, action, kind});
    _state.logTotal++;
    if (_state.log.size() > 100)
        _state.log.pop_front();
}

GuiClient::~GuiClient()
{
