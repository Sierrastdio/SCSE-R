#include "game_hud.hpp"

#include <imgui.h>

#include <algorithm>
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
  drawCameraPanel(session, camera);
  drawFlightPanel(session, width, height);
  drawSelectionPanel(session, height);
}

void GameHud::drawCameraPanel(const GameSession &session, Camera &camera)
{
  const auto &world = session.world();
  ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(365.0f, 155.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.92f);
  ImGui::Begin("SCSR HUD", nullptr, kPanelFlags);
  ImGui::TextColored(kAccentColor, "SCSR  /  SPACE COMBAT");
  ImGui::SameLine();
  ImGui::TextDisabled("BUILD 0.1");
  ImGui::Separator();
  ImGui::Text("T+ %07.1f s   |   TICK %06llu", world.elapsedSeconds, static_cast<unsigned long long>(world.tick));
  const auto *player = world.findPlayer(session.playerId());
  ImGui::Text("%zu CONTACTS   |   %zu PLANETS   |   TEAM %u", session.activeContactCount(), world.planets.size(),
    player ? player->team + 1 : 0);
  ImGui::Text("SELECTED CONTROL GROUP: %zu SHIPS", session.selectedShipIds().size());
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(16.0f, 173.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(365.0f, 58.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.88f);
  ImGui::Begin("View selector", nullptr, kPanelFlags);
  const auto viewButton = [&camera](const char *label, ViewPlane plane) {
    const bool active = camera.plane == plane;
    if (active)
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.40f, 0.57f, 1.0f));
    if (ImGui::Button(label, ImVec2(76.0f, 28.0f)))
      camera.plane = plane;
    if (active)
      ImGui::PopStyleColor();
  };
  viewButton("ORBIT [1]", ViewPlane::Orbit);
  ImGui::SameLine();
  viewButton("XY [2]", ViewPlane::XY);
  ImGui::SameLine();
  viewButton("XZ [3]", ViewPlane::XZ);
  ImGui::SameLine();
  viewButton("YZ [4]", ViewPlane::YZ);
  ImGui::End();
}

