#define SDL_MAIN_HANDLED

#include "engine/actions.hpp"
#include "engine/engine.hpp"
#include "engine/world.hpp"
#include "world_renderer.hpp"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

class GameApp
{
public:
  ~GameApp() { shutdown(); }

  void initialize()
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

    const std::unique_ptr<char, decltype(&SDL_free)> executablePath(SDL_GetBasePath(), &SDL_free);
    if (!executablePath)
      throw std::runtime_error(std::string("Could not locate the executable directory: ") + SDL_GetError());
    const std::filesystem::path rulesPath = std::filesystem::path(executablePath.get()) / "scripts" / "game_rules.lua";
    simulation_ = std::make_unique<spasim::Simulation>(spasim::makeInitialWorld(), rulesPath);
    const auto playerId = spasim::joinTeam(simulation_->world(), 0, "Pilot");
    if (!playerId)
      throw std::runtime_error("Could not join the starter team; check the SCSE scenario's player limits.");
    playerId_ = *playerId;
    const auto *player = simulation_->world().findPlayer(playerId_);
    if (!player)
      throw std::runtime_error("The SCSE engine did not retain the newly joined player.");
    selection_ = spasim::Target{spasim::Target::Kind::Entity, player->shipId};
    setStatus("FLIGHT READY — select a contact or planet to begin.", true);
  }

  int run()
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
      advanceSimulation(frameSeconds);
      updateTrails();
      render();
    }
    return 0;
  }

