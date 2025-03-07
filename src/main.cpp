
#include "defines.h"
#include "engine/Game.h"

#include "ShipSimulationScene.h"

i32 main() {
    Game game{1280, 720, "blockship: code"};

    game.run<ShipSimulationScene>();
    return 0;
}
