#include "Fixtures.hpp"
#include "TestFramework.hpp"
#include "pacman/Entity.hpp"

using namespace pac;

namespace {

// The base class of the player and of the ghosts. Its edge cases -- no
// direction, no speed, an absurd speed -- decide what happens when the game
// goes wrong, and they are hard to reach through the game itself: the game
// never moves an entity without a direction, and it never gives it a speed
// of zero.
//
// The three helpers are protected in Entity, so a probe is the only way to
// call them.
class Probe : public Entity {
public:
    Probe(Maze& maze, Vec2 position, Direction direction, float speed)
        : Entity(maze, position, direction, speed) {
    }

    void moveBy(float distance) { advance(distance); }
    float toNextCenter() const { return distanceToNextCenter(); }
    void snap() { snapToNextCenter(); }
};

// A map with rooms and walls.
Maze openMap() {
    return Maze(pactest::makeLevel(pactest::layoutMap(), __FILE__, __LINE__));
}

// A corridor with a tunnel at both ends: an entity that keeps going right
// wraps around forever.
Maze corridor() {
    return Maze(pactest::makeLevel(pactest::ringMap(), __FILE__, __LINE__));
}

}  // namespace

PAC_TEST(Entity, ignoresANonPositiveDelta) {
    Maze maze = openMap();
    Probe entity(maze, tileCenterOf(1, 1), Direction::Right, 6.0f);
    const Vec2 start = entity.position();

    entity.update(0.0f);
    entity.update(-1.0f);

    CHECK(nearlyEquals(entity.position(), start));
    CHECK_NEAR(entity.distanceTravelled(), 0.0, 1e-6);
}

PAC_TEST(Entity, ignoresAZeroSpeed) {
    Maze maze = openMap();
    Probe entity(maze, tileCenterOf(1, 1), Direction::Right, 0.0f);
    const Vec2 start = entity.position();

    entity.update(0.5f);

    CHECK(nearlyEquals(entity.position(), start));
    CHECK_NEAR(entity.distanceTravelled(), 0.0, 1e-6);
}

PAC_TEST(Entity, stopsAtTheGuardWhenTheSpeedIsAbsurd) {
    Maze maze = corridor();
    Probe entity(maze, tileCenterOf(1, 1), Direction::Right, 1.0e6f);

    entity.update(1.0f);

    // It moved -- but the guard stopped it instead of letting it run around
    // the corridor forever.
    CHECK(entity.distanceTravelled() > 100.0f);
    CHECK(entity.distanceTravelled() <= 513.0f);
    CHECK_EQ(entity.tileY(), 1);
}

PAC_TEST(Entity, aBlockedEntityDoesNotMove) {
    Maze maze = openMap();
    Probe entity(maze, tileCenterOf(1, 1), Direction::Right, 6.0f);
    entity.update(0.1f);
    CHECK(!entity.isAtTileCenter());

    // The direction is taken away between two tiles: the entity has to stop
    // where it is instead of drifting.
    entity.setDirection(Direction::None);
    const Vec2 stopped = entity.position();
    entity.update(0.1f);

    CHECK(nearlyEquals(entity.position(), stopped));
}

PAC_TEST(Entity, cannotMoveWithoutADirection) {
    Maze maze = openMap();
    Probe entity(maze, tileCenterOf(1, 1), Direction::Right, 6.0f);

    CHECK(!entity.canMove(Direction::None));
}

PAC_TEST(Entity, reverseDoesNothingWithoutADirection) {
    Maze maze = openMap();
    Probe entity(maze, tileCenterOf(1, 1), Direction::None, 6.0f);

    entity.reverse();

    CHECK_EQ(entity.direction(), Direction::None);
}

PAC_TEST(Entity, hasNoDistanceToTheNextCenterWithoutADirection) {
    Maze maze = openMap();
    Probe entity(maze, tileCenterOf(1, 1), Direction::None, 6.0f);

    CHECK_NEAR(entity.toNextCenter(), 0.0, 1e-6);
}

PAC_TEST(Entity, doesNotSnapWithoutADirection) {
    Maze maze = openMap();
    const Vec2 offCenter{2.3f, 1.7f};
    Probe entity(maze, offCenter, Direction::None, 6.0f);

    entity.snap();

    CHECK(nearlyEquals(entity.position(), offCenter));
}

PAC_TEST(Entity, ignoresANonPositiveDistance) {
    Maze maze = openMap();
    Probe entity(maze, tileCenterOf(1, 1), Direction::Right, 6.0f);
    const Vec2 start = entity.position();

    entity.moveBy(0.0f);
    entity.moveBy(-1.0f);

    CHECK(nearlyEquals(entity.position(), start));
    CHECK_NEAR(entity.distanceTravelled(), 0.0, 1e-6);
}
