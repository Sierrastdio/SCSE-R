#include "game_hud.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace scsr
{
namespace
{
constexpr ImVec4 kAccentColor{0.40f, 0.80f, 1.0f, 1.0f};
constexpr ImGuiWindowFlags kPanelFlags =
  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
} // namespace

int GameHud::orbitLevel_ = 1;

void GameHud::draw(GameSession &session, Camera &camera, int width, int height)
{
  drawCameraPanel(camera);
  drawFlightPanel(session, width, height);
}

void GameHud::drawCameraPanel(Camera &camera)
{
  ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(230.0f, 52.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.88f);
  ImGui::Begin("View selector", nullptr, kPanelFlags);
  const auto viewButton = [&camera](const char *label, ViewPlane plane) {
    const bool active = camera.plane == plane;

    if (active)
    {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.40f, 0.57f, 1.0f));
    }

    if (ImGui::Button(label, ImVec2(39.0f, 24.0f)))
    {
      camera.plane = plane;
    }

    if (active)
    {
      ImGui::PopStyleColor();
    }
  };

  viewButton("ORB", ViewPlane::Orbit);
  ImGui::SameLine();
  viewButton("XY", ViewPlane::XY);
  ImGui::SameLine();
  viewButton("XZ", ViewPlane::XZ);
  ImGui::SameLine();
  viewButton("YZ", ViewPlane::YZ);
  ImGui::End();
}

