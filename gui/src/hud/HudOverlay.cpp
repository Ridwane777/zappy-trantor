#include "hud/HudOverlay.hpp"

glm::vec3 HudOverlay::teamColor3(int i)
{
    static const glm::vec3 pal[] = {
        {0.20f, 0.80f, 1.00f}, {1.00f, 0.55f, 0.15f}, {0.45f, 1.00f, 0.40f},
        {1.00f, 0.35f, 0.75f}, {1.00f, 0.85f, 0.20f}, {0.65f, 0.45f, 1.00f},
    };
    return pal[((i % 6) + 6) % 6];
}

sf::Color HudOverlay::teamColorU(int i)
{
    glm::vec3 c = teamColor3(i);
    return sf::Color((sf::Uint8)(c.r * 255), (sf::Uint8)(c.g * 255), (sf::Uint8)(c.b * 255));
}

bool HudOverlay::handleClick(sf::Vector2i mouse)
{
    for (const auto &h : _hits) {
        if (!h.rect.contains((float)mouse.x, (float)mouse.y)) continue;
        if (h.kind == 2)                                  // toggle tab
            _open = !_open;
        else if (h.kind == 0) {                           // team header
            _selTeam = (_selTeam == h.team) ? std::string() : h.team;
            _selPlayer = -1;
        } else if (h.kind == 1)                           // player row
            _selPlayer = (_selPlayer == h.pid) ? -1 : h.pid;
        return true;                                      // kind 3 (panel) also consumes
    }
    return false;
}

int HudOverlay::inventoryPollTarget(float dt)
{
    _invPollT -= dt;
    if (_selPlayer >= 0 && _invPollT <= 0.f) {
        _invPollT = 0.33f;
        return _selPlayer;
    }
    return -1;
}

bool HudOverlay::handleScroll(sf::Vector2i mouse, float wheelDelta)
{
    if (_anim < 0.5f || (float)mouse.x < _panelX)   // only when over the open panel
        return false;
    _scroll = std::min(std::max(_scroll - wheelDelta * 34.f, 0.f), _maxScroll);
    return true;
}

void HudOverlay::draw(sf::RenderWindow &win, const GuiState *state,
                      const std::string &host, int port, int mapW, int mapH)
{
    sf::Vector2u ws = win.getSize();
    const sf::Font &font = *_font;

    sf::Text hint;
    hint.setFont(font);
    hint.setString("Drag: orbit   Scroll: zoom   R: reset");
    hint.setCharacterSize(18);
    hint.setFillColor(sf::Color(220, 220, 220, 160));
    hint.setPosition(14.f, 14.f);
    win.draw(hint);

    sf::Text info;
    info.setFont(font);
    std::string status;
    if (state) {
        status = "ONLINE " + host + ":" + std::to_string(port) +
                 "   grid " + std::to_string(state->width) + "x" + std::to_string(state->height) +
                 "   t/u " + std::to_string(state->timeUnit) +
                 "   teams " + std::to_string(state->teams.size()) +
                 "   players " + std::to_string(state->players.size()) +
                 "   eggs " + std::to_string(state->eggs.size());
        info.setFillColor(sf::Color(150, 220, 255, 200));
    } else {
        status = "OFFLINE (viewer)   map.json " +
                 std::to_string(mapW) + "x" + std::to_string(mapH);
        info.setFillColor(sf::Color(230, 180, 140, 190));
    }
    info.setString(status);
    info.setCharacterSize(18);
    sf::FloatRect b = info.getLocalBounds();
    info.setPosition(ws.x - b.width - 14.f, 14.f);
    win.draw(info);

    // ── retractable sidebar (teams / players / levels) ──────────────────────
    _hits.clear();
    const float pad = 14.f;
    float panelX = ws.x - WIDTH * _anim;
    float tabW = 26.f, tabH = 60.f, tabX = panelX - tabW, tabY = 96.f;
    sf::RectangleShape tab(sf::Vector2f(tabW, tabH));
    tab.setPosition(tabX, tabY);
    tab.setFillColor(sf::Color(20, 26, 38, 210));
    tab.setOutlineColor(sf::Color(90, 130, 180, 200));
    tab.setOutlineThickness(1.f);
    win.draw(tab);
    sf::Text arrow; arrow.setFont(font);
    arrow.setString(_open ? ">" : "<");
    arrow.setCharacterSize(20); arrow.setFillColor(sf::Color(180, 210, 255));
    arrow.setPosition(tabX + 8.f, tabY + 16.f); win.draw(arrow);
    _hits.push_back({sf::FloatRect(tabX, tabY, tabW, tabH), 2, "", -1});

    if (_anim > 0.01f && state) {
        _panelX = panelX;
        sf::RectangleShape panel(sf::Vector2f(WIDTH, (float)ws.y));
        panel.setPosition(panelX, 0.f);
        panel.setFillColor(sf::Color(14, 18, 28, 235));
        win.draw(panel);

        float x = panelX + pad, y = 46.f;

        // events: countdown to the next resource reset (fixed header)
        {
            sf::Text eh; eh.setFont(font); eh.setString("EVENTS");
            eh.setCharacterSize(18); eh.setFillColor(sf::Color(185, 205, 240));
            eh.setPosition(x, y); win.draw(eh); y += 26.f;
            float sr = state->secondsToReset;
            std::string ts;
            if (sr < 0.f) {
                ts = "--:--";
            } else {
                int s = (int)sr, mm = s / 60, ss = s % 60;
                ts = std::to_string(mm) + ":" + (ss < 10 ? "0" : "") + std::to_string(ss);
            }
            sf::RectangleShape er(sf::Vector2f(WIDTH - 2 * pad, 24.f));
            er.setPosition(x, y);
            bool soon = (sr >= 0.f && sr <= 12.f);
            er.setFillColor(soon ? sf::Color(255, 180, 90, 70) : sf::Color(255, 255, 255, 12));
            win.draw(er);
            sf::Text et; et.setFont(font); et.setString("Resource reset");
            et.setCharacterSize(14); et.setFillColor(sf::Color(150, 225, 170));
            et.setPosition(x + 6.f, y + 3.f); win.draw(et);
            sf::Text ev; ev.setFont(font); ev.setString(ts);
            ev.setCharacterSize(15); ev.setFillColor(sf::Color(240, 240, 248));
            sf::FloatRect vb = ev.getLocalBounds();
            ev.setPosition(x + (WIDTH - 2 * pad) - vb.width - 8.f, y + 2.f);
            win.draw(ev);
            y += 38.f;
        }

        sf::Text title; title.setFont(font); title.setString("TEAMS");
        title.setCharacterSize(20); title.setFillColor(sf::Color(200, 220, 255));
        title.setPosition(x, y); win.draw(title); y += 34.f;

