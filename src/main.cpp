#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "Simulator.hpp"

using namespace geode::prelude;

namespace stnsf {
    struct PendingInput {
        bool push = false;
        int button = 1;
        bool player2 = false;
        int framesRemaining = 0;
    };

    static std::vector<PendingInput> pending;

    static bool enabled() {
        return Mod::get()->getSettingValue<bool>("enabled");
    }

    static int window() {
        return static_cast<int>(std::clamp(
            Mod::get()->getSettingValue<int64_t>("window"),
            int64_t(0), int64_t(12)
        ));
    }

    static bool playerEnabled(bool player2) {
        return Mod::get()->getSettingValue<bool>(player2 ? "p2" : "p1");
    }

    static void clear() {
        pending.clear();
    }

    static void dispatch(GJBaseGameLayer* layer, PendingInput const& input) {
        // Calling the base implementation bypasses our interception hook.
        GJBaseGameLayer::handleButton(input.push, input.button, !input.player2);
    }

    static void tickPending(GJBaseGameLayer* layer) {
        for (size_t i = 0; i < pending.size();) {
            auto& input = pending[i];
            if (input.framesRemaining > 0)
                --input.framesRemaining;

            if (input.framesRemaining == 0) {
                auto ready = input;
                pending.erase(pending.begin() + static_cast<ptrdiff_t>(i));
                dispatch(layer, ready);
            } else {
                ++i;
            }
        }
    }
}

class $modify(STNSFGJBaseGameLayer, GJBaseGameLayer) {
public:
    void handleButton(bool push, int button, bool player1) {
        auto* pl = PlayLayer::get();
        const bool player2 = !player1;

        if (!stnsf::enabled() || !pl || pl->m_playerDied ||
            !stnsf::playerEnabled(player2) || stnsf::window() == 0) {
            return GJBaseGameLayer::handleButton(push, button, player1);
        }

        auto* livePlayer = player2 ? pl->m_player2 : pl->m_player1;
        if (!livePlayer) {
            return GJBaseGameLayer::handleButton(push, button, player1);
        }

        const int offset = stnsf::sim::findNearestSurvivableOffset(
            this, livePlayer, player2, button, push, stnsf::window()
        );

        // No survivable candidate was found. Never swallow the user's input.
        if (offset < 0) {
            return GJBaseGameLayer::handleButton(push, button, player1);
        }

        // Candidate 0 means the original frame is already survivable.
        if (offset == 0) {
            return GJBaseGameLayer::handleButton(push, button, player1);
        }

        // The simulator found a safe future frame. Store the event and let the
        // normal PlayLayer update loop dispatch it on that exact frame.
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

    void update(float dt) {
        // Deliver queued inputs at the start of the corresponding physics
        // frame, before the game's own update advances that frame.
        stnsf::tickPending(this);
        PlayLayer::update(dt);
    }
};

$on_mod(Loaded) {
    log::info("STNSF loaded — Geometry Dash 2.2081 — 12-frame survivability window");
}
