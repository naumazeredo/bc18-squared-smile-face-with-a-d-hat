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

#include "bc.hpp"

using namespace bc;
using namespace std;

#define debug() printf("line %d", __LINE__); fflush(stdout);

const int TOTAL_PLANETS    = 2;
const int TOTAL_UNIT_TYPES = 7;
const int TOTAL_ADJ_DIRECTIONS = 8; // Center is the last Direction

const vector<Direction> directions = { North, Northeast, East, Southeast, South, Southwest, West, Northwest, Center };
const vector<Direction> adj_directions = { North, Northeast, East, Southeast, South, Southwest, West, Northwest };


enum { WE, THEM };

pair<int, int> to_pair(const MapLocation& map_location) { return { map_location.get_x(), map_location.get_y() }; }

/*

Actions

Determine what to be done, by how many, which units can make, if it's repeatable, the priority, etc
Every game state should have it's own Actions to be done.

*/

struct Action {
  unsigned priority; // 0 ~ 5, from low to high
};

/*
State machine to build better strategy

GameState: the general strategy
  responsible for:
    - queueing researches
    - managing rocket travels
    - managing resources (?)

  FactoryEra: building factories as soon as possible
  RocketEra: building rocket to travel
  EarthEra: surviving on Earth (and possibly having units on Mars)
  MarsEra: surviving solely on Mars (required after round 750)


AggressiveState: the army strategy
  responsible for:
    - controlling which army to build
    - controlling size of the army
    - controlling army strategies

  Pacifist: survive and protect
  GenghisKhan: destroy the enemy!

*/

class StateMachine {
public:
  StateMachine() :
      gc {},
      my_team { gc.get_team() },
      my_planet { gc.get_planet() },
      earth { gc.get_earth_map() },
      mars { gc.get_mars_map() }
  {
    game_state = GameState::BigBangEra;
    aggr_state = AggressiveState::Pacifist;
  }

  void start() {
    printf("d=] starting! Get ready to be pwned!\n");
  }

  void step() {
    get_round_data();

    printf("Round %d (%u)\n", round, my_karbonite);
    fflush(stdout);

    gs_transitions[game_state]();
    as_transitions[aggr_state]();

    gs_run[game_state]();
    as_run[aggr_state]();

    fflush(stdout);
    gc.next_turn();
  }

private:
  enum GameState {
    BigBangEra,
    FactoryEra,
    RocketEra,
    EarthEra,
    MarsEra
  };

  enum AggressiveState {
    Pacifist,
    GenghisKhan
  };

  void change_game_state(GameState new_game_state) {
    game_state_start_round = round;
    game_state = new_game_state;
  }

  void get_units() {
    // Cleanup
    for (int i = 0; i < 2; i++)
      for (int j = 0; j < TOTAL_UNIT_TYPES; j++)
        units[i].clear();
    for (int i = 0; i < TOTAL_UNIT_TYPES; i++)
      idle[i].clear();
    // ----

    auto all_units = gc.get_units();
    for (auto unit : all_units) {
      auto id   = unit.get_id();
      auto team = (unit.get_team() == my_team) ? WE : THEM;
      units[team][id] = unit;
    }
  }

  void get_round_data() {
    round = gc.get_round();
    my_karbonite = gc.get_karbonite();

    get_units();
  }

  /*
  Direction get_best_build_direction(const Unit& worker) {
    auto map_location = worker.get_location().get_map_location();
    auto id = worker.get_id();

    unsigned best_karbonite = UINT_MAX;
    Direction best_direction = Center;
    for (auto direction : adj_directions) {
      if (!gc.can_blueprint(id, Factory, direction))
        continue;

      auto karb = gc.get_karbonite_at(map_location.add(direction));
      if (karb < best_karbonite) {
        best_karbonite = karb;
        best_direction = direction;
      }
    }

    return best_direction;
  }

  MapLocation get_best_build_location(unsigned max_search_range = 5) {
    // {x, y} -> { total workers reaching, total karbonite }
    map<pair<int, int>, pair<int, unsigned>> locations;

    for (auto& worker : idle[Workers]) {
      queue<pair<int, int>> 
      pair<int, int> 
    }
  }
  */

  const PlanetMap& my_planet_map() const { return my_planet == Earth ? earth : mars; }

  // -------------------------------

  // State machine code

