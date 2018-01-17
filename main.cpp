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
#include <iterator>

#include "bc.hpp"

#include "hash.h"


/*
  // Debug time!
  auto now = chrono::high_resolution_clock::now();
  auto end = chrono::high_resolution_clock::now();
  auto dur = chrono::duration_cast<chrono::microseconds>(end - now).count();
*/

// IDEA: Store the maximum number of units seen for the last 30 rounds. Might be useful for action priorities

// TODO: Change pair<...> to tuple<...> for standardization

using namespace bc;
using namespace std;

#define all(x) x.begin(), x.end()
#define debug() printf("line %d\n", __LINE__); fflush(stdout);

const int TOTAL_PLANETS    = 2;
const int TOTAL_UNIT_TYPES = 7;
const int TOTAL_ADJ_DIRECTIONS = 8; // Center is the last Direction

const unsigned FACTORY_MAX_HEALTH = 300;
const unsigned ROCKET_MAX_HEALTH  = 200;

const unsigned BUILD_HEALTH = 5;

const unsigned HEAT_REGEN = 10;

const unsigned FACTORY_COST   = 100;
const unsigned REPLICATE_COST = 15;


const vector<Direction> Directions         = { North, East, South, West, Northeast, Southeast, Southwest, Northwest, Center };
const vector<Direction> DirectionsAdjacent = { North, East, South, West, Northeast, Southeast, Southwest, Northwest };

const vector<UnitType> RobotTypes     = { Worker, Knight, Ranger, Mage, Healer };
const vector<UnitType> StructureTypes = { Factory, Rocket };
const vector<UnitType> AllTypes       = { Worker, Knight, Ranger, Mage, Healer, Factory, Rocket };

// Types for blueprints
// TODO: Continue UnitType enum values
enum {
  FactoryBlueprint = 0,
  RocketBlueprint  = 1,
  TOTAL_BLUEPRINTS = 2
};

int to_blueprint_type(UnitType type) { return (type == Factory) ? FactoryBlueprint : RocketBlueprint; }

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

const UnitTypeBitset RobotBits     = WorkerBit | KnightBit | RangerBit | MageBit | HealerBit;
const UnitTypeBitset StructureBits = FactoryBit | RocketBit;
const UnitTypeBitset AllUnitBits   = RobotBits | StructureBits;

bool on_bitset(UnitType unit_type, UnitTypeBitset unit_type_bitset) { return (1 << unit_type) & unit_type_bitset; }

// 2D auxiliar structure
using Position = pair<int, int>;
const Position InvalidPosition = {-1, -1};

Position to_position(const MapLocation& map_location) { return { map_location.get_x(), map_location.get_y() }; }
MapLocation to_map_location(const Position position, Planet planet) {
  return MapLocation { planet, position.first, position.second };
}
unsigned calculate_distance(Position a, Position b) { return abs(a.first - b.first) + abs(a.second - b.second); }

using PlanetMatrix = vector<vector<pair<bool, unsigned>>>;


// Auxiliar priority
double inverse_linear_ramp(double min, double max, double f) { return max - (max - min) * f; }

// --------------------------------------------------------------------------

class HiveMind {
public:
  HiveMind() :
      gc {},
         my_team { gc.get_team() },
         enemy_team { other_team(my_team) },
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
      if (unit.get_team() != my_team) {
        const auto position = to_position(unit.get_map_location());
        enemy_base_candidates.insert(position);
      }

      /*
      printf("Unit %u from team %d at (%d, %d)\n",
             unit.get_id(), unit.get_team(),
             unit.get_map_location().get_x(),
             unit.get_map_location().get_y());
      fflush(stdout);
      */
    }

    //printf("Time left: %u\n", gc.get_time_left_ms());

    // XXX: Using fixed research queue
    gc.queue_research(Worker);
    gc.queue_research(Knight);
    gc.queue_research(Rocket);
    gc.queue_research(Knight);
    gc.queue_research(Knight);
    gc.queue_research(Ranger);

    // Unassigned
    gc.queue_research(Worker);
    gc.queue_research(Worker);
    gc.queue_research(Worker);

    gc.queue_research(Ranger);
    gc.queue_research(Ranger);

    gc.queue_research(Mage);
    gc.queue_research(Mage);
    gc.queue_research(Mage);
    gc.queue_research(Mage);

    gc.queue_research(Healer);
    gc.queue_research(Healer);
    gc.queue_research(Healer);

    gc.queue_research(Rocket);
    gc.queue_research(Rocket);
  }

  void step() {
    get_round_data();

    printf("Round %d (%u)\n", current_round, my_karbonite);
    //printf("Research rounds left %u\n", gc.get_research_info().rounds_left());

    // Intel here
    run_actions();

    fflush(stdout);
    gc.next_turn();
  }