private:
  static void configureStyle()
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

  void shutdown()
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

  spasim::World &world() { return simulation_->world(); }

  const spasim::World &world() const { return simulation_->world(); }

  const spasim::Ship *playerShip() const
  {
    const auto *player = simulation_->world().findPlayer(playerId_);
    return player ? simulation_->world().findShip(player->shipId) : nullptr;
  }

  void setStatus(std::string text, bool succeeded)
  {
    status_ = std::move(text);
    statusSucceeded_ = succeeded;
  }

  template <typename Action>
  bool execute(const Action &action)
  {
    const auto result = simulation_->execute(playerId_, spasim::WorldCommand{action});
    setStatus(result.message, static_cast<bool>(result));
    return static_cast<bool>(result);
  }

  void aimAtSelection()
  {
    if (!selection_ || selection_->kind != spasim::Target::Kind::Entity)
    {
      setStatus("Select a ship, base, or torpedo before targeting.", false);
      return;
    }

    const auto *ship = playerShip();
    if (!ship || selection_->id == ship->id)
    {
      setStatus("Choose another contact as your target.", false);
      return;
    }

    if (!execute(spasim::SelectTarget{*selection_}))
      return;
    const auto *targetShip = world().findShip(selection_->id);
    const auto *targetBase = world().findBase(selection_->id);
    const auto *targetTorpedo = world().findTorpedo(selection_->id);
    const spasim::Vec3 *position = targetShip      ? &targetShip->position
                                   : targetBase    ? &targetBase->position
                                   : targetTorpedo ? &targetTorpedo->position
                                                   : nullptr;
    if (position)
      execute(spasim::SetHeading{*position - ship->position});
  }

  void firePhasor() { execute(spasim::FirePhasor{1.0}); }

  void launchTorpedo()
  {
    const auto *ship = playerShip();
    if (!ship)
      return;
    const double yaw = ship->orientation.yawDegrees * std::acos(-1.0) / 180.0;
    const double pitch = ship->orientation.pitchDegrees * std::acos(-1.0) / 180.0;
    const spasim::Vec3 forward{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
    execute(spasim::FireTorpedo{100.0, forward});
  }

  void adjustHeading(double yawDegrees, double pitchDegrees)
  {
    const auto *ship = playerShip();
    if (!ship)
      return;
    const double yaw = (ship->orientation.yawDegrees + yawDegrees) * std::acos(-1.0) / 180.0;
    const double pitch = std::clamp(ship->orientation.pitchDegrees + pitchDegrees, -89.0, 89.0) * std::acos(-1.0) / 180.0;
    execute(spasim::SetHeading{{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)}});
  }

  void advanceSimulation(double frameSeconds)
  {
    if (!simulationRunning_)
      return;
    accumulator_ += frameSeconds * simulationSpeed_;
    const double step = world().fixedStepSeconds;
    if (step <= 0.0)
      throw std::runtime_error("SCSE reported an invalid fixed-step duration.");

    int stepCount = 0;
    while (accumulator_ >= step && stepCount < 100)
    {
      simulation_->step();
      accumulator_ -= step;
      ++stepCount;
    }
    if (stepCount == 100)
      accumulator_ = std::fmod(accumulator_, step);
  }

  void updateTrails()
  {
    for (const auto &ship : world().ships)
    {
      if (!ship || !ship->exists)
        continue;
      auto &trail = trails_[ship->id];
      trail.push_back(ship->position);
      if (trail.size() > 180)
        trail.pop_front();
    }
  }

  void handleEvent(const SDL_Event &event)
  {
    ImGui_ImplSDL2_ProcessEvent(&event);
    ImGuiIO &io = ImGui::GetIO();
    if (event.type == SDL_QUIT)
      running_ = false;
    else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT && !io.WantCaptureMouse)
    {
      camera_.dragging = true;
      camera_.dragX = event.button.x;
      camera_.dragY = event.button.y;
    }
    else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)
      camera_.dragging = false;
    else if (event.type == SDL_MOUSEMOTION && camera_.dragging)
    {
      const int deltaX = event.motion.x - camera_.dragX;
      const int deltaY = event.motion.y - camera_.dragY;
      if ((SDL_GetModState() & KMOD_SHIFT) != 0)
      {
        camera_.panX += static_cast<float>(deltaX);
        camera_.panY += static_cast<float>(deltaY);
      }
      else if (camera_.plane == scsr::ViewPlane::Orbit)
      {
        camera_.azimuth = std::fmod(camera_.azimuth + deltaX * 0.5f + 360.0f, 360.0f);
        camera_.elevation = std::clamp(camera_.elevation - deltaY * 0.5f, -89.0f, 89.0f);
      }
      camera_.dragX = event.motion.x;
      camera_.dragY = event.motion.y;
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && !io.WantCaptureMouse)
    {
      int width{};
      int height{};
      SDL_GetWindowSize(window_, &width, &height);
      selection_ =
        scsr::pickObject(world(), scsr::makeProjection(world(), camera_, static_cast<float>(width), static_cast<float>(height)),
          ImVec2(static_cast<float>(event.button.x), static_cast<float>(event.button.y)));
    }
    else if (event.type == SDL_MOUSEWHEEL && !io.WantCaptureMouse)
      camera_.zoom = std::clamp(camera_.zoom * std::pow(1.1f, static_cast<float>(event.wheel.y)), 0.15f, 8.0f);
    else if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && !io.WantCaptureKeyboard)
      handleKey(event.key.keysym.sym);
  }

  void handleKey(SDL_Keycode key)
  {
    switch (key)
    {
      case SDLK_ESCAPE: running_ = false; break;
      case SDLK_1: camera_.plane = scsr::ViewPlane::Orbit; break;
      case SDLK_2: camera_.plane = scsr::ViewPlane::XY; break;
      case SDLK_3: camera_.plane = scsr::ViewPlane::XZ; break;
      case SDLK_4: camera_.plane = scsr::ViewPlane::YZ; break;
      case SDLK_EQUALS:
      case SDLK_PLUS:
      case SDLK_KP_PLUS: camera_.zoom = std::min(camera_.zoom * 1.1f, 8.0f); break;
      case SDLK_MINUS:
      case SDLK_KP_MINUS: camera_.zoom = std::max(camera_.zoom / 1.1f, 0.15f); break;
      case SDLK_LEFT: adjustHeading(-5.0, 0.0); break;
      case SDLK_RIGHT: adjustHeading(5.0, 0.0); break;
      case SDLK_UP: adjustHeading(0.0, 5.0); break;
      case SDLK_DOWN: adjustHeading(0.0, -5.0); break;
      case SDLK_w: execute(spasim::SetAcceleration{spasim::AccelerationMode::Accelerate}); break;
      case SDLK_s: execute(spasim::SetAcceleration{spasim::AccelerationMode::Decelerate}); break;
      case SDLK_x: execute(spasim::SetAcceleration{spasim::AccelerationMode::Cruise}); break;
      case SDLK_SPACE: firePhasor(); break;
      case SDLK_t: launchTorpedo(); break;
      case SDLK_d: execute(spasim::DetonateTorpedo{}); break;
      default: break;
    }
  }

  void render()
  {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    int width{};
    int height{};
    SDL_GetWindowSize(window_, &width, &height);
    scsr::drawWorld(*ImGui::GetBackgroundDrawList(), world(), camera_, selection_, trails_, static_cast<float>(width),
      static_cast<float>(height));
    drawCameraPanel();
    drawFlightPanel(width, height);
    drawSelectionPanel(height);
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

  void drawCameraPanel()
  {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(365.0f, 125.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    constexpr ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
    ImGui::Begin("SCSR HUD", nullptr, flags);
    ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "SCSR  /  SPACE COMBAT");
    ImGui::SameLine();
    ImGui::TextDisabled("BUILD 0.1");
    ImGui::Separator();
    ImGui::Text("T+ %07.1f s   |   TICK %06llu", world().elapsedSeconds, static_cast<unsigned long long>(world().tick));
    ImGui::Text("%zu CONTACTS   |   %zu PLANETS   |   TEAM %u", activeContactCount(), world().planets.size(), playerTeam() + 1);
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(16.0f, 153.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(365.0f, 58.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);
    ImGui::Begin("View selector", nullptr, flags);
    const auto viewButton = [this](const char *label, scsr::ViewPlane plane) {
      const bool active = camera_.plane == plane;
      if (active)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.40f, 0.57f, 1.0f));
      if (ImGui::Button(label, ImVec2(76.0f, 28.0f)))
        camera_.plane = plane;
      if (active)
        ImGui::PopStyleColor();
    };
    viewButton("ORBIT [1]", scsr::ViewPlane::Orbit);
    ImGui::SameLine();
    viewButton("XY [2]", scsr::ViewPlane::XY);
    ImGui::SameLine();
    viewButton("XZ [3]", scsr::ViewPlane::XZ);
    ImGui::SameLine();
    viewButton("YZ [4]", scsr::ViewPlane::YZ);
    ImGui::End();
  }

  void drawFlightPanel(int width, int height)
  {
    const float panelWidth = std::min(330.0f, static_cast<float>(width) * 0.38f);
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) - panelWidth - 16.0f, 16.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, std::max(350.0f, static_cast<float>(height) - 32.0f)), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);
    constexpr ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
    ImGui::Begin("Flight systems", nullptr, flags);

    ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "FLIGHT SYSTEMS");
    ImGui::Separator();
    if (ImGui::Button(simulationRunning_ ? "PAUSE SIMULATION" : "RESUME SIMULATION", ImVec2(-1.0f, 32.0f)))
      simulationRunning_ = !simulationRunning_;
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("Simulation rate", &simulationSpeed_, 0.25f, 5.0f, "%.2fx");

    const auto *ship = playerShip();
    if (ship)
    {
      ImGui::SeparatorText("PILOT VESSEL");
      ImGui::Text("%s  /  ID %u", ship->name.c_str(), ship->id);
      ImGui::Text("HULL  %.0f / %.0f", ship->combat.hull - ship->combat.hullDamage, ship->combat.hull);
      ImGui::Text("SHIELDS  %.0f    ENERGY  %.0f", ship->combat.shields, ship->resources.energy);
      ImGui::Text("SPEED  %.2f    HEADING  %+.0f / %+.0f", ship->speed, ship->orientation.yawDegrees, ship->orientation.pitchDegrees);
      ImGui::Text("MODE  %s    THROTTLE  %u", spasim::toString(ship->acceleration), ship->throttleLevel);

      ImGui::SeparatorText("FLIGHT");
      if (ImGui::Button("THRUST  [W]", ImVec2((panelWidth - 42.0f) * 0.5f, 30.0f)))
        execute(spasim::SetAcceleration{spasim::AccelerationMode::Accelerate});
      ImGui::SameLine();
      if (ImGui::Button("BRAKE  [S]", ImVec2((panelWidth - 42.0f) * 0.5f, 30.0f)))
        execute(spasim::SetAcceleration{spasim::AccelerationMode::Decelerate});
      if (ImGui::Button("COAST  [X]", ImVec2(-1.0f, 28.0f)))
        execute(spasim::SetAcceleration{spasim::AccelerationMode::Cruise});

      ImGui::TextDisabled("Throttle stage");
      for (int level = 1; level <= 4; ++level)
      {
        if (level > 1)
          ImGui::SameLine();
        if (ImGui::Button(std::to_string(level).c_str(), ImVec2(42.0f, 27.0f)))
          execute(spasim::SetThrottleLevel{static_cast<std::uint8_t>(level)});
      }

      ImGui::TextDisabled("Heading controls");
      if (ImGui::Button("YAW -", ImVec2((panelWidth - 42.0f) * 0.5f, 27.0f)))
        adjustHeading(-15.0, 0.0);
      ImGui::SameLine();
      if (ImGui::Button("YAW +", ImVec2((panelWidth - 42.0f) * 0.5f, 27.0f)))
        adjustHeading(15.0, 0.0);
      if (ImGui::Button("PITCH +", ImVec2((panelWidth - 42.0f) * 0.5f, 27.0f)))
        adjustHeading(0.0, 10.0);
      ImGui::SameLine();
      if (ImGui::Button("PITCH -", ImVec2((panelWidth - 42.0f) * 0.5f, 27.0f)))
        adjustHeading(0.0, -10.0);
    }

    ImGui::SeparatorText("TARGETING");
    if (ImGui::Button("SET TARGET / AIM  [R]", ImVec2(-1.0f, 30.0f)))
      aimAtSelection();
    if (const auto *player = world().findPlayer(playerId_); player && player->selectedTarget)
      ImGui::Text("Locked contact: #%u", *player->selectedTarget);
    else
      ImGui::TextDisabled("No weapon target locked");

    if (ImGui::BeginCombo("Weapon", currentWeaponName().c_str()))
    {
      for (std::size_t index = 0; index < world().weapons.size(); ++index)
      {
        const auto *player = world().findPlayer(playerId_);
        const bool selected = player && player->weapon == index;
        if (ImGui::Selectable(world().weapons[index].name.c_str(), selected))
          execute(spasim::SetWeaponProfile{static_cast<spasim::WeaponId>(index)});
        if (selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    if (ImGui::Button("FIRE PHASOR  [SPACE]", ImVec2(-1.0f, 30.0f)))
      firePhasor();
    if (ImGui::Button("LAUNCH TORPEDO  [T]", ImVec2(-1.0f, 30.0f)))
      launchTorpedo();
    if (ImGui::Button("DETONATE TORPEDO  [D]", ImVec2(-1.0f, 28.0f)))
      execute(spasim::DetonateTorpedo{});

    ImGui::SeparatorText("NAVIGATION");
    if (selection_ && selection_->kind == spasim::Target::Kind::Planet)
    {
      const auto &planet = world().planets[selection_->id];
      ImGui::Text("Course destination: %s", planet.name.c_str());
      const int maxOrbit = std::max(1, static_cast<int>(planet.orbitLevelCount));
      orbitLevel_ = std::clamp(orbitLevel_, 1, maxOrbit);
      ImGui::SetNextItemWidth(-1.0f);
      ImGui::SliderInt("Orbit level", &orbitLevel_, 1, maxOrbit);
      if (ImGui::Button("SET COURSE TO PLANET", ImVec2(-1.0f, 30.0f)))
        execute(spasim::SetGoal{*selection_, static_cast<std::uint8_t>(orbitLevel_)});
    }
    else if (ship && ship->orbitingPlanet)
    {
      const auto &planet = world().planets[*ship->orbitingPlanet];
      ImGui::Text("Orbiting %s  /  LEVEL %u", planet.name.c_str(), ship->orbitLevel);
      const int maxOrbit = std::max(1, static_cast<int>(planet.orbitLevelCount));
      orbitLevel_ = std::clamp(orbitLevel_, 1, maxOrbit);
      ImGui::SetNextItemWidth(-1.0f);
      ImGui::SliderInt("Transfer level", &orbitLevel_, 1, maxOrbit);
      if (ImGui::Button("TRANSFER ORBIT", ImVec2(-1.0f, 30.0f)))
        execute(spasim::SetOrbitLevel{static_cast<std::uint8_t>(orbitLevel_)});
    }
    else
      ImGui::TextDisabled("Select a planet to set a course.");

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, statusSucceeded_ ? ImVec4(0.50f, 0.88f, 0.71f, 1.0f) : ImVec4(1.0f, 0.48f, 0.44f, 1.0f));
    ImGui::TextWrapped("%s", status_.c_str());
    ImGui::PopStyleColor();
    ImGui::End();
  }

  void drawSelectionPanel(int height)
  {
    ImGui::SetNextWindowPos(ImVec2(16.0f, static_cast<float>(height) - 104.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(365.0f, 86.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);
    constexpr ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
    ImGui::Begin("Selected contact", nullptr, flags);
    ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "SELECTED CONTACT");
    if (!selection_)
      ImGui::TextDisabled("Click a ship, base, torpedo, or planet.");
    else if (selection_->kind == spasim::Target::Kind::Planet && selection_->id < world().planets.size())
    {
      const auto &planet = world().planets[selection_->id];
      ImGui::Text("%s  |  GRAVITY RADIUS %.0f", planet.name.c_str(), planet.sphereOfInfluenceRadius);
    }
    else
    {
      const auto *ship = world().findShip(selection_->id);
      const auto *base = world().findBase(selection_->id);
      const auto *torpedo = world().findTorpedo(selection_->id);
      if (ship)
        ImGui::Text("%s  |  SHIELDS %.0f  |  HULL %.0f", ship->name.c_str(), ship->combat.shields,
          ship->combat.hull - ship->combat.hullDamage);
      else if (base)
        ImGui::Text("BASE #%u  |  SHIELDS %.0f", base->id, base->combat.shields);
      else if (torpedo)
        ImGui::Text("TORPEDO #%u  |  ENERGY %.0f", torpedo->id, torpedo->energy);
      else
        ImGui::TextDisabled("Contact no longer available.");
    }
    ImGui::End();
  }

  std::string currentWeaponName() const
  {
    const auto *player = world().findPlayer(playerId_);
    if (!player || player->weapon >= world().weapons.size())
      return "No weapon";
    return world().weapons[player->weapon].name;
  }

  spasim::TeamId playerTeam() const
  {
    const auto *player = world().findPlayer(playerId_);
    return player ? player->team : 0;
  }

  std::size_t activeContactCount() const
  {
    std::size_t count = 0;
    for (const auto &ship : world().ships)
      count += ship && ship->exists;
    for (const auto &base : world().bases)
      count += base && base->exists;
    for (const auto &torpedo : world().torpedoes)
      count += torpedo && torpedo->exists;
    return count;
  }

  bool running_{true};
  bool simulationRunning_{true};
  bool statusSucceeded_{true};
  bool sdlInitialized_{};
  bool imguiCreated_{};
  bool imguiSdlInitialized_{};
  bool imguiGlInitialized_{};
  float simulationSpeed_{1.0f};
  double accumulator_{};
  int orbitLevel_{1};
  SDL_Window *window_{};
  SDL_GLContext glContext_{};
  spasim::PlayerId playerId_{};
  std::unique_ptr<spasim::Simulation> simulation_;
  std::optional<spasim::Target> selection_;
  scsr::Camera camera_;
  scsr::ShipTrails trails_;
  std::string status_;
};

} // namespace

int main()
{
  try
  {
    GameApp app;
    app.initialize();
    return app.run();
  }
  catch (const std::exception &error)
  {
    std::fprintf(stderr, "SCSR startup/runtime error: %s\n", error.what());
    return 1;
  }
}