  // IMPORTANT: Same order as enum!
  const vector<function<void()>> gs_run = {
    [this] { // Big Bang Era
      // Shouldn't ever execute
      printf("Big Bang Era is executing!!! STOP THE CAR!\n");
    },

    [this] { // Factory Era
      if (game_state_start_round == round) {
        printf("Factory Era!\n");
      }

      if (round == 1) {
        // FIXME: Change when research queue schema has been made
        gc.queue_research(Rocket);

        auto& worker = (*idle[Worker].begin());
        auto dir = get_best_build_direction(worker);
        gc.blueprint(worker.get_id(), Factory, dir);
      }
    },
    [this] { // Rocket Era
      if (game_state_start_round == round) {
        printf("Rocket Era!\n");
      }
    },
    [this] { // Earth Era
      if (game_state_start_round == round) {
        printf("Earth Era!\n");
      }
    },
    [this] { // Mars Era
      if (game_state_start_round == round) {
        printf("Mars Era!\n");
      }
    }
  };

  const vector<function<void()>> as_run = {
    [this] {},
    [this] {}
  };

  const vector<function<void()>> gs_transitions = {
    [this] { // Big Bang Era
      if (game_state_start_round == round) {
        printf("Big Bang Era!\n");
      }

      if (my_planet == Earth) {
        // Start Factory Era immediatly
        change_game_state(GameState::FactoryEra);
      } else {
        // Mars only have one era: Mars Era
        change_game_state(GameState::MarsEra);
      }
    },
    [this] { // Factory Era
      //if (units[WE][Factory].size() > 0)
      //game_state = GameState::RocketEra;
    },
    [this] { // Rocket Era
    },
    [this] { // Earth Era
    },
    [this] { // Mars Era
      // XXX: There's no comeback from Mars! Don't change to another Era!
    }
  };

  // IMPORTANT: Same order as enum!
  const vector<function<void()>> as_transitions = {
    [this] {},
    [this] {}
  };

  // XXX: Use separate states?
  GameState       game_state;
  AggressiveState aggr_state;

  unsigned game_state_start_round = 1;

  // TODO: create state machine transitions

  // --------------------

  // Game code
  GameController gc;

  // Constant for whole game
  const Team       my_team;
  const Planet     my_planet;
  const PlanetMap& earth;
  const PlanetMap& mars;

  // Changed every round
  unsigned round;
  unsigned my_karbonite;

  // 
  vector<vector<pair<bool, unsigned>>> planets[TOTAL_PLANETS];

  unordered_map<unsigned, Unit> units[2];
  unordered_set<const Unit&> idle[TOTAL_UNIT_TYPES]; // XXX: Idleness by type

  // ---------
};


int main() {

  StateMachine bot;
  bot.start();

  while (true) {
    bot.step();

    /*
    auto units = gc.get_my_units();
    for (const auto unit : units) {
      const unsigned id = unit.get_id();

      if (unit.get_unit_type() == Factory) {
        auto garrison = unit.get_structure_garrison();

        if (garrison.size() > 0){
          Direction dir = (Direction) dice();
          if (gc.can_unload(id, dir)){
            gc.unload(id, dir);
            continue;
          }
        } else if (gc.can_produce_robot(id, Knight)){
          gc.produce_robot(id, Knight);
          continue;
        }
      }

      if (unit.get_location().is_on_map()){
        // Calls on the controller take unit IDs for ownership reasons.
        const auto locus = unit.get_location().get_map_location();
        const auto nearby = gc.sense_nearby_units(locus, 2);
        for (auto place : nearby) {
          //Building 'em blueprints
          if(gc.can_build(id, place.get_id()) && unit.get_unit_type() == Worker){
            gc.build(id, place.get_id());
            continue;
          }
          //Attacking 'em enemies
          if (place.get_team() != unit.get_team() and
              gc.is_attack_ready(id) and
              gc.can_attack(id, place.get_id())) {
            gc.attack(id, place.get_id());
            continue;
          }
        }
      }

      Direction d = (Direction) dice();

      // Placing 'em blueprints
      if(gc.can_blueprint(id, Factory, d) and gc.get_karbonite() > unit_type_get_blueprint_cost(Factory)){
        gc.blueprint(id, Factory, d);
      } else if (gc.is_move_ready(id) && gc.can_move(id,d)){ // Moving otherwise (if possible)
        gc.move_robot(id,d);
      }
    }
    */
  }
}
