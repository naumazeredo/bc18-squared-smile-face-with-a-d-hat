// TODO: Verify which includes are needed
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <random>
#include <functional>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <tuple>
#include <set>
#include <queue>
#include <chrono>

#include "bc.hpp"


/*
  auto now = chrono::high_resolution_clock::now();
  auto end = chrono::high_resolution_clock::now();
  auto dur = chrono::duration_cast<chrono::microseconds>(end - now).count();
*/


using namespace bc;
using namespace std;

#define all(x) x.begin(), x.end()
#define debug() printf("line %d", __LINE__); fflush(stdout);

const int TOTAL_PLANETS    = 2;
const int TOTAL_UNIT_TYPES = 7;
const int TOTAL_ADJ_DIRECTIONS = 8; // Center is the last Direction

const unsigned FACTORY_MAX_HEALTH = 300;
const unsigned ROCKET_MAX_HEALTH  = 200;

const unsigned BUILD_HEALTH = 5;

const unsigned HEAT_REGEN = 10;


const vector<Direction> Directions         = { North, Northeast, East, Southeast, South, Southwest, West, Northwest, Center };
const vector<Direction> DirectionsAdjacent = { North, Northeast, East, Southeast, South, Southwest, West, Northwest };

const vector<UnitType> RobotTypes     = { Worker, Knight, Ranger, Mage, Healer };
const vector<UnitType> StructureTypes = { Factory, Rocket };

// Types for blueprints
// TODO: Continue UnitType enum values
enum {
  FactoryBlueprint = 0,
  RocketBlueprint  = 1,
  TOTAL_BLUEPRINTS = 2
};

// Constants to ease stuff for multiple unit types
using UnitTypeBitset = unsigned;

const int NoUnit     = 0;
const int WorkerBit  = (1<<Worker);
const int KnightBit  = (1<<Knight);
const int RangerBit  = (1<<Ranger);
const int MageBit    = (1<<Mage);
const int HealerBit  = (1<<Healer);
const int FactoryBit = (1<<Factory);
const int RocketBit  = (1<<Rocket);

const UnitTypeBitset Robots = WorkerBit | KnightBit | RangerBit | MageBit | HealerBit;
const UnitTypeBitset Structures = FactoryBit | RocketBit;

bool on_bitset(UnitType unit_type, UnitTypeBitset unit_type_bitset) { return (1 << unit_type) & unit_type_bitset; }

// 2D auxiliar structure
using Position = pair<int, int>;
const Position InvalidPosition = {-1, -1};

Position to_position(const MapLocation& map_location) { return { map_location.get_x(), map_location.get_y() }; }
MapLocation to_map_location(const Position position, Planet planet) {
  return MapLocation { planet, position.first, position.second };
}
unsigned calc_manhattan_distance(Position a, Position b) { return abs(a.first - b.first) + abs(a.second - b.second); }

using PlanetMatrix = vector<vector<pair<bool, unsigned>>>;


// --------------------------------------------------------------------------

class HiveMind {
public:
  HiveMind() :
      gc {},
         my_team { gc.get_team() },
         my_planet { gc.get_planet() },
         earth { gc.get_earth_map() },
         mars { gc.get_mars_map() },
         earth_initial { earth.get_initial_map() },
         mars_initial { mars.get_initial_map() }
  {
    planet_matrix = (my_planet == Earth) ? earth_initial : mars_initial;
  }

  void start() {
    printf("d=] starting! Get ready to be pwned!\n");

    printf("Team %d\n", my_team);

    for (auto unit : earth.get_initial_units()) {
      const auto map_loc = unit.get_location().get_map_location();
      printf("Unit %u from team %d at (%d, %d)\n",
             unit.get_id(), unit.get_team(),
             map_loc.get_x(), map_loc.get_y());
      fflush(stdout);
    }
  }

  void step() {
    get_round_data();

    printf("Round %d (%u)\n", current_round, my_karbonite);
    fflush(stdout);

    // Intel here
    run_actions();

    fflush(stdout);
    gc.next_turn();
  }

private:
  //
  struct Consts {
    static const unsigned Factories = 2;
  };


  // Actions
  enum Actions {
    BlueprintFactory,
    BlueprintRocket,
    BuildFactory,
    BuildRocket,
    /*
    BuildWorker,
    BuildKnight,
    BuildRanger,
    BuildMage,
    BuildHealer,
    Harvest,
    Replicate,
    */

