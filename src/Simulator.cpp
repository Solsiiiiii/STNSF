#include "Simulator.hpp"

using namespace geode::prelude;

namespace stnsf::sim {
    static PlayerObject* g_simPlayer = nullptr;
    static bool g_simDied = false;

    bool isSimulationPlayer(PlayerObject* player) {
        return player && player == g_simPlayer;
    }

    void markSimulationDeath(PlayerObject* player) {
        if (isSimulationPlayer(player))
            g_simDied = true;
    }

    static void destroySimulationPlayer() {
        if (!g_simPlayer)
            return;
        g_simPlayer->removeFromParentAndCleanup(true);
        g_simPlayer = nullptr;
    }

    static PlayerObject* makeSimulationPlayer(GJBaseGameLayer* layer, PlayerObject* live) {
        // Geometry Dash 2.2081 exposes this exact five-argument factory.
        auto* sim = PlayerObject::create(1, 1, layer, layer, true);
        if (!sim)
            return nullptr;

        // copyAttributes is part of the 2.2081 PlayerObject API and copies the
        // gameplay state needed to continue physics from the live player.
        sim->copyAttributes(live);
        sim->setPosition(live->getPosition());
        sim->setRotation(live->getRotation());
        sim->setVisible(false);
        layer->addChild(sim, -100000);
        return sim;
    }

    static bool advanceOnePhysicsFrame(GJBaseGameLayer* layer, PlayerObject* sim) {
        if (g_simDied)
            return false;

        constexpr float PHYSICS_DT = 1.f / 240.f;
        sim->resetCollisionLog(false);
        sim->update(PHYSICS_DT);
        layer->checkCollisions(sim, PHYSICS_DT, false);
        return !g_simDied && !sim->m_playerDied;
    }

    int findNearestSurvivableOffset(
        GJBaseGameLayer* layer,
        PlayerObject* livePlayer,
        bool player2,
        int button,
        bool push,
        int maxFrames
    ) {
        if (!layer || !livePlayer || maxFrames < 0)
            return -1;

        maxFrames = std::clamp(maxFrames, 0, 12);

        destroySimulationPlayer();
        g_simDied = false;

        auto* sim = makeSimulationPlayer(layer, livePlayer);
        if (!sim)
            return -1;
        g_simPlayer = sim;

        // Test candidates in ascending order. Candidate 0 is the exact input
        // frame; candidate N advances the clone N physics frames before the
        // input. The first surviving candidate is therefore the nearest safe
        // frame, with no more than the configured 12-frame forward allowance.
        for (int candidate = 0; candidate <= maxFrames; ++candidate) {
            sim->copyAttributes(livePlayer);
            sim->setPosition(livePlayer->getPosition());
            sim->setRotation(livePlayer->getRotation());
            sim->m_playerDied = false;
            g_simDied = false;

            bool alive = true;
            for (int frame = 0; frame < candidate && alive; ++frame)
                alive = advanceOnePhysicsFrame(layer, sim);

            if (alive) {
                const auto playerButton = static_cast<PlayerButton>(button);
                if (push)
                    sim->pushButton(playerButton);
                else
                    sim->releaseButton(playerButton);

                alive = advanceOnePhysicsFrame(layer, sim);
            }

            if (alive) {
                g_simPlayer = nullptr;
                sim->removeFromParentAndCleanup(true);
                return candidate;
            }
        }

        g_simPlayer = nullptr;
        sim->removeFromParentAndCleanup(true);
        return -1;
    }
}

// Suppress death effects for the hidden simulation player. The real player is
// never intercepted by this hook.
class $modify(STNSFSimulationPlayer, PlayerObject) {
public:
    void playerDestroyed(bool noEffects) {
        if (stnsf::sim::isSimulationPlayer(this)) {
            stnsf::sim::markSimulationDeath(this);
            return;
        }
        PlayerObject::playerDestroyed(noEffects);
    }
};