private:
  //
  struct Consts {
    static const unsigned Factories = 6;
    static const unsigned Knights   = 20;
    static const unsigned Rangers   = 12;
    static const unsigned Workers   = 6;

    static const unsigned AttackSearchDistance = 8;
  };


  // Actions
  enum Actions {
    BlueprintFactory,
    BlueprintRocket,
    BuildFactory,
    BuildRocket,
    BuildWorker,
    BuildKnight,
    /*
    BuildRanger,
    BuildMage,
    BuildHealer,
    Harvest,
    */
    Replicate,
    UnloadGarrison,
    Attack, // XXX: Change to Explore?

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
    const function<double()> priority; // Between 0 and 1 (0 = no priority, 1 = top priority), -1 if disabled
    const function<void()> act;
  };

  // IMPORTANT: same order of actions
  const vector<Action> actions = {
    { // BlueprintFactory
      [this] {
        const auto factories = my_unit_ids[Factory].size();
        return inverse_linear_ramp(0.0, 1.0, factories / static_cast<double>(Consts::Factories));
      },
      [this] {
        printf("-- BlueprintFactory --\n");
        printf("Idle workers: %lu\n", my_idle_unit_ids[Worker].size());

        // Can only construct if we have workers
        if (my_idle_unit_ids[Worker].size() == 0)
          return;

        const auto best_position = calculate_best_blueprint_position(Factory);

        build_at(best_position, Factory);

        // Readd the action on queue
        //push_action(BlueprintFactory);
      }
    },
    { // BlueprintRocket
      [this] { return -1.0; },
      [this] {}
    },
    { // BuildFactory
      [this] {
        const auto factory_blueprints = my_blueprint_ids[FactoryBlueprint].size();
        const auto factories = my_unit_ids[Factory].size();

        if (factory_blueprints == 1)
          return 1.0;
        else if (factory_blueprints > 0)
          return inverse_linear_ramp(0.2, 0.8, factory_blueprints / static_cast<double>(factories));

        return -1.0;
      },
      [this] {
        printf("-- BuildFactory --\n");
        printf("Idle workers: %lu\n", my_idle_unit_ids[Worker].size());

        // Can only construct if we have workers
        if (my_idle_unit_ids[Worker].size() == 0)
          return;

        const auto& factory_blueprints = my_blueprint_ids[FactoryBlueprint];
        vector<unsigned> blueprints { factory_blueprints.begin(), factory_blueprints.end() };

        sort(all(blueprints),
             [this](unsigned a, unsigned b) {
               const auto fa = gc.get_unit(a), fb = gc.get_unit(b);
               const auto pa = to_position(fa.get_map_location()),
                          pb = to_position(fb.get_map_location());
               return calculate_rounds_to_build(pa, Factory, fa.get_health()) <
                      calculate_rounds_to_build(pb, Factory, fb.get_health());
             });

        const auto& unit = gc.get_unit(blueprints[0]);
        const auto pos = to_position(unit.get_map_location());
        build_at(pos, Factory);

        // Readd the action on queue
        //push_action(BuildFactory);
      }
    },
    { // BuildRocket
      [this] { return -1.0; },
      [this] {}
    },
    { // BuildWorker
      [this] { return -1.0; },
      [this] {}
    },
    { // BuildKnight
      [this] {
        // TODO: Consider size of the enemy army (max visible attack units?)
        // TODO: Don't hardcode total number of knights

        const auto total_factories = my_unit_ids[Factory].size();
        if (total_factories == 0) return -1.0; // No factories to build

        const auto total_knights = my_unit_ids[Knight].size();
        if (total_knights < Consts::Knights)
          return inverse_linear_ramp(0.0, 0.8, total_knights / static_cast<double>(Consts::Knights));
        return -1.0;
      },
      [this] {
        const auto factories = my_unit_ids[Factory];

        for (auto factory_id : factories) {
          const auto& factory = gc.get_unit(factory_id);
          if (gc.can_produce_robot(factory_id, Knight)) {
            printf("Factory %u producing Knight!\n", factory_id);
            gc.produce_robot(factory_id, Knight);
          }
        }
      }
    },
    { // Replicate
      [this] {
        // Only replicate after having money (or after blueprint a factory)
        const auto factories = my_unit_ids[Factory].size();
        if (factories == 0 and my_karbonite < FACTORY_COST + REPLICATE_COST)
          return -1.0;

        const auto workers = my_unit_ids[Worker].size();
        return inverse_linear_ramp(0.0, 0.9, workers / static_cast<double>(Consts::Workers));
      },
      [this] {
        printf("-- Replicating --\n");
        printf("Workers/Idle workers: %lu/%lu\n",
               my_unit_ids[Worker].size(), my_idle_unit_ids[Worker].size());

        const auto worker_ids = my_idle_unit_ids[Worker];
        if (worker_ids.size() == 0)
          return;

        for (auto worker_id : worker_ids) {
          for (auto dir : DirectionsAdjacent) {
            if (gc.can_replicate(worker_id, dir)) {
              replicate_worker(worker_id, dir);

              push_action(Replicate);
              return;
            }
          }
        }
      }
    },
    { // UnloadGarrison
      [this] {
        // TODO: Change this?
        return 0.9;
      }, // High priority to release units before other actions
      [this] {
        const auto factories = my_unit_ids[Factory];

        for (auto factory_id : factories) {
          const auto& factory = gc.get_unit(factory_id);
          const auto garrison = factory.get_structure_garrison();

          while (garrison.size() > 0) {
            bool can_unload = true;
            for (auto dir : Directions) {
              if (dir == Center) {
                can_unload = false;
                break;
              }

              if (gc.can_unload(factory_id, dir)) {
                printf("Factory %u unloading!\n", factory_id);
                gc.unload(factory_id, dir);
              }
            }

            if (!can_unload) break;
          }
        }
      }
    },
    // XXX: Remove this and have better logic for attacking
    { // Attack
      [this] {
        if (my_idle_unit_ids[Knight].size() == 0)
          return -1.0;
        return 0.2;
      },
      [this] {
        const auto offensive_unit_ids = get_units(my_team, KnightBit | RangerBit, true);

        if (offensive_unit_ids.size() == 0)
          return;

        for (auto unit_id : offensive_unit_ids) {
          const auto unit = gc.get_unit(unit_id);

          if (!unit.is_on_map())
            continue;

          const auto unit_pos = to_position(unit.get_map_location());

          //const auto enemy_id_list = get_enemy_nearest_units(unit_pos, AllUnitBits, 1, true);
          const auto enemy_id_list = get_nearest_units(
            enemy_team,
            unit_pos,
            AllUnitBits,
            1
          );

          if (enemy_id_list.size() > 0 and get<1>(enemy_id_list[0]) < Consts::AttackSearchDistance) {
            // Try to attack the unit
            const auto enemy_id = get<0>(enemy_id_list[0]);
            const auto dir      = get<2>(enemy_id_list[0]); // dir pos -> unit

            move_unit(unit_id, dir);
            attack_unit(unit_id, enemy_id);
          } else {
            // No units nearby, go to the base direction
            //tuple<Position, unsigned, Direction> calculate_enemy_nearest_base(Position pos)
            const auto base = calculate_enemy_nearest_base(unit_pos);
            const auto target_dir = get<2>(base);
            if (target_dir != Center) {
              move_unit(unit_id, target_dir);
            } else {
              // No base to go after. Would be good to explore!
            }
          }
        }
      }
    }

  };
  // ------------

  void push_action(Actions action) {
    const auto priority = actions[action].priority();
    if (priority >= 0.0)
      action_queue.push({ priority, action });
  }

  void run_actions() {
    if (my_planet == Mars) return;

    for (int i = 0; i < Actions::TotalActions; i++)
      push_action(static_cast<Actions>(i));

    while (!action_queue.empty()) {
      const auto action = action_queue.top();
      action_queue.pop();
      actions[action.second].act();
    }
  }

  const PlanetMap& my_planet_map() const { return my_planet == Earth ? earth : mars; }
  const PlanetMatrix& my_planet_map_initial() const {
    return my_planet == Earth ? earth_initial : mars_initial;
  }

  void get_units_data() {
    // Cleanup
    unit_ids.clear();
    for (int i = 0; i < TOTAL_UNIT_TYPES; i++) {
      enemy_unit_ids[i].clear();
      my_unit_ids[i].clear();
      my_idle_unit_ids[i].clear();
    }
    for (int i = 0; i < TOTAL_BLUEPRINTS; i++)
      my_blueprint_ids[i].clear();
    // ----

    auto all_units = gc.get_units();
    for (auto unit : all_units) {
      const auto id   = unit.get_id();
      const auto team = unit.get_team();
      const auto type = unit.get_unit_type();

      unit_ids.insert(unit.get_id());

      if (team == my_team) {
        my_unit_ids[type].insert(id);

        // FIXME: units that can't move are considered as idle too
        my_idle_unit_ids[type].insert(id);

        if (unit.is_structure() and !unit.structure_is_built())
          my_blueprint_ids[type == Factory ? FactoryBlueprint : RocketBlueprint].insert(id);
      } else {
        enemy_unit_ids[type].insert(id);
      }
    }
  }

  void update_planet_matrix() {
    if (my_planet == Earth) {
      for (auto unit_id : unit_ids) {
        const auto& unit = gc.get_unit(unit_id);

        if (unit.is_structure()) {
          auto map_location = unit.get_map_location();
          planet_matrix[map_location.get_y()][map_location.get_x()] = { true, 0 };
        }
      }
    } else {
      // TODO: Consider asteroid strikes to increment karbonite on terrain
    }

    // Remove enemy base candidates if in range and there's no structure at the location
    vector<Position> enemy_base_candidates_to_remove;
    for (auto base_candidate : enemy_base_candidates) {
      const auto map_loc = to_map_location(base_candidate, my_planet);

      if (gc.can_sense_location(map_loc)) {
        auto remove = false;

        if (!gc.has_unit_at_location(map_loc) or !gc.sense_unit_at_location(map_loc).is_structure())
          enemy_base_candidates_to_remove.push_back(base_candidate);
      }
    }

    // Add every enemy structure to the enemy base candidates
    for (int unit_type = Factory; unit_type <= Rocket; unit_type++) {
      for (auto unit_id : enemy_unit_ids[unit_type]) {
        const auto unit = gc.get_unit(unit_id);
        if (unit.is_structure())
          enemy_base_candidates.insert(to_position(unit.get_map_location()));
      }
    }
  }

  void get_round_data() {
    current_round = gc.get_round();
    my_karbonite = gc.get_karbonite();

    get_units_data();
    update_planet_matrix();
  }

  // { distance, Direction pos -> target, Direction target -> pos }
  tuple<unsigned, Direction, Direction>
  calculate_move_to_position(Position pos, Position target_pos) const {
    using Ret = tuple<unsigned, Direction, Direction>;
    const auto& planet_matrix = my_planet_map_initial();

    // A-star
    using MatInfo = tuple<unsigned, Direction>;
    vector<vector<MatInfo>> matrix { my_planet_map().get_height(), vector<MatInfo> { my_planet_map().get_width(), { UINT_MAX, Center } } };

    using Info = tuple<unsigned, Position>;
    auto f = calculate_distance;

    priority_queue<Info, vector<Info>, greater<Info>> q;

    q.push({ f(pos, target_pos), pos });
    matrix[pos.second][pos.first] = { 0 , Center };

    while (!q.empty()) {
      const auto u = q.top(); q.pop();
      const auto u_pos  = get<1>(u);
      const auto u_map_loc = to_map_location(u_pos, my_planet);

      const auto u_dist = get<0>(matrix[u_pos.second][u_pos.first]);

      if (u_pos == target_pos) {
        // Update target_pos (required if it's adjacent)
        target_pos = u_pos;
        break;
      }

      if (gc.has_unit_at_location(u_map_loc) and
          u_pos != pos and u_pos != target_pos)
        continue;

      for (auto direction : DirectionsAdjacent) {
        const auto v_map_loc = u_map_loc.add(direction);

        // TODO: use updated map
        if (!my_planet_map().is_on_map(v_map_loc) or
            !planet_matrix[v_map_loc.get_y()][v_map_loc.get_x()].first)
          continue;

        auto v_dist = get<0>(matrix[v_map_loc.get_y()][v_map_loc.get_x()]);
        if (v_dist > u_dist + 1) {
          v_dist = u_dist + 1;
          matrix[v_map_loc.get_y()][v_map_loc.get_x()] = { v_dist, direction };
          q.push({
                   v_dist + f(to_position(v_map_loc), target_pos),
                   to_position(v_map_loc)
                 });
        }
      }
    }

    const auto distance = get<0>(matrix[target_pos.second][target_pos.first]);
    const auto dir_target_pos = direction_opposite(get<1>(matrix[target_pos.second][target_pos.first]));
    const auto dir_pos_target = [&] {
      auto dir = Center;
      auto pos = to_map_location(target_pos, my_planet);
      while (get<1>(matrix[pos.get_y()][pos.get_x()]) != Center) {
        dir = get<1>(matrix[pos.get_y()][pos.get_x()]);
        pos = pos.subtract(dir);
      }
      return dir;
    }();

    return { distance, dir_pos_target, dir_target_pos };
  }

  // TODO: DRY... get_nearest_<.> all have the same structure

  // Return the units that reach earlier on position
  // { unit id, rounds to reach, Direction of the next move to reach position }
  // TODO: remove to_adj

  /*
  vector<tuple<unsigned, unsigned, Direction>> get_my_nearest_robots(Position position, UnitTypeBitset bitset, unsigned max_units = UINT_MAX, bool only_idle = false, bool to_adj = false) const {
    using Ret = tuple<unsigned, unsigned, Direction>;
    set<Ret, bool(*)(Ret, Ret)> candidates ( [](Ret a, Ret b) { return get<1>(a) < get<1>(b); } );

    const auto& unit_list = only_idle ? my_idle_unit_ids : my_unit_ids;
    for (auto unit_type : RobotTypes) if (on_bitset(unit_type, bitset)) {
      for (auto unit_id : unit_list[unit_type]) {
        const auto& unit = gc.get_unit(unit_id);
        if (!unit.is_on_map())
          continue;

        const auto move_info = calculate_move_to_position(to_position(unit.get_map_location()), position, to_adj);

        const auto unit_heat = unit.get_movement_heat();
        const auto unit_move_cd = unit.get_movement_cooldown();
        const auto rounds = to_rounds(move_info.first, unit_heat, unit_move_cd);

        candidates.insert({ unit_id, rounds, move_info.second });

        if (candidates.size() > max_units)
          candidates.erase(candidates.end());
      }
    }

    return vector<Ret>( candidates.begin(), candidates.end() );
  }


  // Return the units that reach earlier on position
  // { unit id, distance, Direction of the next move to reach the enemy }
  // IMPORTANT: Direction of the next move is different from get_my_nearest_robots!
  // TODO: remove to_adj
  vector<tuple<unsigned, unsigned, Direction>> get_enemy_nearest_units(Position position, UnitTypeBitset bitset, unsigned max_units = UINT_MAX, bool to_adj = false) const {
    using Ret = tuple<unsigned, unsigned, Direction>;

    if (max_units == 0)
      return vector<Ret>();

    set<Ret, bool(*)(Ret, Ret)> candidates ( [](Ret a, Ret b) { return get<1>(a) < get<1>(b); } );

    const auto& unit_list = enemy_unit_ids;
    for (auto unit_type : AllTypes) if (on_bitset(unit_type, bitset)) {
      for (auto unit_id : unit_list[unit_type]) {
        const auto& unit = gc.get_unit(unit_id);
        if (!unit.is_on_map())
          continue;

        const auto move_info = calculate_move_to_position(position, to_position(unit.get_map_location()), to_adj);
        candidates.insert({ unit_id, move_info.first, move_info.second });

        if (candidates.size() > max_units)
          candidates.erase(prev(candidates.end()));
      }
    }

    return vector<Ret>( candidates.begin(), candidates.end() );
  }
  */

  // { unit id, distance, Direction position -> unit, Direction unit -> position }
  /*
  vector<tuple<unsigned, unsigned, Direction, Direction>>
  get_nearest_units(Team team,
                    Position position,
                    UnitTypeBitset bitset = AllUnitBits,
                    unsigned max_units    = UINT_MAX,
                    bool idle_units       = false) const {
    using Ret = tuple<unsigned, unsigned, Direction, Direction>;

    const auto& unit_list = [=]() {
      if (team == my_team) return idle_units ? my_idle_unit_ids : my_unit_ids;
      else return enemy_unit_ids;
    }();

    vector<Ret> nearest_units;

    // Breadth-First Search

    // Recovery matrix { distance, direction cell -> initial position }
    using MatInfo = tuple<unsigned, Direction>;
    vector<vector<MatInfo>> matrix { my_planet_map().get_height(), vector<MatInfo> { my_planet_map().get_width(), { UINT_MAX, Center } } };

    queue<Position> q;

    q.push(position);
    matrix[position.second][position.first] = { 0, Center };

    while (!q.empty() and nearest_units.size() < max_units) {
      const auto u_pos  = q.front();
      const auto u_dist = get<0>(matrix[u_pos.second][u_pos.first]);
      q.pop();

      const auto u_map_loc = to_map_location(u_pos, my_planet);

      if (gc.has_unit_at_location(u_map_loc)) {
        const auto unit = gc.sense_unit_at_location(u_map_loc);
        if (unit.get_team() == team and on_bitset(unit.get_unit_type(), bitset)) {
          const auto dir_unit_pos = get<1>(matrix[u_pos.second][u_pos.first]);

          const auto dir_pos_unit = [&] {
            auto dir = Center;
            auto pos = u_map_loc;
            while (get<1>(matrix[pos.get_y()][pos.get_x()]) != Center) {
              dir = get<1>(matrix[pos.get_y()][pos.get_x()]);
              pos = pos.add(dir);
            }
            return direction_opposite(dir);
          }();

          nearest_units.push_back({ unit.get_id(), u_dist, dir_pos_unit, dir_unit_pos });
        } else {
          if (u_pos != position)
            continue; // Don't ignore other units
        }
      }

      for (auto direction : DirectionsAdjacent) {
        const auto v_map_loc = u_map_loc.subtract(direction);

        if (!my_planet_map().is_on_map(v_map_loc) or
            !my_planet_map().is_passable_terrain_at(v_map_loc))
          continue;

        auto v_dist = get<0>(matrix[v_map_loc.get_y()][v_map_loc.get_x()]);
        if (v_dist > u_dist + 1) {
          v_dist = u_dist + 1;
          matrix[v_map_loc.get_y()][v_map_loc.get_x()] = { v_dist, direction };
          q.push(to_position(v_map_loc));
        }
      }
    }

    return nearest_units;
  }
  */
  vector<tuple<unsigned, unsigned, Direction, Direction>>
  get_nearest_units(Team team,
                    Position position,
                    UnitTypeBitset bitset = AllUnitBits,
                    unsigned max_units    = UINT_MAX,
                    unsigned max_distance = 10,
                    bool idle_units       = false) const {
    using Ret = tuple<unsigned, unsigned, Direction, Direction>;

    if (max_units == 0)
      return vector<Ret>();

    set<Ret, bool(*)(Ret, Ret)> candidates ( [](Ret a, Ret b) { return get<1>(a) < get<1>(b); } );

    const auto nearby_unit_ids = get_nearby_units(team, position, bitset, max_distance, idle_units);

    for (auto unit_id : nearby_unit_ids) {
      const auto& unit = gc.get_unit(unit_id);

      const auto move_info = calculate_move_to_position(position, to_position(unit.get_map_location()));
      candidates.insert({ unit_id, get<0>(move_info), get<1>(move_info), get<2>(move_info) });

      if (candidates.size() > max_units)
        candidates.erase(prev(candidates.end()));
    }
    fflush(stdout);

    return vector<Ret>( candidates.begin(), candidates.end() );
  }

  vector<unsigned>
  get_nearby_units(Team team,
                   Position position,
                   UnitTypeBitset bitset = AllUnitBits,
                   unsigned max_distance = 10,
                   bool idle_units = false) const {
    vector<unsigned> nearby_unit_ids;

    const auto& unit_list = [=] {
      if (team == my_team) return idle_units ? my_idle_unit_ids : my_unit_ids;
      return enemy_unit_ids;
    }();

    for (auto unit_type : AllTypes) if (on_bitset(unit_type, bitset)) {
      for (auto unit_id : unit_list[unit_type]) {
        const auto unit = gc.get_unit(unit_id);
        if (!unit.is_on_map())
          continue;

        if (calculate_distance(to_position(unit.get_map_location()), position) <= max_distance)
          nearby_unit_ids.push_back(unit_id);
      }
    }

    return nearby_unit_ids;
  }

  // TODO: generalize for multiple results like get_enemy_nearest_robots?
  // Calculate the nearest enemy base from the position given
  tuple<Position, unsigned, Direction> calculate_enemy_nearest_base(Position pos) {
    using Ret = tuple<Position, unsigned, Direction>;
    Ret ans = { InvalidPosition, UINT_MAX, Center };
    for (auto enemy_pos : enemy_base_candidates) {
      const auto move = calculate_move_to_position(pos, enemy_pos);
      if (get<0>(move) < get<1>(ans)) {
        ans = { pos, get<0>(move), get<1>(move) };
      }
    }

    return ans;
  }

  // TODO: change return to tuple<unsigned, vector<unsigned, Direction>> to return the worker ids that can help building
  unsigned
  calculate_rounds_to_build(
    Position position,
    UnitType structure_type,
    unsigned cur_health = UINT_MAX
  ) const {
    const auto x = position.first;
    const auto y = position.second;

    vector<unsigned> rounds_workers_to_reach;
    rounds_workers_to_reach.push_back(2000);

    //auto workers_near = get_my_nearest_robots({ x, y }, WorkerBit, 4, true, true);
    const auto workers_near = get_nearest_units(
      my_team,
      { x, y },
      WorkerBit,
      8,
      10,
      true
    );

    for (auto worker : workers_near) {
      const auto worker_id = get<0>(worker);

      // distance to position -> distance to adj -> act on same round that moves
      const auto dist = to_adjacent_minus(get<1>(worker));

      const auto rounds = to_rounds(dist, get<0>(worker));
      if (rounds != UINT_MAX) {
        rounds_workers_to_reach.push_back(rounds);
      }
    }

    sort(all(rounds_workers_to_reach));

    // Iterate on rounds workers reach the building to estimate the round it will get build
    const auto max_health    = (structure_type == Factory) ? FACTORY_MAX_HEALTH : ROCKET_MAX_HEALTH;

    /*
    // XXX: get current building health
    unsigned current_health  = [=] {
      const auto map_loc = to_map_location(position, my_planet);
      if (gc.can_sense_location(map_loc) and
          gc.has_unit_at_location(map_loc) and
          gc.sense_unit_at_location(map_loc).is_structure() and
          !gc.sense_unit_at_location(map_loc).structure_is_built())
        return gc.sense_unit_at_location(map_loc).get_health();
      return max_health / 4;
    };
    */

    unsigned current_health  = (cur_health == UINT_MAX) ? max_health / 4 : cur_health;
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

    return rounds_taken;
  }

  // TODO: Change return to tuple<Position, vector<unsigned, Direction>>
  Position calculate_best_blueprint_position(UnitType structure_type) const {
    assert(my_planet == Earth);

    // TODO: Consider available space around structure
    // TODO: Consider distance to other structures

    // { rounds to build, karbonite at location, Position }
    tuple<unsigned, unsigned, Position> best = { UINT_MAX, UINT_MAX, InvalidPosition };

    if (my_idle_unit_ids[Worker].size() == 0)
      return get<2>(best);

    for (int y = 0; y < earth_initial.size(); y++) {
      for (int x = 0; x < earth_initial[0].size(); x++) {
        const auto map_loc = MapLocation { Earth, x, y };
        if (!earth_initial[y][x].first or
            gc.has_unit_at_location(map_loc))
          continue;

        const auto rounds_taken = calculate_rounds_to_build({ x, y }, structure_type);

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

  // Assign workers to build at position
  void build_at(Position build_position, UnitType structure_type) {
    assert(my_planet == Earth);

    // TODO: change to build(UnitType)
    //       if there are blueprints of the type, focus on building them
    //       otherwise calculate best position to build

    if (build_position == InvalidPosition) {
      printf("Can't build: (%d, %d). InvalidPosition!\n", build_position.first, build_position.second);
      return;
    }

    printf("Build at (%d, %d)\n", build_position.first, build_position.second);

    const auto map_location = to_map_location(build_position, Earth);
    if (gc.has_unit_at_location(map_location)) {
      auto unit = gc.sense_unit_at_location(map_location);

      if (unit.is_structure()) {
        if (unit.structure_is_built()) {
          printf("Can't build: (%d, %d). Structure on position!\n", map_location.get_x(), map_location.get_y());
          return;
        } else {
          // Build
          // TODO: move only workers that help
          //auto workers = get_my_nearest_robots(build_position, WorkerBit, 4, true, true);
          auto workers_info = get_nearest_units(
            my_team,
            build_position,
            WorkerBit,
            8,
            10,
            true
          );

          printf("BUILD\n");
          printf("Workers assigned: %lu\n", workers_info.size());
          for (auto worker_info : workers_info)
            printf("(%u, %u, %d, %d)\n",
                   get<0>(worker_info), get<1>(worker_info),
                   get<2>(worker_info), get<3>(worker_info));
          fflush(stdout);

          // Move to structure and try to build if adjacent
          for (auto worker_info : workers_info) {
            const auto worker_id = get<0>(worker_info);
            const auto dir = get<3>(worker_info); // dir unit -> pos

            move_unit(worker_id, dir);
            const auto worker = gc.get_unit(worker_id);
            if (worker.get_location().is_adjacent_to(map_location)) {
              build_structure(worker_id, unit.get_id());
            }
          }
        }
      }
    } else {
      // Blueprint
      // TODO: move only workers that help
      //auto workers = get_my_nearest_robots(build_position, WorkerBit, 4, true, true);
      auto workers_info = get_nearest_units(
        my_team,
        build_position,
        WorkerBit,
        8,
        10,
        true
      );

      if (workers_info.empty()) {
        printf("Can't build: (%d, %d). No idle workers!\n", map_location.get_x(), map_location.get_y());
        return;
      }

      // Try to blueprint with the first worker
      bool blueprinted = false;

      {
        const auto worker_id = get<0>(workers_info[0]);
        const auto dir       = get<3>(workers_info[0]); // dir unit -> pos

        auto worker = gc.get_unit(worker_id);
        auto worker_map_location = worker.get_map_location();

        // If not adjacent, try to move first
        if (!worker_map_location.is_adjacent_to(map_location)) {
          move_unit(worker_id, dir);
        }

        worker = gc.get_unit(worker_id);
        worker_map_location = worker.get_map_location();
        // If is adjacent, try to blueprint
        if (worker_map_location.is_adjacent_to(map_location)) {
          const auto direction = worker_map_location.direction_to(map_location);

          if (gc.can_blueprint(worker.get_id(), structure_type, direction)) {
            gc.blueprint(worker.get_id(), structure_type, direction);
            set_unit_acted(worker.get_id());

            blueprinted = true;

            printf("Blueprint built at (%d, %d) by %u\n", map_location.get_x(), map_location.get_y(), worker.get_id());
          }
        }
      }

      workers_info.erase(workers_info.begin());

      // Move to structure and try to build if adjacent
      for (auto worker_info : workers_info) {
        const auto worker_id = get<0>(worker_info);
        const auto dir       = get<3>(worker_info); // dir unit -> pos

        move_unit(worker_id, dir);
        const auto worker = gc.get_unit(worker_id);
        if (blueprinted and worker.get_map_location().is_adjacent_to(map_location)) {
          const auto structure = gc.sense_unit_at_location(map_location);
          build_structure(worker_id, structure.get_id());
        }
      }
    }
  }

  vector<unsigned>
  get_units(
    Team team,
    UnitTypeBitset types_bitset,
    bool idle_units = false
  ) {
    // TODO: create a method for this!
    const auto& unit_list = [=] {
      if (team == my_team) return idle_units ? my_idle_unit_ids : my_unit_ids;
      return enemy_unit_ids;
    }();

    vector<unsigned> units;

    for (auto unit_type : AllTypes) if (on_bitset(unit_type, types_bitset)) {
      units.insert(units.end(), unit_list[unit_type].begin(), unit_list[unit_type].end());
    }

    return units;
  }

  void set_unit_acted(unsigned unit_id) {
    const auto& unit = gc.get_unit(unit_id);
    my_idle_unit_ids[unit.get_unit_type()].erase(unit_id);
  }

  void move_unit(unsigned unit_id, Direction direction) {
    // FIXME: idleness is only measured getting the movement
    set_unit_acted(unit_id);

    if (direction == Center)
      return;

    const auto unit = gc.get_unit(unit_id);
    if (unit.get_movement_heat() < 10 and gc.can_move(unit_id, direction)) {
      gc.move_robot(unit_id, direction);
      printf("Unit %u moving (%d)\n", unit_id, direction);
    }
  }

  void attack_unit(unsigned unit_id, unsigned target_id) {
    // FIXME: idleness is only measured getting the movement
    set_unit_acted(unit_id);

    const auto unit = gc.get_unit(unit_id);
    if (unit.get_attack_heat() < 10 and gc.can_attack(unit_id, target_id)) {
      gc.attack(unit_id, target_id);
      printf("Attacking %u -> %u\n", unit_id, target_id);

      if (!gc.has_unit(target_id)) {
        printf("Unit %u died!\n", unit_id);
        die_unit(target_id);
      }
    }
  }

  void build_structure(unsigned worker_id, unsigned structure_id) {
    if (gc.can_build(worker_id, structure_id)) {
      gc.build(worker_id, structure_id);
      set_unit_acted(worker_id);
      printf("Worker %u building %u\n", worker_id, structure_id);

      // Remove built unit from blueprints
      const auto& structure = gc.get_unit(structure_id);
      if (structure.structure_is_built()) {
        my_blueprint_ids[to_blueprint_type(structure.get_unit_type())].erase(structure_id);
        printf("Building done!\n");
      }
    }
  }

  void die_unit(unsigned unit_id) {
    unit_ids.erase(unit_id);
    for (int i = 0; i < TOTAL_UNIT_TYPES; i++) {
      enemy_unit_ids[i].erase(unit_id);
      my_unit_ids[i].erase(unit_id);
      my_idle_unit_ids[i].erase(unit_id);
    }

    for (int i = 0; i < TOTAL_BLUEPRINTS; i++)
      my_blueprint_ids[i].erase(unit_id);
  }

  void replicate_worker(unsigned worker_id, Direction dir) {
    gc.replicate(worker_id, dir);
    set_unit_acted(worker_id);


    const auto worker     = gc.get_unit(worker_id);
    const auto new_worker = gc.sense_unit_at_location(worker.get_map_location().add(dir));
    printf("Worker %u replicating (%d) -> %u\n", worker_id, dir, new_worker.get_id());

    my_unit_ids[Worker].insert(new_worker.get_id());
    unit_ids.insert(new_worker.get_id());
  }


  // Auxiliar
  unsigned to_rounds(unsigned distance, unsigned unit_id) const {
    if (distance == UINT_MAX)
      return UINT_MAX;

    const auto unit = gc.get_unit(unit_id);
    return (unit.get_movement_heat() + distance * unit.get_movement_cooldown()) / HEAT_REGEN;
  }

  unsigned to_adjacent(unsigned distance) const {
    if (distance == UINT_MAX)
      return UINT_MAX;
    return distance == 0 ? distance + 1 : distance - 1;
  }

  unsigned to_adjacent_minus(unsigned distance) const {
    if (distance == UINT_MAX)
      return UINT_MAX;
    return distance >= 2 ? distance - 2 : 0;
  }

  // -------------------------------

  // Attributes

  // Game code
  GameController gc;

  // Constant for whole game
  const Team         my_team, enemy_team;
  const Planet       my_planet;
  const PlanetMap&   earth;
  const PlanetMap&   mars;
  const PlanetMatrix earth_initial;
  const PlanetMatrix mars_initial;

  // Changed every round
  unsigned current_round;
  unsigned my_karbonite;

  PlanetMatrix planet_matrix; // current planet, using vision info and units information

  unordered_set<unsigned> unit_ids;

  unordered_set<unsigned> enemy_unit_ids[TOTAL_UNIT_TYPES];

  unordered_set<unsigned> my_unit_ids[TOTAL_UNIT_TYPES];
  unordered_set<unsigned> my_blueprint_ids[TOTAL_BLUEPRINTS];
  // TODO: my_unit_ids_in_garrison
  // TODO: my_unit_ids_in_space
  // TODO: enemy_unit_ids (by type)

  // TODO: Optimize this! idleness for move and action?
  unordered_set<unsigned> my_idle_unit_ids[TOTAL_UNIT_TYPES];

  // TODO: when spot an enemy factory add to enemy_base_candidates
  // TODO: verify if has_unit_at_location and can_sense_location to remove base candidate
  unordered_set<Position, pair_hash> enemy_base_candidates;

  // ---------

  // Actions

  priority_queue<pair<double, Actions>> action_queue;
};


int main() {

  HiveMind bot;
  bot.start();

  while (true)
    bot.step();
}