    TotalActions
  };


  // Unit Action
  // IDEA: persistent unit action across round
  /*
  struct UnitAction {
    Actions action;

    unsigned unit_id;   // Unit doing the action
    unsigned target_id;
    MapLocation map_location;
  };
  */

  // HiveMind Action
  struct Action {
    const function<double()> priority; // Between 0 and 1 (0 = no priority, 1 = top priority)
    const function<void()> act;
  };

  // IMPORTANT: same order of actions
  const vector<Action> actions = {
    { // BlueprintFactory
      [this] {
        const auto factories = my_units[Factory].size();
        if (factories == 0) return 1.0;
        if (factories < Consts::Factories) return 0.2;
        return 0.0;
      },
      [this] {
        // Can only construct if we have workers
        if (my_idle_units[Worker].size() == 0) return;

        const auto best_position = calculate_best_build_position(Factory);
        // TODO
      }
    },
    { // BlueprintRocket
      [this] { return 0.0; },
      [this] {}
    },
    { // BuildFactory
      [this] {
        const auto factory_blueprints = my_blueprints[FactoryBlueprint].size();
        if (factory_blueprints > 0) return 1.0;
        return 0.0;
      },
      [this] {}
    },
    { // BuildRocket
      [this] { return 0.0; },
      [this] {}
    }
  };
  // ------------


  void run_actions() {
    vector<Actions> acts;
    for (int i = 0; i < Actions::TotalActions; i++)
      acts.push_back(static_cast<Actions>(i));
    sort(all(acts),
         [this](Actions a, Actions b) { return actions[a].priority() < actions[b].priority(); });

    for (auto act : acts)
      actions[act].act();
  }

  const PlanetMap& my_planet_map() const { return my_planet == Earth ? earth : mars; }
  const PlanetMatrix& my_planet_map_initial() const {
    return my_planet == Earth ? earth_initial : mars_initial;
  }

  void get_units() {
    // Update planet matrix
    // If any structure disappeared we have to set it's location passable
    for (auto unit_ : units) {
      auto unit = unit_.second;
      if (unit.is_structure() and !gc.has_unit(unit.get_id())) {
        // Unit not available anymore. Set terrain as passable on it's position
        auto map_loc = unit.get_location().get_map_location();
        planet_matrix[map_loc.get_y()][map_loc.get_x()] = { true, 0 };
      }
    }

    // Cleanup
    units.clear();
    for (int i = 0; i < TOTAL_UNIT_TYPES; i++) {
      my_units[i].clear();
      my_idle_units[i].clear();
    }
    for (int i = 0; i < TOTAL_BLUEPRINTS; i++)
      my_blueprints[i].clear();
    // ----

    auto all_units = gc.get_units();
    for (auto unit : all_units) {
      const auto id   = unit.get_id();
      const auto team = unit.get_team();
      const auto type = unit.get_unit_type();

      units[id] = unit;

      if (team == my_team) {
        my_idle_units[type].insert(id);
        my_units[type].insert(id);

        if (unit.is_structure() and !unit.structure_is_built())
          my_blueprints[type == Factory ? FactoryBlueprint : RocketBlueprint].insert(id);
      }
    }
  }

  void update_planet_matrix() {
    if (my_planet == Earth) {
      for (auto unit_ : units) {
        const auto& unit = unit_.second;

        if (unit.is_structure()) {
          auto map_location = unit.get_location().get_map_location();
          planet_matrix[map_location.get_y()][map_location.get_x()] = { false, 0 };
        }
      }
    } else {
      // TODO: Consider asteroid strikes to increment karbonite on terrain
    }
  }

  void get_round_data() {
    current_round = gc.get_round();
    my_karbonite = gc.get_karbonite();

    get_units();
    update_planet_matrix();
  }

