#pragma once

#include "engine/types.hpp"

#include <imgui.h>

#include <deque>
#include <optional>
#include <unordered_map>

namespace scsr
{

enum class ViewPlane
{
  Orbit,
  XY,
  XZ,
  YZ
};

struct Camera
{
  ViewPlane plane{ViewPlane::Orbit};
  float azimuth{-55.0f};
  float elevation{28.0f};
  float panX{};
  float panY{};
  float zoom{1.0f};
  bool dragging{};
  int dragX{};
  int dragY{};
};

struct WorldProjection
{
  spasim::Vec3 center{};
  float scale{1.0f};
  float width{};
  float height{};
  const Camera *camera{};

  [[nodiscard]] ImVec2 project(const spasim::Vec3 &point) const;
};

using ShipTrails = std::unordered_map<spasim::EntityId, std::deque<spasim::Vec3>>;

[[nodiscard]] WorldProjection makeProjection(const spasim::World &world, const Camera &camera, float width, float height);
[[nodiscard]] std::optional<spasim::Target> pickObject(const spasim::World &world, const WorldProjection &projection,
  const ImVec2 &point, float radius = 16.0f);
void drawWorld(ImDrawList &drawList, const spasim::World &world, const Camera &camera, const std::optional<spasim::Target> &selection,
  const ShipTrails &trails, float width, float height);

} // namespace scsr
