#pragma once

#include <Geode/Geode.hpp>

namespace stnsf::sim {
    // Returns the smallest forward offset in [0, maxFrames] for which the
    // simulated input leaves the player alive. Returns -1 when no candidate
    // survives. The simulation never advances the real player.
    int findNearestSurvivableOffset(
        GJBaseGameLayer* layer,
        PlayerObject* livePlayer,
        bool player2,
        int button,
        bool push,
        int maxFrames
    );

    // Called by the PlayerObject death hook while a simulation is active.
    bool isSimulationPlayer(PlayerObject* player);
    void markSimulationDeath(PlayerObject* player);
}