void GameHud::drawFlightPanel(GameSession &session, int width, int height)
{
  const float panelWidth = std::min(330.0f, static_cast<float>(width) * 0.38f);
  ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) - panelWidth - 16.0f, 16.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(panelWidth, std::max(350.0f, static_cast<float>(height) - 32.0f)), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.94f);
  ImGui::Begin("Flight systems", nullptr, kPanelFlags);

  ImGui::TextColored(kAccentColor, "FLIGHT SYSTEMS");
  ImGui::Separator();
  if (ImGui::Button(session.simulationRunning() ? "PAUSE SIMULATION" : "RESUME SIMULATION", ImVec2(-1.0f, 32.0f)))
    session.toggleSimulation();
  float simulationSpeed = session.simulationSpeed();
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::SliderFloat("Simulation rate", &simulationSpeed, 0.25f, 5.0f, "%.2fx"))
    session.setSimulationSpeed(simulationSpeed);

  const auto *ship = session.playerShip();
  const auto selectedShipIds = session.selectedShipIds();
  const auto *navigationShip = selectedShipIds.empty() ? ship : session.world().findShip(selectedShipIds.front());
  if (ship)
  {
    ImGui::SeparatorText("PILOT VESSEL");
    ImGui::Text("%s  /  ID %u", ship->name.c_str(), ship->id);
    ImGui::TextDisabled("%zu ships selected; flight, targeting and navigation orders affect this group.", selectedShipIds.size());
    ImGui::Text("HULL  %.0f / %.0f", ship->combat.hull - ship->combat.hullDamage, ship->combat.hull);
    ImGui::Text("SHIELDS  %.0f    ENERGY  %.0f", ship->combat.shields, ship->resources.energy);
    ImGui::Text("SPEED  %.2f    HEADING  %+.0f / %+.0f", ship->speed, ship->orientation.yawDegrees, ship->orientation.pitchDegrees);
    ImGui::Text("MODE  %s    THROTTLE  %u", spasim::toString(ship->acceleration), ship->throttleLevel);

    ImGui::SeparatorText("FLIGHT");
    if (ImGui::Button("THRUST  [W]", ImVec2((panelWidth - 42.0f) * 0.5f, 30.0f)))
      session.execute(spasim::SetAcceleration{spasim::AccelerationMode::Accelerate});
    ImGui::SameLine();
    if (ImGui::Button("BRAKE  [S]", ImVec2((panelWidth - 42.0f) * 0.5f, 30.0f)))
      session.execute(spasim::SetAcceleration{spasim::AccelerationMode::Decelerate});
    if (ImGui::Button("COAST  [X]", ImVec2(-1.0f, 28.0f)))
      session.execute(spasim::SetAcceleration{spasim::AccelerationMode::Cruise});

    ImGui::TextDisabled("Throttle stage");
    for (int level = 1; level <= 4; ++level)
    {
      if (level > 1)
        ImGui::SameLine();
      if (ImGui::Button(std::to_string(level).c_str(), ImVec2(42.0f, 27.0f)))
        session.execute(spasim::SetThrottleLevel{static_cast<std::uint8_t>(level)});
    }

    ImGui::TextDisabled("Heading controls");
    if (ImGui::Button("YAW -", ImVec2((panelWidth - 42.0f) * 0.5f, 27.0f)))
      session.adjustHeading(-15.0, 0.0);
    ImGui::SameLine();
    if (ImGui::Button("YAW +", ImVec2((panelWidth - 42.0f) * 0.5f, 27.0f)))
      session.adjustHeading(15.0, 0.0);
    if (ImGui::Button("PITCH +", ImVec2((panelWidth - 42.0f) * 0.5f, 27.0f)))
      session.adjustHeading(0.0, 10.0);
    ImGui::SameLine();
    if (ImGui::Button("PITCH -", ImVec2((panelWidth - 42.0f) * 0.5f, 27.0f)))
      session.adjustHeading(0.0, -10.0);
  }

  ImGui::SeparatorText("TARGETING");
  std::string selectedShipLabel = "Select ship by ID";
  if (session.selection() && session.selection()->kind == spasim::Target::Kind::Entity)
  {
    const auto *selectedShip = session.world().findShip(session.selection()->id);
    if (selectedShip)
      selectedShipLabel = "ID " + std::to_string(selectedShip->id) + " | " + selectedShip->name;
  }
  if (ImGui::BeginCombo("Ship by ID", selectedShipLabel.c_str()))
  {
    for (const auto &shipSlot : session.world().ships)
    {
      if (!shipSlot || !shipSlot->exists)
        continue;

      const auto &ship = *shipSlot;
      const bool selected =
        session.selection() && session.selection()->kind == spasim::Target::Kind::Entity && session.selection()->id == ship.id;
      const std::string label = "ID " + std::to_string(ship.id) + " | T" + std::to_string(ship.team + 1) + " | " + ship.name +
                                (ship.playerControlled ? " [player]" : " [practice]");
      if (ImGui::Selectable(label.c_str(), selected))
      {
        session.setSelection(spasim::Target{spasim::Target::Kind::Entity, ship.id});
        if (session.controlsShip(ship.id))
          session.selectShips({ship.id});
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  if (ImGui::Button("SET WEAPON TARGET  [R]", ImVec2(-1.0f, 30.0f)))
    session.aimAtSelection();
  bool hasWeaponTarget = false;
  for (const auto shipId : selectedShipIds)
  {
    const auto *selectedShip = session.world().findShip(shipId);
    hasWeaponTarget = hasWeaponTarget || (selectedShip && selectedShip->selectedTarget.has_value());
  }
  if (hasWeaponTarget)
    ImGui::Text("Weapon target locked on selected ship(s)");
  else
    ImGui::TextDisabled("No weapon target locked");

  if (ImGui::BeginCombo("Weapon", currentWeaponName(session)))
  {
    for (std::size_t index = 0; index < session.world().weapons.size(); ++index)
    {
      const auto *player = session.world().findPlayer(session.playerId());
      const bool selected = player && player->weapon == index;
      if (ImGui::Selectable(session.world().weapons[index].name.c_str(), selected))
        session.execute(spasim::SetWeaponProfile{static_cast<spasim::WeaponId>(index)});
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  if (ImGui::Button("FIRE PHASOR  [SPACE]", ImVec2(-1.0f, 30.0f)))
    session.execute(spasim::FirePhasor{1.0});
  if (ImGui::Button("LAUNCH TORPEDO  [T]  |  SELECTED GROUP", ImVec2(-1.0f, 30.0f)))
    session.launchTorpedo();
  if (ImGui::Button("DETONATE TORPEDO  [D]", ImVec2(-1.0f, 28.0f)))
    session.execute(spasim::DetonateTorpedo{});

  ImGui::SeparatorText("NAVIGATION");
  ImGui::TextDisabled("Navigation commands apply to the selected ship group.");
  if (session.selection() && session.selection()->kind == spasim::Target::Kind::Planet &&
      session.selection()->id < session.world().planets.size())
  {
    const auto &planet = session.world().planets[session.selection()->id];
    ImGui::Text("Course destination: %s", planet.name.c_str());
    int orbitLevel = std::clamp(orbitLevel_, 1, std::max(1, static_cast<int>(planet.orbitLevelCount)));
    if (ImGui::SliderInt("Orbit level", &orbitLevel, 1, std::max(1, static_cast<int>(planet.orbitLevelCount))))
      orbitLevel_ = orbitLevel;
    if (ImGui::Button("SET COURSE TO SELECTED SHIPS", ImVec2(-1.0f, 30.0f)))
      session.execute(spasim::SetGoal{*session.selection(), static_cast<std::uint8_t>(orbitLevel_)});
  }
  else if (navigationShip && navigationShip->orbitingPlanet && *navigationShip->orbitingPlanet < session.world().planets.size())
  {
    const auto &planet = session.world().planets[*navigationShip->orbitingPlanet];
    ImGui::Text("Orbiting %s  /  LEVEL %u", planet.name.c_str(), navigationShip->orbitLevel);
    int orbitLevel = std::clamp(orbitLevel_, 1, std::max(1, static_cast<int>(planet.orbitLevelCount)));
    if (ImGui::SliderInt("Transfer level", &orbitLevel, 1, std::max(1, static_cast<int>(planet.orbitLevelCount))))
      orbitLevel_ = orbitLevel;
    if (ImGui::Button("TRANSFER SELECTED ORBITS", ImVec2(-1.0f, 30.0f)))
      session.execute(spasim::SetOrbitLevel{static_cast<std::uint8_t>(orbitLevel_)});
  }
  else
    ImGui::TextDisabled("Select a planet to set a course.");

  ImGui::Separator();
  ImGui::PushStyleColor(ImGuiCol_Text,
    session.statusSucceeded() ? ImVec4(0.50f, 0.88f, 0.71f, 1.0f) : ImVec4(1.0f, 0.48f, 0.44f, 1.0f));
  ImGui::TextWrapped("%s", session.status().c_str());
  ImGui::PopStyleColor();
  ImGui::End();
}

void GameHud::drawSelectionPanel(const GameSession &session, int height)
{
  ImGui::SetNextWindowPos(ImVec2(16.0f, static_cast<float>(height) - 112.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(365.0f, 94.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.88f);
  ImGui::Begin("Selected contact", nullptr, kPanelFlags);
  ImGui::TextColored(kAccentColor, "SELECTED CONTACT");
  ImGui::TextDisabled("CONTROL GROUP: %zu SHIPS", session.selectedShipIds().size());
  const auto &selection = session.selection();
  if (!selection)
    ImGui::TextDisabled("Click a ship, base, torpedo, or planet.");
  else if (selection->kind == spasim::Target::Kind::Planet && selection->id < session.world().planets.size())
  {
    const auto &planet = session.world().planets[selection->id];
    ImGui::Text("%s  |  GRAVITY RADIUS %.0f", planet.name.c_str(), planet.sphereOfInfluenceRadius);
  }
  else if (selection->kind == spasim::Target::Kind::Entity)
  {
    const auto *ship = session.world().findShip(selection->id);
    const auto *base = session.world().findBase(selection->id);
    const auto *torpedo = session.world().findTorpedo(selection->id);
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

const char *GameHud::currentWeaponName(const GameSession &session)
{
  const auto *player = session.world().findPlayer(session.playerId());
  if (!player || player->weapon >= session.world().weapons.size())
    return "No weapon";
  return session.world().weapons[player->weapon].name.c_str();
}

} // namespace scsr