  // Return numbers of rounds reach position and Direction of the next move to reach location
  pair<unsigned, Direction> calculate_move_to_position(unsigned unit_id, Position target_position) const {
    using Ret = pair<unsigned, Direction>;

    const auto& unit = units.at(unit_id);
    if (!unit.get_location().is_on_map())
      return { UINT_MAX, Center };

    const auto& planet_matrix = my_planet_map_initial();

    const auto unit_pos = to_position(unit.get_location().get_map_location());

    // Breadth-first search
    vector<vector<Ret>> matrix { my_planet_map().get_height(), vector<Ret> { my_planet_map().get_width(), { UINT_MAX, Center } } };

    using Info = tuple<unsigned, unsigned, Position>;
    auto f = calc_manhattan_distance;

    priority_queue<Info, vector<Info>, greater<Info>> q;

    q.push({ f(unit_pos, target_position), 0, unit_pos });
    matrix[unit_pos.second][unit_pos.first] = { 0 , Center };

    while (!q.empty()) {
      const auto u = q.top(); q.pop();
      const auto u_tot  = get<0>(u);
      const auto u_dist = get<1>(u);
      const auto u_pos  = get<2>(u);

      if (u_pos == target_position)
        break;

      const auto u_map_loc = to_map_location(u_pos, my_planet);
      for (auto direction : DirectionsAdjacent) {
        const auto v_map_loc = u_map_loc.add(direction);

        // TODO: use updated map
        if (!my_planet_map().is_on_map(v_map_loc) or
            !planet_matrix[v_map_loc.get_y()][v_map_loc.get_x()].first or
            gc.has_unit_at_location(v_map_loc))
          continue;

        auto v_dist = matrix[v_map_loc.get_y()][v_map_loc.get_x()].first;
        if (v_dist > u_dist + 1) {
          v_dist = u_dist + 1;
          matrix[v_map_loc.get_y()][v_map_loc.get_x()] = { v_dist, direction };
          q.push({
                   v_dist + f(to_position(v_map_loc), target_position),
                   v_dist,
                   to_position(v_map_loc)
                 });
        }
      }
    }

    const auto move_cd = unit.get_movement_cooldown();
    const auto unit_heat = unit.get_movement_heat();
    const auto rounds = (unit_heat + matrix[target_position.second][target_position.first].first * move_cd) / HEAT_REGEN;

    // Backtrack
    auto dir = Center;
    auto pos = to_map_location(target_position, my_planet);
    while (matrix[pos.get_y()][pos.get_x()].second != Center) {
      dir = matrix[pos.get_y()][pos.get_x()].second;
      pos = pos.subtract(dir);
    }

    return { rounds, dir };
  }

  // Return the units that reach earlier on position
  // { unit id, rounds to reach, Direction of the next move to reach position }
  vector<tuple<unsigned, unsigned, Direction>> get_nearest_units(Position position, UnitTypeBitset bitset, unsigned max_units = UINT_MAX, bool only_idle = false) const {
    using Ret = tuple<unsigned, unsigned, Direction>;
    set<Ret, bool(*)(Ret, Ret)> candidates ( [](Ret a, Ret b) { return get<1>(a) < get<1>(b); } );

    const auto& unit_list = only_idle ? my_idle_units : my_units;
    for (auto unit_type : RobotTypes) if (on_bitset(unit_type, bitset)) {
      for (auto unit_id : my_units[unit_type]) {
        const auto& unit = units.at(unit_id);

        const auto move_info = calculate_move_to_position(unit_id, position);

        candidates.insert({ unit_id, move_info.first, move_info.second });

        if (candidates.size() > max_units)
          candidates.erase(candidates.end());
      }
    }

    return vector<Ret>( candidates.begin(), candidates.end() );
  }

