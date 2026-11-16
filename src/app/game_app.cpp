#include "game_app.hpp"

#include "ui/game_hud.hpp"

#include <SDL_opengl.h>
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace scsr
{

GameApp::~GameApp() { shutdown(); }

void GameApp::initialize()
{
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    throw std::runtime_error(std::string("SDL initialization failed: ") + SDL_GetError());
  sdlInitialized_ = true;

  const auto setGlAttribute = [](SDL_GLattr attribute, int value) {
    if (SDL_GL_SetAttribute(attribute, value) != 0)
      throw std::runtime_error(std::string("OpenGL setup failed: ") + SDL_GetError());
  };
  setGlAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  setGlAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  setGlAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  setGlAttribute(SDL_GL_DOUBLEBUFFER, 1);
  setGlAttribute(SDL_GL_DEPTH_SIZE, 0);

  window_ = SDL_CreateWindow("SCSR | Space Combat", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1440, 900,
    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window_)
    throw std::runtime_error(std::string("Window creation failed: ") + SDL_GetError());
  SDL_SetWindowMinimumSize(window_, 1024, 640);

  glContext_ = SDL_GL_CreateContext(window_);
  if (!glContext_)
    throw std::runtime_error(std::string("OpenGL context creation failed: ") + SDL_GetError());
  if (SDL_GL_MakeCurrent(window_, glContext_) != 0)
    throw std::runtime_error(std::string("Could not activate the OpenGL context: ") + SDL_GetError());
  if (SDL_GL_SetSwapInterval(1) != 0)
    std::fprintf(stderr, "Warning: vertical synchronization is unavailable: %s\n", SDL_GetError());

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  imguiCreated_ = true;
  ImGui::GetIO().IniFilename = nullptr;
  configureStyle();
  if (!ImGui_ImplSDL2_InitForOpenGL(window_, glContext_))
    throw std::runtime_error("Dear ImGui SDL2 backend initialization failed.");
  imguiSdlInitialized_ = true;
  if (!ImGui_ImplOpenGL3_Init("#version 130"))
    throw std::runtime_error("Dear ImGui OpenGL backend initialization failed.");
  imguiGlInitialized_ = true;

  session_ = std::make_unique<GameSession>();
}

int GameApp::run()
{
  auto previousFrame = std::chrono::steady_clock::now();
  while (running_)
  {
    SDL_Event event;
    while (SDL_PollEvent(&event))
      handleEvent(event);

    const auto now = std::chrono::steady_clock::now();
    const double frameSeconds = std::min(std::chrono::duration<double>(now - previousFrame).count(), 0.25);
    previousFrame = now;
    session_->advance(frameSeconds);
    session_->updateTrails();
    render();
  }
  return 0;
}

void GameApp::configureStyle()
{
  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 7.0f;
  style.ChildRounding = 5.0f;
  style.FrameRounding = 4.0f;
  style.PopupRounding = 5.0f;
  style.ScrollbarRounding = 5.0f;
  style.WindowPadding = ImVec2(14.0f, 13.0f);
  style.FramePadding = ImVec2(9.0f, 6.0f);
  style.ItemSpacing = ImVec2(9.0f, 8.0f);
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.055f, 0.085f, 0.94f);
  style.Colors[ImGuiCol_FrameBg] = ImVec4(0.09f, 0.13f, 0.19f, 1.0f);
  style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.22f, 0.32f, 1.0f);
  style.Colors[ImGuiCol_Button] = ImVec4(0.08f, 0.25f, 0.38f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.10f, 0.39f, 0.58f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.08f, 0.50f, 0.68f, 1.0f);
  style.Colors[ImGuiCol_Header] = ImVec4(0.08f, 0.25f, 0.38f, 0.8f);
  style.Colors[ImGuiCol_CheckMark] = ImVec4(0.38f, 0.78f, 1.0f, 1.0f);
}

void GameApp::shutdown() noexcept
{
  if (imguiGlInitialized_)
    ImGui_ImplOpenGL3_Shutdown();
  if (imguiSdlInitialized_)
    ImGui_ImplSDL2_Shutdown();
  if (imguiCreated_)
    ImGui::DestroyContext();
  if (glContext_)
    SDL_GL_DeleteContext(glContext_);
  if (window_)
    SDL_DestroyWindow(window_);
  if (sdlInitialized_)
    SDL_Quit();
}

void GameApp::handleEvent(const SDL_Event &event)
{
  if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
  {
    running_ = false;
    return;
  }
  input_.handleEvent(event, window_, camera_, *session_);
}

void GameApp::render()
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  int width{};
  int height{};
  SDL_GetWindowSize(window_, &width, &height);
  drawWorld(*ImGui::GetBackgroundDrawList(), session_->world(), session_->playerId(), camera_, session_->selection(),
    session_->shipTrails(), static_cast<float>(width), static_cast<float>(height));
  GameHud::draw(*session_, camera_, width, height);
  input_.drawOverlay();
  ImGui::Render();

  int drawableWidth{};
  int drawableHeight{};
  SDL_GL_GetDrawableSize(window_, &drawableWidth, &drawableHeight);
  glViewport(0, 0, drawableWidth, drawableHeight);
  glClearColor(0.03f, 0.05f, 0.08f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(window_);
}

} // namespace scsr
