#include "game_session.hpp"

#include "engine/world.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace scsr
{

GameSession::GameSession() : simulation_(spasim::makeInitialWorld())
{
  const auto playerId = spasim::joinTeam(simulation_.world(), 0, "Pilot");
  if (!playerId)
    throw std::runtime_error("Could not join the starter team; check the SCSE scenario's player limits.");

  playerId_ = *playerId;
  const auto *player = simulation_.world().findPlayer(playerId_);
  if (!player)
    throw std::runtime_error("The SCSE engine did not retain the newly joined player.");

  selection_ = spasim::Target{spasim::Target::Kind::Entity, player->shipId};
  setStatus("FLIGHT READY — select a contact or planet to begin.", true);
}

spasim::World &GameSession::world() noexcept { return simulation_.world(); }

const spasim::World &GameSession::world() const noexcept { return simulation_.world(); }

spasim::PlayerId GameSession::playerId() const noexcept { return playerId_; }

const spasim::Ship *GameSession::playerShip() const noexcept
{
  const auto *player = world().findPlayer(playerId_);
  return player ? world().findShip(player->shipId) : nullptr;
}

const std::optional<spasim::Target> &GameSession::selection() const noexcept { return selection_; }

void GameSession::setSelection(std::optional<spasim::Target> selection) { selection_ = std::move(selection); }

std::vector<spasim::EntityId> GameSession::selectedShipIds() const
{
  const auto *player = world().findPlayer(playerId_);
  if (!player)
    return {};
  if (player->selectedShips.empty())
    return controlsShip(player->shipId) ? std::vector<spasim::EntityId>{player->shipId} : std::vector<spasim::EntityId>{};

  std::vector<spasim::EntityId> selected;
  for (const auto shipId : player->selectedShips)
    if (controlsShip(shipId) && std::ranges::find(selected, shipId) == selected.end())
      selected.push_back(shipId);
  return selected;
}

bool GameSession::controlsShip(spasim::EntityId shipId) const noexcept
{
  const auto *ship = world().findShip(shipId);
  return ship && ship->exists && !ship->combat.destroyed && ship->playerControlled && ship->owner == playerId_;
}

bool GameSession::selectShips(std::vector<spasim::EntityId> shipIds) { return execute(spasim::SelectShips{std::move(shipIds)}); }

bool GameSession::toggleShipSelection(spasim::EntityId shipId)
{
  if (!controlsShip(shipId))
  {
    setStatus("Only active ships under your control can be selected.", false);
    return false;
  }

  auto selected = selectedShipIds();
  const auto found = std::ranges::find(selected, shipId);
  if (found == selected.end())
    selected.push_back(shipId);
  else if (selected.size() > 1)
    selected.erase(found);
  else
  {
    setStatus("At least one ship must remain selected.", false);
    return false;
  }
  return selectShips(std::move(selected));
}

const ShipTrails &GameSession::shipTrails() const noexcept { return trails_; }

const std::string &GameSession::status() const noexcept { return status_; }

bool GameSession::statusSucceeded() const noexcept { return statusSucceeded_; }

void GameSession::setStatus(std::string text, bool succeeded)
{
  status_ = std::move(text);
  statusSucceeded_ = succeeded;
}

bool GameSession::simulationRunning() const noexcept { return simulationRunning_; }

void GameSession::toggleSimulation() { simulationRunning_ = !simulationRunning_; }

float GameSession::simulationSpeed() const noexcept { return simulationSpeed_; }

void GameSession::setSimulationSpeed(float speed) noexcept { simulationSpeed_ = std::clamp(speed, 0.25f, 5.0f); }

void GameSession::advance(double frameSeconds)
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
    simulation_.step();
    accumulator_ -= step;
    ++stepCount;
  }
  if (stepCount == 100)
    accumulator_ = std::fmod(accumulator_, step);
}

void GameSession::updateTrails()
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

bool GameSession::execute(const spasim::WorldCommand &command)
{
  const auto result = simulation_.execute(playerId_, command);
  setStatus(result.message, static_cast<bool>(result));
  return static_cast<bool>(result);
}

void GameSession::firePhasor() { execute(spasim::FirePhasor{1.0}); }

void GameSession::aimAtSelection()
{
  if (!selection_ || selection_->kind != spasim::Target::Kind::Entity)
  {
    setStatus("Select a ship, base, or torpedo before targeting.", false);
    return;
  }

  execute(spasim::SelectTarget{*selection_});
}

void GameSession::launchTorpedo()
{
  const auto *ship = playerShip();
  if (!ship)
    return;

  const double yaw = ship->orientation.yawDegrees * std::numbers::pi / 180.0;
  const double pitch = ship->orientation.pitchDegrees * std::numbers::pi / 180.0;
  const spasim::Vec3 forward{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
  execute(spasim::FireTorpedo{100.0, forward});
}

void GameSession::adjustHeading(double yawDegrees, double pitchDegrees)
{
  const auto selected = selectedShipIds();
  if (selected.empty())
    return;
  const auto *ship = world().findShip(selected.front());
  if (!ship)
    return;

  const double yaw = (ship->orientation.yawDegrees + yawDegrees) * std::numbers::pi / 180.0;
  const double pitch = std::clamp(ship->orientation.pitchDegrees + pitchDegrees, -89.0, 89.0) * std::numbers::pi / 180.0;
  execute(spasim::SetHeading{{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)}});
}

std::size_t GameSession::activeContactCount() const noexcept
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

} // namespace scsr
