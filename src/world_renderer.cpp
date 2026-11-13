#include "world_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

namespace scsr
{
namespace
{
constexpr std::array<ImU32, 5> kTeamColors{
  IM_COL32(74, 190, 255, 255),
  IM_COL32(255, 100, 112, 255),
  IM_COL32(183, 142, 255, 255),
  IM_COL32(255, 197, 91, 255),
  IM_COL32(88, 225, 175, 255),
};

ImU32 teamColor(spasim::TeamId team) { return kTeamColors[team % kTeamColors.size()]; }

spasim::Vec3 forwardVector(const spasim::Orientation &orientation)
{
  const double yaw = orientation.yawDegrees * std::numbers::pi / 180.0;
  const double pitch = orientation.pitchDegrees * std::numbers::pi / 180.0;
  return {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
}

void drawDashedLine(ImDrawList &drawList, const ImVec2 &from, const ImVec2 &to, ImU32 color, float thickness)
{
  constexpr int segmentCount = 12;
  for (int segment = 0; segment < segmentCount; segment += 2)
  {
    const float start = static_cast<float>(segment) / segmentCount;
    const float end = static_cast<float>(segment + 1) / segmentCount;
    drawList.AddLine(ImVec2(from.x + (to.x - from.x) * start, from.y + (to.y - from.y) * start),
      ImVec2(from.x + (to.x - from.x) * end, from.y + (to.y - from.y) * end), color, thickness);
  }
}

void drawArrow(ImDrawList &drawList, const ImVec2 &from, const ImVec2 &to, ImU32 color, float thickness)
{
  drawList.AddLine(from, to, color, thickness);
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  const float length = std::hypot(dx, dy);
  if (length < 1.0f)
    return;

  const float ux = dx / length;
  const float uy = dy / length;
  const ImVec2 base(to.x - ux * 7.0f, to.y - uy * 7.0f);
  const ImVec2 side(-uy * 3.5f, ux * 3.5f);
  drawList.AddTriangleFilled(to, ImVec2(base.x + side.x, base.y + side.y), ImVec2(base.x - side.x, base.y - side.y), color);
}

void drawRing(ImDrawList &drawList, const WorldProjection &projection, const spasim::Vec3 &center, double radius, ImU32 color,
  bool dashed)
{
  if (radius <= 0.0)
    return;

  constexpr int segments = 64;
  if (dashed)
  {
    for (int index = 0; index < segments; index += 2)
    {
      const double firstAngle = 2.0 * std::numbers::pi * index / segments;
      const double secondAngle = 2.0 * std::numbers::pi * (index + 1) / segments;
      const ImVec2 first =
        projection.project({center.x + radius * std::cos(firstAngle), center.y + radius * std::sin(firstAngle), center.z});
      const ImVec2 second =
        projection.project({center.x + radius * std::cos(secondAngle), center.y + radius * std::sin(secondAngle), center.z});
      drawList.AddLine(first, second, color, 1.0f);
    }
    return;
  }

  drawList.PathClear();
  for (int index = 0; index < segments; ++index)
  {
    const double angle = 2.0 * std::numbers::pi * index / segments;
    drawList.PathLineTo(projection.project({center.x + radius * std::cos(angle), center.y + radius * std::sin(angle), center.z}));
  }
  drawList.PathStroke(color, ImDrawFlags_Closed, 1.0f);
}

void drawAxes(ImDrawList &drawList, const WorldProjection &projection)
{
  constexpr double axisLength = 500.0;
  const ImVec2 origin = projection.project({});
  drawList.AddCircleFilled(origin, 2.5f, IM_COL32(235, 241, 252, 255));

  const std::array<std::pair<const char *, spasim::Vec3>, 3> axes{
    std::pair{"X", spasim::Vec3{axisLength, 0.0, 0.0}},
    std::pair{"Y", spasim::Vec3{0.0, axisLength, 0.0}},
    std::pair{"Z", spasim::Vec3{0.0, 0.0, axisLength}},
  };
  constexpr std::array<ImU32, 3> colors{
    IM_COL32(255, 103, 113, 255),
    IM_COL32(90, 225, 149, 255),
    IM_COL32(103, 167, 255, 255),
  };

  for (std::size_t index = 0; index < axes.size(); ++index)
  {
    const ImVec2 end = projection.project(axes[index].second);
    drawArrow(drawList, origin, end, colors[index], 1.5f);
    drawList.AddText(ImVec2(end.x + 5.0f, end.y - 5.0f), colors[index], axes[index].first);
  }

  for (int step : {-2, -1, 1, 2})
  {
    const double offset = axisLength * step / 2.0;
    spasim::Vec3 gridPoint{};
    if (projection.camera->plane == ViewPlane::XY)
      gridPoint = {offset, -offset, 0.0};
    else if (projection.camera->plane == ViewPlane::XZ)
      gridPoint = {offset, 0.0, -offset};
    else if (projection.camera->plane == ViewPlane::YZ)
      gridPoint = {0.0, offset, -offset};
    else
      gridPoint = {offset, -offset, 0.0};
    drawDashedLine(drawList, origin, projection.project(gridPoint), IM_COL32(45, 59, 78, 170), 1.0f);
  }
}

void drawShip(ImDrawList &drawList, const WorldProjection &projection, const spasim::Ship &ship, ImU32 color, const ShipTrails &trails)
{
  const ImVec2 position = projection.project(ship.position);
  const spasim::Vec3 forward = forwardVector(ship.orientation);
  const ImVec2 forwardPoint = projection.project(ship.position + forward * 100.0);
  float dx = forwardPoint.x - position.x;
  float dy = forwardPoint.y - position.y;
  float directionLength = std::hypot(dx, dy);
  if (directionLength < 0.001f)
  {
    dx = 0.0f;
    dy = -1.0f;
    directionLength = 1.0f;
  }
  dx /= directionLength;
  dy /= directionLength;
  const float px = -dy;
  const float py = dx;
  const ImVec2 nose(position.x + dx * 10.0f, position.y + dy * 10.0f);
  const ImVec2 left(position.x - dx * 6.0f + px * 5.5f, position.y - dy * 6.0f + py * 5.5f);
  const ImVec2 right(position.x - dx * 6.0f - px * 5.5f, position.y - dy * 6.0f - py * 5.5f);
  drawList.AddTriangleFilled(nose, left, right, color);
  drawList.AddTriangle(nose, left, right, IM_COL32(238, 245, 255, 255), 1.0f);

  const double speed = std::max(ship.speed, 1.0);
  const double headingDistance = std::clamp(speed * 10.0, 80.0, 600.0);
  drawDashedLine(drawList, position, projection.project(ship.position + forward * headingDistance), color, 1.5f);

  const double velocityLength = ship.velocity.length();
  if (velocityLength > 0.01)
  {
    const double arrowLength = std::clamp(velocityLength * 8.0, 20.0, 100.0);
    drawArrow(drawList, position, projection.project(ship.position + ship.velocity * (arrowLength / velocityLength)),
      IM_COL32(249, 213, 106, 255), 1.7f);
  }

  const auto trail = trails.find(ship.id);
  if (trail != trails.end() && trail->second.size() > 1)
  {
    std::vector<ImVec2> points;
    points.reserve(trail->second.size());
    for (const auto &point : trail->second)
      points.push_back(projection.project(point));
    drawList.AddPolyline(points.data(), static_cast<int>(points.size()), IM_COL32(100, 160, 235, 105), 0, 1.5f);
  }

  drawList.AddText(ImVec2(position.x + 11.0f, position.y + 9.0f), color, (ship.name + " #" + std::to_string(ship.id)).c_str());
}

} // namespace

ImVec2 WorldProjection::project(const spasim::Vec3 &point) const
{
  const double x = point.x - center.x;
  const double y = point.y - center.y;
  const double z = point.z - center.z;
  double screenX{};
  double screenY{};

  if (camera->plane == ViewPlane::XY)
  {
    screenX = x;
    screenY = -y;
  }
  else if (camera->plane == ViewPlane::XZ)
  {
    screenX = x;
    screenY = -z;
  }
  else if (camera->plane == ViewPlane::YZ)
  {
    screenX = y;
    screenY = -z;
  }
  else
  {
    const double azimuth = camera->azimuth * std::numbers::pi / 180.0;
    const double elevation = camera->elevation * std::numbers::pi / 180.0;
    const double rightX = std::cos(azimuth);
    const double rightY = std::sin(azimuth);
    const double upX = -std::sin(azimuth) * std::sin(elevation);
    const double upY = std::cos(azimuth) * std::sin(elevation);
    const double upZ = std::cos(elevation);
    screenX = x * rightX + y * rightY;
    screenY = -(x * upX + y * upY + z * upZ);
  }

  return {width * 0.5f + camera->panX + static_cast<float>(screenX) * scale,
    height * 0.54f + camera->panY + static_cast<float>(screenY) * scale};
}

WorldProjection makeProjection(const spasim::World &world, const Camera &camera, float width, float height)
{
  std::vector<spasim::Vec3> positions;
  positions.reserve(world.planets.size() + world.ships.size() + world.bases.size() + world.torpedoes.size());
  for (const auto &planet : world.planets)
    positions.push_back(planet.position);
  for (const auto &ship : world.ships)
    if (ship && ship->exists)
      positions.push_back(ship->position);
  for (const auto &base : world.bases)
    if (base && base->exists)
      positions.push_back(base->position);
  for (const auto &torpedo : world.torpedoes)
    if (torpedo && torpedo->exists)
      positions.push_back(torpedo->position);
  if (positions.empty())
    positions.push_back({});

  spasim::Vec3 minimum = positions.front();
  spasim::Vec3 maximum = positions.front();
  for (const auto &position : positions)
  {
    minimum.x = std::min(minimum.x, position.x);
    minimum.y = std::min(minimum.y, position.y);
    minimum.z = std::min(minimum.z, position.z);
    maximum.x = std::max(maximum.x, position.x);
    maximum.y = std::max(maximum.y, position.y);
    maximum.z = std::max(maximum.z, position.z);
  }

  const spasim::Vec3 center = (minimum + maximum) * 0.5;
  const double span = std::max({maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z, 250.0});
  const float safeWidth = std::max(width, 100.0f);
  const float safeHeight = std::max(height, 100.0f);
  const float scale = std::min(safeWidth, safeHeight) * 0.72f / static_cast<float>(span) * camera.zoom;
  return {center, scale, width, height, &camera};
}

std::optional<spasim::Target> pickObject(const spasim::World &world, const WorldProjection &projection, const ImVec2 &point,
  float radius)
{
  const float maxDistanceSquared = radius * radius;
  float bestDistanceSquared = maxDistanceSquared;
  std::optional<spasim::Target> picked;
  const auto consider = [&](spasim::Target::Kind kind, std::uint32_t id, const spasim::Vec3 &position) {
    const ImVec2 screen = projection.project(position);
    const float dx = screen.x - point.x;
    const float dy = screen.y - point.y;
    const float distanceSquared = dx * dx + dy * dy;
    if (distanceSquared <= bestDistanceSquared)
    {
      bestDistanceSquared = distanceSquared;
      picked = spasim::Target{kind, id};
    }
  };

  for (const auto &planet : world.planets)
    consider(spasim::Target::Kind::Planet, planet.id, planet.position);
  for (const auto &ship : world.ships)
    if (ship && ship->exists)
      consider(spasim::Target::Kind::Entity, ship->id, ship->position);
  for (const auto &base : world.bases)
    if (base && base->exists)
      consider(spasim::Target::Kind::Entity, base->id, base->position);
  for (const auto &torpedo : world.torpedoes)
    if (torpedo && torpedo->exists)
      consider(spasim::Target::Kind::Entity, torpedo->id, torpedo->position);
  return picked;
}

void drawWorld(ImDrawList &drawList, const spasim::World &world, const Camera &camera, const std::optional<spasim::Target> &selection,
  const ShipTrails &trails, float width, float height)
{
  drawList.AddRectFilled({0.0f, 0.0f}, {width, height}, IM_COL32(7, 11, 19, 255));
  for (int index = 0; index < 140; ++index)
  {
    const float x = static_cast<float>((index * 97 + 31) % std::max(1, static_cast<int>(width)));
    const float y = static_cast<float>((index * 193 + 67) % std::max(1, static_cast<int>(height)));
    const float radius = index % 13 == 0 ? 1.6f : 1.0f;
    drawList.AddCircleFilled({x, y}, radius, IM_COL32(116, 145, 180, index % 4 == 0 ? 125 : 70));
  }

  const WorldProjection projection = makeProjection(world, camera, width, height);
  drawAxes(drawList, projection);

  for (const auto &planet : world.planets)
  {
    if (planet.sphereOfInfluenceRadius > 0.0)
      drawRing(drawList, projection, planet.position, planet.sphereOfInfluenceRadius, IM_COL32(220, 202, 92, 175), true);

    for (std::uint8_t index = 0; index < planet.orbitLevelCount; ++index)
    {
      const double radius = planet.orbitLevelRadii[index];
      if (radius > 0.0 && index > 0)
        drawRing(drawList, projection, planet.position, radius, IM_COL32(95, 116, 143, 125), true);
      const ImVec2 label = projection.project({planet.position.x + radius, planet.position.y, planet.position.z});
      drawList.AddText(ImVec2(label.x + 2.0f, label.y - 7.0f), IM_COL32(173, 190, 213, 200),
        ("L" + std::to_string(index + 1)).c_str());
    }

    const ImVec2 center = projection.project(planet.position);
    const float planetRadius = std::clamp(static_cast<float>(planet.radius) * projection.scale, 5.0f, 20.0f);
    drawList.AddCircleFilled(center, planetRadius, IM_COL32(213, 211, 121, 255));
    drawList.AddCircle(center, planetRadius, IM_COL32(255, 255, 230, 255), 0, 1.5f);
    drawList.AddText(ImVec2(center.x + planetRadius + 5.0f, center.y - 10.0f), IM_COL32(249, 245, 180, 255), planet.name.c_str());

    if (selection && selection->kind == spasim::Target::Kind::Planet && selection->id == planet.id)
      drawList.AddCircle(center, planetRadius + 7.0f, IM_COL32(255, 255, 255, 245), 0, 2.0f);
  }

  for (const auto &team : world.teams)
  {
    if (team.homePlanet >= world.planets.size())
      continue;
    const auto &home = world.planets[team.homePlanet];
    const spasim::Vec3 spawn{
      home.position.x + team.spawnOffset.x, home.position.y + team.spawnOffset.y, home.position.z + team.spawnOffset.z};
    const ImVec2 position = projection.project(spawn);
    const ImU32 color = teamColor(team.id);
    if (team.randomSpawn && team.spawnRadius > 0.0)
      drawRing(drawList, projection, spawn, team.spawnRadius, color, true);
    else
      drawList.AddCircle(position, 6.0f, color, 0, 1.5f);
    drawList.AddText(ImVec2(position.x + 7.0f, position.y - 12.0f), color, (team.name + " spawn").c_str());
  }

  for (const auto &ship : world.ships)
  {
    if (!ship || !ship->exists)
      continue;
    const ImU32 color = teamColor(ship->team);
    drawShip(drawList, projection, *ship, color, trails);
    const ImVec2 position = projection.project(ship->position);
    if (selection && selection->kind == spasim::Target::Kind::Entity && selection->id == ship->id)
      drawList.AddCircle(position, 16.0f, IM_COL32(255, 255, 255, 230), 0, 2.0f);
  }

  for (const auto &base : world.bases)
  {
    if (!base || !base->exists)
      continue;
    const ImVec2 position = projection.project(base->position);
    const ImU32 color = teamColor(base->team);
    drawList.AddRectFilled(ImVec2(position.x - 6.0f, position.y - 6.0f), ImVec2(position.x + 6.0f, position.y + 6.0f), color);
    drawList.AddRect(ImVec2(position.x - 6.0f, position.y - 6.0f), ImVec2(position.x + 6.0f, position.y + 6.0f),
      IM_COL32(240, 245, 255, 255));
    if (selection && selection->kind == spasim::Target::Kind::Entity && selection->id == base->id)
      drawList.AddCircle(position, 15.0f, IM_COL32(255, 255, 255, 230), 0, 2.0f);
    drawList.AddText(ImVec2(position.x + 9.0f, position.y + 6.0f), color, ("base #" + std::to_string(base->id)).c_str());
  }

  for (const auto &torpedo : world.torpedoes)
  {
    if (!torpedo || !torpedo->exists)
      continue;
    const ImVec2 position = projection.project(torpedo->position);
    const ImU32 color = teamColor(torpedo->team);
    const ImVec2 future = projection.project(torpedo->position + torpedo->velocity * 0.3);
    drawArrow(drawList, position, future, color, 1.5f);
    drawList.AddCircleFilled(position, 4.0f, IM_COL32(255, 255, 255, 255));
    drawList.AddCircle(position, 6.0f, color, 0, 1.5f);
    if (selection && selection->kind == spasim::Target::Kind::Entity && selection->id == torpedo->id)
      drawList.AddCircle(position, 14.0f, IM_COL32(255, 255, 255, 230), 0, 2.0f);
  }

  const char *plane = camera.plane == ViewPlane::Orbit ? "ORBITAL"
                      : camera.plane == ViewPlane::XY  ? "XY"
                      : camera.plane == ViewPlane::XZ  ? "XZ"
                                                       : "YZ";
  drawList.AddText(ImVec2(16.0f, height - 23.0f), IM_COL32(169, 185, 204, 230),
    "RMB orbit  |  SHIFT+RMB pan  |  Wheel zoom  |  1-4 projections");
  drawList.AddText(ImVec2(width - 110.0f, height - 23.0f), IM_COL32(105, 190, 255, 255), plane);
}

} // namespace scsr
