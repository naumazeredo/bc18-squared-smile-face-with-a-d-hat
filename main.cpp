// TODO: Verify which includes are needed
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <random>
#include <functional>
#include <map>

#include "bc.hpp"

#define debug() printf("line %d", __LINE__); fflush(stdout);

const int TOTAL_PLANETS    = 2;
const int TOTAL_UNIT_TYPES = 7;

using namespace bc;
using namespace std;

enum { WE, THEM };

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
      earth { gc.get_earth_map() },
      mars { gc.get_mars_map() }
  {
    game_state = GameState::FactoryEra;
    aggr_state = AggressiveState::Pacifist;
  }

  void start() {
  }

  void step() {
    get_round_data();
    printf("round %d %u\n", round, karbonite);

    gs_transitions[game_state]();
    as_transitions[aggr_state]();

    fflush(stdout);
    gc.next_turn();
  }

private:
  // State machine code
  enum GameState {
    FactoryEra,
    RocketEra,
    EarthEra,
    MarsEra
  };

  enum AggressiveState {
    Pacifist,
    GenghisKhan
  };

  // IMPORTANT: Same order as enum!
  const vector<function<void()>> gs_transitions = {
    // FactoryEra
    [this] {
      if (units[WE][Factory].size() > 0)
        game_state = GameState::RocketEra;
    },
    [this] {},
    [this] {},
    [this] {}
  };

  // IMPORTANT: Same order as enum!
  const vector<function<void()>> as_transitions = {
    [this] {},
    [this] {}
  };

  // XXX: Use separate states?
  GameState       game_state;
  AggressiveState aggr_state;

  // TODO: create state machine transitions

  // --------------------

  // Game code
  GameController gc;

  // Constant for whole game
  const Team my_team;
  const PlanetMap& earth, mars;

  // Changed every round
  unsigned round;
  unsigned karbonite;

  vector<vector<pair<bool, unsigned>>> planets[TOTAL_PLANETS];

  vector<Unit> units[2][TOTAL_UNIT_TYPES];

  void get_units() {
    // Clear units
    for (int i = 0; i < 2; i++)
      for (int j = 0; j < TOTAL_UNIT_TYPES; j++)
        units[i][j].clear();

    auto all_units = gc.get_units();
    for (auto unit : all_units)
      units[!(unit.get_team() == my_team)][unit.get_unit_type()].push_back(unit);
  }

  void get_round_data() {
    round = gc.get_round();
    karbonite = gc.get_karbonite();

    get_units();
  }
  // ---------
};


int main() {
  printf("d=] starting! Get ready to be pwned!\n");

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
