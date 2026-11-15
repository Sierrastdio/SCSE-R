#pragma once

#include "engine/engine.hpp"
#include "renderer/world_renderer.hpp"

#include <optional>
#include <string>
#include <vector>

namespace scsr
{

class GameSession
{
public:
  GameSession();

  [[nodiscard]] spasim::World &world() noexcept;
  [[nodiscard]] const spasim::World &world() const noexcept;
  [[nodiscard]] spasim::PlayerId playerId() const noexcept;
  [[nodiscard]] const spasim::Ship *playerShip() const noexcept;

  [[nodiscard]] const std::optional<spasim::Target> &selection() const noexcept;
  void setSelection(std::optional<spasim::Target> selection);
  [[nodiscard]] std::vector<spasim::EntityId> selectedShipIds() const;
  [[nodiscard]] bool controlsShip(spasim::EntityId shipId) const noexcept;
  bool selectShips(std::vector<spasim::EntityId> shipIds);
  bool toggleShipSelection(spasim::EntityId shipId);
  [[nodiscard]] const ShipTrails &shipTrails() const noexcept;
  [[nodiscard]] const WeaponShotVisuals &weaponShots() const noexcept;

  [[nodiscard]] const std::string &status() const noexcept;
  [[nodiscard]] bool statusSucceeded() const noexcept;
  void setStatus(std::string text, bool succeeded);

  [[nodiscard]] bool simulationRunning() const noexcept;
  void toggleSimulation();
  [[nodiscard]] float simulationSpeed() const noexcept;
  void setSimulationSpeed(float speed) noexcept;
  void advance(double frameSeconds);
  void updateTrails();

  bool execute(const spasim::WorldCommand &command);
  void firePhasor();
  void aimAtSelection();
  void launchTorpedo();
  void adjustHeading(double yawDegrees, double pitchDegrees);
  [[nodiscard]] std::size_t activeContactCount() const noexcept;

private:
  spasim::Simulation simulation_;
  spasim::PlayerId playerId_{};
  std::optional<spasim::Target> selection_;
  ShipTrails trails_;
  WeaponShotVisuals weaponShots_;
  std::string status_;
  bool statusSucceeded_{true};
  bool simulationRunning_{true};
  float simulationSpeed_{1.0f};
  double accumulator_{};
};

} // namespace scsr