  Position calculate_best_build_position(UnitType structure_type) const {
    assert(my_planet == Earth);

    // TODO: Consider available space around structure
    // TODO: Consider distance to other structures

    // { rounds to build, karbonite at location, Position }
    tuple<unsigned, unsigned, Position> best = { UINT_MAX, UINT_MAX, InvalidPosition };

    if (my_idle_units[Worker].size() == 0)
      return get<2>(best);

    for (int y = 0; y < earth_initial.size(); y++) {
      for (int x = 0; x < earth_initial[0].size(); x++) {
        if (!earth_initial[y][x].first or
            gc.has_unit_at_location({ Earth, x, y }))
          continue;

        vector<unsigned> rounds_workers_to_reach;
        rounds_workers_to_reach.push_back(2000);

        auto workers_near = get_nearest_units({ x, y }, WorkerBit, 4, true);
        for (auto worker : workers_near) {
          const auto rounds = get<1>(worker);
          if (rounds != UINT_MAX)
            rounds_workers_to_reach.push_back(rounds);
        }

        sort(all(rounds_workers_to_reach));

        // Iterate on rounds workers reach the building to estimate the round it will get build
        const auto max_health    = (structure_type == Factory) ? FACTORY_MAX_HEALTH : ROCKET_MAX_HEALTH;
        unsigned current_health  = max_health / 4;
        unsigned workers_at_work = 0;
        unsigned rounds_taken    = 0;

        for (auto round : rounds_workers_to_reach) {
          const auto health_add = workers_at_work * BUILD_HEALTH * (round - rounds_taken);

          if (current_health + health_add >= max_health) {
            rounds_taken = rounds_taken + 1 + (max_health - current_health - 1)/(workers_at_work * BUILD_HEALTH); // ceiling logic
            break;
          }

          current_health += health_add;
          workers_at_work += 1;
          rounds_taken = round;
        }

        // TODO: use updated map
        if (rounds_taken < get<0>(best) or (rounds_taken == get<0>(best) and earth_initial[y][x].second < get<1>(best))) {
          best = { rounds_taken, earth_initial[y][x].second, { x, y } };
        }

      }
    }

    printf("Best build position: (%d, %d) in %u rounds, losing %u karb.\n",
           get<2>(best).first, get<2>(best).second, get<0>(best), get<1>(best));

    return get<2>(best);
  }

  // TODO: bool build_at(const MapLocation&, UnitType) const; // assign workers to build at location
  void build_at(Position build_position, UnitType structure_type) {
    assert(my_planet == Earth);

    const auto map_location = to_map_location(build_position, Earth);
    if (gc.has_unit_at_location(map_location)) {
      auto unit = gc.sense_unit_at_location(map_location);

      if (unit.is_structure()) {
        if (unit.structure_is_built()) {
          printf("Can't build: (%d, %d). Structure on position!\n", map_location.get_x(), map_location.get_y());
          return;
        } else {
          // Build
          auto workers = get_nearest_units(build_position, WorkerBit, 1, true);
          auto worker = units.at(get<0>(workers[0]));
        }
      }
    } else {
      // Blueprint
      auto workers = get_nearest_units(build_position, WorkerBit, 1, true);
      if (workers.empty()) {
        printf("Can't build: (%d, %d). No idle workers!\n", map_location.get_x(), map_location.get_y());
        return;
      }

      const auto& worker = units.at(get<0>(workers[0]));
      if (worker.get_location().is_adjacent_to(map_location)) {
        const auto direction = worker.get_location().get_map_location().direction_to(map_location);

        if (gc.can_blueprint(worker.get_id(), structure_type, direction)) {
          gc.blueprint(worker.get_id(), structure_type, direction);
          set_unit_acted(worker.get_id());
        } else {
          printf("Can't build: (%d, %d). can_blueprint == false!\n", map_location.get_x(), map_location.get_y());
          return;
        }
      }
    }

    // If built -> error
    // Move units to adjacent
  }

  void set_unit_acted(unsigned unit_id) {
    const auto& unit = units.at(unit_id);
    my_idle_units[unit.get_unit_type()].erase(unit_id);
  }

  // TODO: void move_unit(unsigned unit_id, Position target_position, bool to_adjacent = false) const;


  // -------------------------------

  // Attributes

  // Game code
  GameController gc;

  // Constant for whole game
  const Team         my_team;
  const Planet       my_planet;
  const PlanetMap&   earth;
  const PlanetMap&   mars;
  const PlanetMatrix earth_initial;
  const PlanetMatrix mars_initial;

  // Changed every round
  unsigned current_round;
  unsigned my_karbonite;

  PlanetMatrix planet_matrix; // current planet, using vision info and units information

  unordered_map<unsigned, Unit> units;
  unordered_set<unsigned> my_units[TOTAL_UNIT_TYPES];
  unordered_set<unsigned> my_blueprints[TOTAL_BLUEPRINTS];

  // TODO: Optimize this! idleness for move and action?
  unordered_set<unsigned> my_idle_units[TOTAL_UNIT_TYPES];

  // ---------

  // Actions


  map<Actions, int> action_map;
};


int main() {

  HiveMind bot;
  bot.start();

  while (true)
    bot.step();
}
