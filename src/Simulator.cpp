#include "Simulator.hpp"

using namespace geode::prelude;

namespace stnsf::sim {
    static PlayerObject* g_simPlayer = nullptr;
    static bool g_simulating = false;
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
        // 2.2081's binding exposes this five-argument constructor.
        auto* sim = PlayerObject::create(1, 1, layer, layer, true);
        if (!sim)
            return nullptr;

        // copyAttributes is a 2.2081 PlayerObject method specifically intended
        // to transfer the complete gameplay state between PlayerObjects.
        sim->copyAttributes(live);
        sim->setPosition(live->getPosition());
        sim->setRotation(live->getRotation());
        sim->setVisible(false);

        // Keep the clone in the layer's node tree so cocos/Geometry Dash
        // pointers used during physics are valid, while keeping it invisible.
        layer->addChild(sim, -100000);
        return sim;
    }

    static bool advanceOneFrame(GJBaseGameLayer* layer, PlayerObject* sim, float dt) {
        if (g_simDied)
            return false;

        sim->resetCollisionLog();
        sim->update(dt);
        layer->checkCollisions(sim, dt, false);
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
        g_simulating = true;

        auto* sim = makeSimulationPlayer(layer, livePlayer);
        if (!sim) {
            g_simulating = false;
            return -1;
        }
        g_simPlayer = sim;

        // Candidate 0 is the actual input frame. Candidates >0 advance the
        // clone first, then apply the input. The first surviving candidate is
        // therefore the nearest survivable frame to the real click.
        for (int candidate = 0; candidate <= maxFrames; ++candidate) {
            sim->copyAttributes(livePlayer);
            sim->setPosition(livePlayer->getPosition());
            sim->setRotation(livePlayer->getRotation());
            sim->m_playerDied = false;
            g_simDied = false;

            bool alive = true;
            for (int frame = 0; frame < candidate && alive; ++frame) {
                alive = advanceOneFrame(layer, sim, 1.f / 240.f);
            }

            if (alive) {
                if (push)
                    sim->pushButton(button, player2);
                else
                    sim->releaseButton(button, player2);

                alive = advanceOneFrame(layer, sim, 1.f / 240.f);
            }

            if (alive) {
                g_simulating = false;
                destroySimulationPlayer();
                return candidate;
            }
        }

        g_simulating = false;
        destroySimulationPlayer();
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
