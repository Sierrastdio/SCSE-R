#pragma once

#include "game/game_session.hpp"
#include "renderer/world_renderer.hpp"

namespace scsr
{

class GameHud
{
public:
  static void draw(GameSession &session, Camera &camera, int width, int height);

private:
  static void drawCameraPanel(const GameSession &session, Camera &camera);
  static void drawFlightPanel(GameSession &session, int width, int height);
  static void drawSelectionPanel(const GameSession &session, int height);
  [[nodiscard]] static const char *currentWeaponName(const GameSession &session);

  static int orbitLevel_;
};

} // namespace scsr
