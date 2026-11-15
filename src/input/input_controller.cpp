#include "input_controller.hpp"

#include <backends/imgui_impl_sdl2.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace scsr
{

void InputController::handleEvent(const SDL_Event &event, SDL_Window *window, Camera &camera, GameSession &session)
{
  ImGui_ImplSDL2_ProcessEvent(&event);
  const ImGuiIO &io = ImGui::GetIO();

  if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT && !io.WantCaptureMouse)
  {
    selectingShips_ = true;
    additiveSelection_ = (SDL_GetModState() & KMOD_SHIFT) != 0;
    selectionStartX_ = event.button.x;
    selectionStartY_ = event.button.y;
    selectionCurrentX_ = event.button.x;
    selectionCurrentY_ = event.button.y;
  }
  else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT && selectingShips_)
  {
    selectionCurrentX_ = event.button.x;
    selectionCurrentY_ = event.button.y;
    finishShipSelection(window, camera, session);
    selectingShips_ = false;
  }
  else if (event.type == SDL_MOUSEMOTION && selectingShips_)
  {
    selectionCurrentX_ = event.motion.x;
    selectionCurrentY_ = event.motion.y;
  }
  else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT && !io.WantCaptureMouse)
  {
    camera.dragging = true;
    camera.dragX = event.button.x;
    camera.dragY = event.button.y;
  }
  else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT)
    camera.dragging = false;
  else if (event.type == SDL_MOUSEMOTION && camera.dragging)
  {
    const int deltaX = event.motion.x - camera.dragX;
    const int deltaY = event.motion.y - camera.dragY;
    if ((SDL_GetModState() & KMOD_SHIFT) != 0)
    {
      camera.panX += static_cast<float>(deltaX);
      camera.panY += static_cast<float>(deltaY);
    }
    else if (camera.plane == ViewPlane::Orbit)
    {
      camera.azimuth = std::fmod(camera.azimuth + deltaX * 0.5f + 360.0f, 360.0f);
      camera.elevation = std::clamp(camera.elevation - deltaY * 0.5f, -89.0f, 89.0f);
    }
    camera.dragX = event.motion.x;
    camera.dragY = event.motion.y;
  }
  else if (event.type == SDL_MOUSEWHEEL && !io.WantCaptureMouse)
    camera.zoom = std::clamp(camera.zoom * std::pow(1.1f, static_cast<float>(event.wheel.y)), 0.15f, 8.0f);
  else if (event.type == SDL_KEYDOWN && event.key.repeat == 0 && !io.WantCaptureKeyboard)
    handleKey(event.key.keysym.sym, camera, session);
}

void InputController::finishShipSelection(SDL_Window *window, Camera &camera, GameSession &session)
{
  int width{};
  int height{};
  SDL_GetWindowSize(window, &width, &height);
  const auto projection = makeProjection(session.world(), camera, static_cast<float>(width), static_cast<float>(height));
  const int left = std::min(selectionStartX_, selectionCurrentX_);
  const int right = std::max(selectionStartX_, selectionCurrentX_);
  const int top = std::min(selectionStartY_, selectionCurrentY_);
  const int bottom = std::max(selectionStartY_, selectionCurrentY_);

  if (right - left >= 5 || bottom - top >= 5)
  {
    std::vector<spasim::EntityId> shipIds;
    for (const auto &ship : session.world().ships)
    {
      if (!ship || !session.controlsShip(ship->id))
        continue;
      const ImVec2 screen = projection.project(ship->position);
      if (screen.x >= left && screen.x <= right && screen.y >= top && screen.y <= bottom)
        shipIds.push_back(ship->id);
    }

    if (additiveSelection_)
    {
      auto selected = session.selectedShipIds();
      for (const auto shipId : shipIds)
        if (std::ranges::find(selected, shipId) == selected.end())
          selected.push_back(shipId);
      shipIds = std::move(selected);
    }

    if (!shipIds.empty() && session.selectShips(std::move(shipIds)))
    {
      const auto selected = session.selectedShipIds();
      if (!selected.empty())
        session.setSelection(spasim::Target{spasim::Target::Kind::Entity, selected.front()});
    }
    else if (!additiveSelection_)
      session.setStatus("No active friendly ships inside the selection box.", false);
    return;
  }

  auto picked =
    pickObject(session.world(), projection, ImVec2(static_cast<float>(selectionCurrentX_), static_cast<float>(selectionCurrentY_)));
  session.setSelection(picked);
  if (!picked || picked->kind != spasim::Target::Kind::Entity || !session.controlsShip(picked->id))
    return;

  if (additiveSelection_)
    session.toggleShipSelection(picked->id);
  else
    session.selectShips({picked->id});
}

void InputController::drawOverlay() const
{
  if (!selectingShips_)
    return;

  const ImVec2 first(static_cast<float>(std::min(selectionStartX_, selectionCurrentX_)),
    static_cast<float>(std::min(selectionStartY_, selectionCurrentY_)));
  const ImVec2 second(static_cast<float>(std::max(selectionStartX_, selectionCurrentX_)),
    static_cast<float>(std::max(selectionStartY_, selectionCurrentY_)));
  ImDrawList *drawList = ImGui::GetForegroundDrawList();
  drawList->AddRectFilled(first, second, IM_COL32(69, 185, 255, 35));
  drawList->AddRect(first, second, IM_COL32(100, 206, 255, 220), 0.0f, 0, 1.5f);
}

void InputController::handleKey(SDL_Keycode key, Camera &camera, GameSession &session)
{
  switch (key)
  {
    case SDLK_1: camera.plane = ViewPlane::Orbit; break;
    case SDLK_2: camera.plane = ViewPlane::XY; break;
    case SDLK_3: camera.plane = ViewPlane::XZ; break;
    case SDLK_4: camera.plane = ViewPlane::YZ; break;
    case SDLK_EQUALS:
    case SDLK_PLUS:
    case SDLK_KP_PLUS: camera.zoom = std::min(camera.zoom * 1.1f, 8.0f); break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS: camera.zoom = std::max(camera.zoom / 1.1f, 0.15f); break;
    case SDLK_LEFT: session.adjustHeading(-5.0, 0.0); break;
    case SDLK_RIGHT: session.adjustHeading(5.0, 0.0); break;
    case SDLK_UP: session.adjustHeading(0.0, 5.0); break;
    case SDLK_DOWN: session.adjustHeading(0.0, -5.0); break;
    case SDLK_w: session.execute(spasim::SetAcceleration{spasim::AccelerationMode::Accelerate}); break;
    case SDLK_s: session.execute(spasim::SetAcceleration{spasim::AccelerationMode::Decelerate}); break;
    case SDLK_x: session.execute(spasim::SetAcceleration{spasim::AccelerationMode::Cruise}); break;
    case SDLK_r: session.aimAtSelection(); break;
    case SDLK_SPACE: session.firePhasor(); break;
    case SDLK_t: session.launchTorpedo(); break;
    case SDLK_d: session.execute(spasim::DetonateTorpedo{}); break;
    default: break;
  }
}

} // namespace scsr
