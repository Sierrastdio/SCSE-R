#pragma once

#include "game/game_session.hpp"
#include "input/input_controller.hpp"
#include "renderer/world_renderer.hpp"

#include <SDL.h>

#include <memory>

namespace scsr
{

class GameApp
{
public:
  GameApp() = default;
  ~GameApp();

  GameApp(const GameApp &) = delete;
  GameApp &operator=(const GameApp &) = delete;

  void initialize();
  int run();

private:
  static void configureStyle();
  void shutdown() noexcept;
  void handleEvent(const SDL_Event &event);
  void render();

  bool running_{true};
  bool sdlInitialized_{};
  bool imguiCreated_{};
  bool imguiSdlInitialized_{};
  bool imguiGlInitialized_{};
  SDL_Window *window_{};
  SDL_GLContext glContext_{};
  std::unique_ptr<GameSession> session_;
  InputController input_;
  Camera camera_;
};

} // namespace scsr
