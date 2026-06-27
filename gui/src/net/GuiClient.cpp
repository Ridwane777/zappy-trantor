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

