#pragma once

#include "game/game_session.hpp"
#include "renderer/world_renderer.hpp"

#include <string>

namespace scsr
{

class GameHud
{
public:
  static void draw(GameSession &session, Camera &camera, int width, int height);

private:
  static void drawCameraPanel(Camera &camera);
  static void drawFlightPanel(GameSession &session, int width, int height);
  [[nodiscard]] static std::string currentWeaponName(const GameSession &session);

  static int orbitLevel_;
};

} // namespace scsr
