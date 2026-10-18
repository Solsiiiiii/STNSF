#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

namespace stnsf {
    struct PendingInput {
        bool push = false;
        int button = 1;
        bool player2 = false;
        int framesRemaining = 0;
    };

    static bool enabled() {
        return Mod::get()->getSettingValue<bool>("enabled");
    }

    static int window() {
        return std::clamp(Mod::get()->getSettingValue<int64_t>("window"), int64_t(0), int64_t(12));
    }

    static bool playerEnabled(bool player2) {
        return Mod::get()->getSettingValue<bool>(player2 ? "p2" : "p1");
    }

    // STNSF's public interface is deliberately separated from the simulation
    // engine. The simulator will replace this evaluator once the 2.2081
    // PlayerObject snapshot/clone state is wired in.
    // Returning 0 means "use the original input frame" and is the safe
    // fallback until a survivability result has been established.
    static int nearestSurvivableOffset(GJBaseGameLayer*, bool, int, bool) {
        return 0;
    }

    static std::vector<PendingInput> pending;

    static void clear() {
        pending.clear();
    }
}

class $modify(STNSFGJBaseGameLayer, GJBaseGameLayer) {
public:
    void handleButton(bool push, int button, bool player1) {
        auto* pl = PlayLayer::get();
        if (!stnsf::enabled() || !pl || pl->m_playerDied) {
            return GJBaseGameLayer::handleButton(push, button, player1);
        }

        const bool player2 = !player1;
        if (!stnsf::playerEnabled(player2)) {
            return GJBaseGameLayer::handleButton(push, button, player1);
        }

        // Do not alter the input unless the survivability solver finds a
        // different valid frame. This guarantees the initial implementation
        // is behavior-preserving while the solver is being integrated.
        const int offset = stnsf::nearestSurvivableOffset(this, player2, button, push);
        if (offset <= 0) {
            return GJBaseGameLayer::handleButton(push, button, player1);
        }

        // The actual delayed-input scheduler is intentionally kept separate
        // from this hook so P1/P2 and touch/controller inputs all share one
        // timing path.
        stnsf::pending.push_back({push, button, player2, offset});
    }
};

class $modify(STNSFPlayLayer, PlayLayer) {
public:
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;
        stnsf::clear();
        return true;
    }

    void resetLevel() {
        stnsf::clear();
        PlayLayer::resetLevel();
    }
};

$on_mod(Loaded) {
    log::info("STNSF loaded — Geometry Dash 2.2081 target");
}
