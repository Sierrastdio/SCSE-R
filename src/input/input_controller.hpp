#pragma once

#include "game/game_session.hpp"
#include "renderer/world_renderer.hpp"

#include <SDL.h>

namespace scsr
{

class InputController
{
public:
  void handleEvent(const SDL_Event &event, SDL_Window *window, Camera &camera, GameSession &session);
  void drawOverlay() const;

private:
  void finishShipSelection(SDL_Window *window, Camera &camera, GameSession &session);
  static void handleKey(SDL_Keycode key, Camera &camera, GameSession &session);

  bool selectingShips_{};
  bool additiveSelection_{};
  int selectionStartX_{};
  int selectionStartY_{};
  int selectionCurrentX_{};
  int selectionCurrentY_{};
};

} // namespace scsr