void GameHud::drawFlightPanel(GameSession &session, int width, int height)
{
  const float availableScreenWidth = std::max(0.0f, static_cast<float>(width) - 32.0f);
  const float panelWidth = availableScreenWidth;
  const float desiredPanelHeight = std::clamp(static_cast<float>(height) * 0.23f, 190.0f, 220.0f);
  const float panelHeight = std::max(0.0f, std::min(static_cast<float>(height) - 32.0f, desiredPanelHeight));
  const float panelY = std::max(16.0f, static_cast<float>(height) - panelHeight - 16.0f);

  const auto &world = session.world();
  const auto *player = world.findPlayer(session.playerId());
  char runInfo[128]{};
  std::snprintf(runInfo, sizeof(runInfo), "T+%07.1fs  |  T%06llu  |  C%zu  |  P%zu  |  TEAM %u  |  GROUP %zu",
    world.elapsedSeconds, static_cast<unsigned long long>(world.tick), session.activeContactCount(), world.planets.size(),
    player ? player->team + 1 : 0, session.selectedShipIds().size());
  ImGui::GetForegroundDrawList()->AddText(ImVec2(16.0f, panelY - 21.0f), IM_COL32(215, 225, 238, 255), runInfo);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 3.0f));
  ImGui::SetNextWindowPos(ImVec2(16.0f, panelY), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.96f);
  ImGui::Begin("Command deck", nullptr, kPanelFlags);

  const auto selectedShipIds = session.selectedShipIds();
  const auto *ship = selectedShipIds.empty() ? session.playerShip() : world.findShip(selectedShipIds.front());
  const auto *navigationShip = ship;
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const float availableWidth = ImGui::GetContentRegionAvail().x;
  const float columnWidth = std::max(0.0f, availableWidth - gap * 3.0f);
  const float selectionWidth = columnWidth * 0.19f;
  const float flightWidth = columnWidth * 0.23f;
  const float targetingWidth = columnWidth * 0.34f;
  const float navigationWidth = columnWidth - selectionWidth - flightWidth - targetingWidth;
  const float columnHeight = std::max(0.0f, ImGui::GetContentRegionAvail().y - 26.0f);

  ImGui::BeginChild("##selection", ImVec2(selectionWidth, columnHeight), true);
  ImGui::TextColored(kAccentColor, "SELECTED");
  if (ship)
  {
    ImGui::TextDisabled("%zu SHIPS  |  #%u %s", selectedShipIds.size(), ship->id, ship->name.c_str());
    ImGui::Text("HULL %.0f  SHD %.0f  EN %.0f", ship->combat.hull - ship->combat.hullDamage, ship->combat.shields,
      ship->resources.energy);
    ImGui::Text("SPD %.1f  HDG %+.0f / %+.0f", ship->speed, ship->orientation.yawDegrees, ship->orientation.pitchDegrees);
  }
  else
  {
    ImGui::TextDisabled("NO ACTIVE SHIP");
  }

  const auto &selection = session.selection();
  if (!selection)
  {
    ImGui::TextDisabled("NO CONTACT");
  }
  else if (selection->kind == spasim::Target::Kind::Planet && selection->id < session.world().planets.size())
  {
    ImGui::TextDisabled("PLANET  %s", world.planets[selection->id].name.c_str());
  }
  else if (selection->kind == spasim::Target::Kind::Entity)
  {
    ImGui::TextDisabled("ENTITY  #%u", selection->id);
  }
  ImGui::EndChild();

  ImGui::SameLine(0.0f, gap);
  ImGui::BeginChild("##flight", ImVec2(flightWidth, columnHeight), true);
  ImGui::TextColored(kAccentColor, "FLIGHT");
  const float flightContentWidth = ImGui::GetContentRegionAvail().x;
  const float halfWidth = (flightContentWidth - gap) * 0.5f;
  if (ImGui::Button(session.simulationRunning() ? "PAUSE" : "RESUME", ImVec2(halfWidth, 23.0f)))
  {
    session.toggleSimulation();
  }

  float simulationSpeed = session.simulationSpeed();
  ImGui::SameLine();
  ImGui::SetNextItemWidth(halfWidth);
  if (ImGui::SliderFloat("##simulation_rate", &simulationSpeed, 0.25f, 5.0f, "%.1fx"))
  {
    session.setSimulationSpeed(simulationSpeed);
  }

  const float thirdWidth = (flightContentWidth - gap * 2.0f) / 3.0f;
  if (ImGui::Button("THRUST", ImVec2(thirdWidth, 23.0f)))
  {
    session.execute(spasim::SetAcceleration{spasim::AccelerationMode::Accelerate});
  }

  ImGui::SameLine();
  if (ImGui::Button("BRAKE", ImVec2(thirdWidth, 23.0f)))
  {
    session.execute(spasim::SetAcceleration{spasim::AccelerationMode::Decelerate});
  }

  ImGui::SameLine();
  if (ImGui::Button("COAST", ImVec2(thirdWidth, 23.0f)))
  {
    session.execute(spasim::SetAcceleration{spasim::AccelerationMode::Cruise});
  }

  ImGui::TextDisabled("THROTTLE");
  ImGui::SameLine();
  for (int level = 1; level <= 4; ++level)
  {
    if (level > 1)
    {
      ImGui::SameLine();
    }

    if (ImGui::Button(std::to_string(level).c_str(), ImVec2(23.0f, 22.0f)))
    {
      session.execute(spasim::SetThrottleLevel{static_cast<std::uint8_t>(level)});
    }
  }

  const float headingButtonWidth = (flightContentWidth - gap * 3.0f) * 0.25f;
  if (ImGui::Button("YAW-", ImVec2(headingButtonWidth, 22.0f)))
  {
    session.adjustHeading(-15.0, 0.0);
  }

  ImGui::SameLine();
  if (ImGui::Button("YAW+", ImVec2(headingButtonWidth, 22.0f)))
  {
    session.adjustHeading(15.0, 0.0);
  }

  ImGui::SameLine();
  if (ImGui::Button("PITCH+", ImVec2(headingButtonWidth, 22.0f)))
  {
    session.adjustHeading(0.0, 10.0);
  }

  ImGui::SameLine();
  if (ImGui::Button("PITCH-", ImVec2(headingButtonWidth, 22.0f)))
  {
    session.adjustHeading(0.0, -10.0);
  }

  ImGui::EndChild();

  ImGui::SameLine(0.0f, gap);
  ImGui::BeginChild("##weapons", ImVec2(targetingWidth, columnHeight), true);
  ImGui::TextColored(kAccentColor, "WEAPONS / TARGET");

  std::size_t lockedTargetCount{};
  for (const auto shipId : selectedShipIds)
  {
    const auto *selectedShip = world.findShip(shipId);

    if (selectedShip && selectedShip->selectedTarget)
    {
      ++lockedTargetCount;
    }
  }

  std::string selectedShipLabel = "Select ship by ID";
  if (session.selection() && session.selection()->kind == spasim::Target::Kind::Entity)
  {
    const auto *selectedShip = session.world().findShip(session.selection()->id);

    if (selectedShip)
    {
      selectedShipLabel = "ID " + std::to_string(selectedShip->id) + " | " + selectedShip->name;
    }
  }

  const float targetingContentWidth = ImGui::GetContentRegionAvail().x;
  ImGui::SetNextItemWidth(targetingContentWidth * 0.62f);
  if (ImGui::BeginCombo("##target_by_id", selectedShipLabel.c_str()))
  {
    for (const auto &shipSlot : session.world().ships)
    {
      if (!shipSlot || !shipSlot->exists)
      {
        continue;
      }

      const auto &ship = *shipSlot;
      const bool selected =
        session.selection() && session.selection()->kind == spasim::Target::Kind::Entity && session.selection()->id == ship.id;
      const std::string label = "ID " + std::to_string(ship.id) + " | T" + std::to_string(ship.team + 1) + " | " + ship.name +
                                (ship.playerControlled ? " [player]" : " [practice]");

      if (ImGui::Selectable(label.c_str(), selected))
      {
        session.setSelection(spasim::Target{spasim::Target::Kind::Entity, ship.id});

        if (session.controlsShip(ship.id))
        {
          session.selectShips({ship.id});
        }
      }

      if (selected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  ImGui::SameLine();
  const std::string lockLabel =
    "LOCK " + std::to_string(lockedTargetCount) + "/" + std::to_string(selectedShipIds.size()) + " [R]";
  if (ImGui::Button(lockLabel.c_str(), ImVec2(-1.0f, 23.0f)))
  {
    session.aimAtSelection();
  }

  const bool mixedWeapons = ship && std::any_of(selectedShipIds.begin(), selectedShipIds.end(), [&world, ship](spasim::EntityId shipId) {
    const auto *selectedShip = world.findShip(shipId);
    return selectedShip && selectedShip->weapon != ship->weapon;
  });
  const std::string weaponName = currentWeaponName(session);
  ImGui::SetNextItemWidth(targetingContentWidth * 0.52f);
  if (ImGui::BeginCombo("##weapon_profile", weaponName.c_str()))
  {
    for (std::size_t index = 0; index < session.world().weapons.size(); ++index)
    {
      const bool selected = !selectedShipIds.empty() &&
        std::all_of(selectedShipIds.begin(), selectedShipIds.end(), [&world, index](spasim::EntityId shipId) {
          const auto *selectedShip = world.findShip(shipId);
          return selectedShip && selectedShip->weapon == index;
        });

      if (ImGui::Selectable(session.world().weapons[index].name.c_str(), selected))
      {
        session.execute(spasim::SetWeaponProfile{static_cast<spasim::WeaponId>(index)});
      }

      if (selected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }

  if (!mixedWeapons && ship && ship->weapon < world.weapons.size())
  {
    const auto &weapon = world.weapons[ship->weapon];
    ImGui::SameLine();
    ImGui::TextDisabled("R %.0f", weapon.range);
  }
  else if (mixedWeapons)
  {
    ImGui::SameLine();
    ImGui::TextDisabled("MIXED");
  }

  if (ImGui::Button("FIRE PHASOR  [SPACE]", ImVec2(-1.0f, 24.0f)))
  {
    session.firePhasor();
  }

  const float halfActionWidth = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
  if (ImGui::Button("TORPEDO [T]", ImVec2(halfActionWidth, 22.0f)))
  {
    session.launchTorpedo();
  }

  ImGui::SameLine();
  if (ImGui::Button("DETONATE [D]", ImVec2(halfActionWidth, 22.0f)))
  {
    session.execute(spasim::DetonateTorpedo{});
  }

  ImGui::EndChild();

  ImGui::SameLine(0.0f, gap);
  ImGui::BeginChild("##navigation", ImVec2(navigationWidth, columnHeight), true);
  ImGui::TextColored(kAccentColor, "NAVIGATION");
  ImGui::TextDisabled("GROUP %zu", selectedShipIds.size());
  if (session.selection() && session.selection()->kind == spasim::Target::Kind::Planet &&
      session.selection()->id < session.world().planets.size())
  {
    const auto &planet = session.world().planets[session.selection()->id];
    ImGui::TextDisabled("%s", planet.name.c_str());
    int orbitLevel = std::clamp(orbitLevel_, 1, std::max(1, static_cast<int>(planet.orbitLevelCount)));

    const float navigationContentWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(navigationContentWidth * 0.45f);
    if (ImGui::SliderInt("##orbit_level", &orbitLevel, 1, std::max(1, static_cast<int>(planet.orbitLevelCount))))
    {
      orbitLevel_ = orbitLevel;
    }

    ImGui::SameLine();
    if (ImGui::Button("SET COURSE", ImVec2(-1.0f, 23.0f)))
    {
      session.execute(spasim::SetGoal{*session.selection(), static_cast<std::uint8_t>(orbitLevel_)});
    }
  }
  else if (navigationShip && navigationShip->orbitingPlanet && *navigationShip->orbitingPlanet < session.world().planets.size())
  {
    const auto &planet = session.world().planets[*navigationShip->orbitingPlanet];
    ImGui::TextDisabled("%s  |  ORBIT %u", planet.name.c_str(), navigationShip->orbitLevel);
    int orbitLevel = std::clamp(orbitLevel_, 1, std::max(1, static_cast<int>(planet.orbitLevelCount)));

    const float navigationContentWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(navigationContentWidth * 0.45f);
    if (ImGui::SliderInt("##transfer_level", &orbitLevel, 1, std::max(1, static_cast<int>(planet.orbitLevelCount))))
    {
      orbitLevel_ = orbitLevel;
    }

    ImGui::SameLine();
    if (ImGui::Button("TRANSFER", ImVec2(-1.0f, 23.0f)))
    {
      session.execute(spasim::SetOrbitLevel{static_cast<std::uint8_t>(orbitLevel_)});
    }
  }
  else
  {
    ImGui::TextDisabled("SELECT A PLANET");
  }
  ImGui::EndChild();

  ImGui::Separator();
  ImGui::PushStyleColor(ImGuiCol_Text,
    session.statusSucceeded() ? ImVec4(0.50f, 0.88f, 0.71f, 1.0f) : ImVec4(1.0f, 0.48f, 0.44f, 1.0f));
  ImGui::TextWrapped("%s", session.status().c_str());
  ImGui::PopStyleColor();
  ImGui::End();
  ImGui::PopStyleVar(3);
}

std::string GameHud::currentWeaponName(const GameSession &session)
{
  const auto &world = session.world();
  const auto selectedShipIds = session.selectedShipIds();
  const auto *firstShip = selectedShipIds.empty() ? session.playerShip() : world.findShip(selectedShipIds.front());

  if (!firstShip || firstShip->weapon >= world.weapons.size())
  {
    return "No weapon";
  }

  for (const auto shipId : selectedShipIds)
  {
    const auto *ship = world.findShip(shipId);

    if (ship && ship->weapon != firstShip->weapon)
    {
      return "Mixed loadout";
    }
  }

  return world.weapons[firstShip->weapon].name;
}

} // namespace scsr
