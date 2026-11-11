#include "MonitorPositionController.hpp"

#include "../config/shared/monitor/MonitorRule.hpp"
#include "../debug/log/Logger.hpp"
#include "../macros.hpp"

#include <algorithm>

using namespace State;

UP<CMonitorPositionController>& State::monitorPositionController() {
    static UP<CMonitorPositionController> p = makeUnique<CMonitorPositionController>();
    return p;
}

void CMonitorPositionController::arrange(std::span<const SP<Monitor::IMonitorArrangeable>> monitors, bool xwaylandForceZeroScaling, bool xwaylandRealLayout) const {
    std::vector<SP<Monitor::IMonitorArrangeable>> toArrange(monitors.begin(), monitors.end());
    std::vector<SP<Monitor::IMonitorArrangeable>> arranged;
    arranged.reserve(toArrange.size());

    for (auto it = toArrange.begin(); it != toArrange.end();) {
        auto m = *it;

        if (const auto POS = m->explicitPosition(); POS.has_value()) {
            LOG(Log::DEBUG, "arrangeMonitors: {} explicit {:j}", m->name(), *POS);

            m->moveTo(*POS);
            arranged.push_back(m);
            it = toArrange.erase(it);

            if (it == toArrange.end())
                break;

            continue;
        }

        ++it;
    }

    // Variables to store the max and min values of monitors on each axis.
    int  maxXOffsetRight = 0;
    int  maxXOffsetLeft  = 0;
    int  maxYOffsetUp    = 0;
    int  maxYOffsetDown  = 0;

    auto recalcMaxOffsets = [&]() {
        maxXOffsetRight = 0;
        maxXOffsetLeft  = 0;
        maxYOffsetUp    = 0;
        maxYOffsetDown  = 0;

        for (auto const& m : arranged) {
            maxXOffsetRight = std::max<double>(m->position().x + m->size().x, maxXOffsetRight);
            maxXOffsetLeft  = std::min<double>(m->position().x, maxXOffsetLeft);
            maxYOffsetDown  = std::max<double>(m->position().y + m->size().y, maxYOffsetDown);
            maxYOffsetUp    = std::min<double>(m->position().y, maxYOffsetUp);
        }
    };

    // Iterates through all non-explicitly placed monitors.
    for (auto const& m : toArrange) {
        recalcMaxOffsets();

        Vector2D newPosition = {0, 0};
        switch (m->autoDirection()) {
            case Config::eAutoDirs::DIR_AUTO_UP: newPosition.y = maxYOffsetUp - m->size().y; break;
            case Config::eAutoDirs::DIR_AUTO_DOWN: newPosition.y = maxYOffsetDown; break;
            case Config::eAutoDirs::DIR_AUTO_LEFT: newPosition.x = maxXOffsetLeft - m->size().x; break;
            case Config::eAutoDirs::DIR_AUTO_RIGHT:
            case Config::eAutoDirs::DIR_AUTO_NONE: newPosition.x = maxXOffsetRight; break;
            case Config::eAutoDirs::DIR_AUTO_CENTER_UP: {
                int width     = maxXOffsetRight - maxXOffsetLeft;
                newPosition.y = maxYOffsetUp - m->size().y;
                newPosition.x = maxXOffsetLeft + (width - m->size().x) / 2;
                break;
            }
            case Config::eAutoDirs::DIR_AUTO_CENTER_DOWN: {
                int width     = maxXOffsetRight - maxXOffsetLeft;
                newPosition.y = maxYOffsetDown;
                newPosition.x = maxXOffsetLeft + (width - m->size().x) / 2;
                break;
            }
            case Config::eAutoDirs::DIR_AUTO_CENTER_LEFT: {
                int height    = maxYOffsetDown - maxYOffsetUp;
                newPosition.x = maxXOffsetLeft - m->size().x;
                newPosition.y = maxYOffsetUp + (height - m->size().y) / 2;
                break;
            }
            case Config::eAutoDirs::DIR_AUTO_CENTER_RIGHT: {
                int height    = maxYOffsetDown - maxYOffsetUp;
                newPosition.x = maxXOffsetRight;
                newPosition.y = maxYOffsetUp + (height - m->size().y) / 2;
                break;
            }
            default: UNREACHABLE();
        }

        m->moveTo(newPosition);
        LOG(Log::DEBUG, "arrangeMonitors: {} auto {:j}", m->name(), m->position());
        arranged.emplace_back(m);
    }

    // Real layout: xwayland coords are the layout coords shifted to be non-negative.
    // Only valid when every monitor is unscaled, as xwayland works in buffer pixels.
    const bool ALL_UNSCALED = std::ranges::all_of(monitors, [](const auto& m) { return m->scale() == 1.F; });
    if (xwaylandRealLayout && ALL_UNSCALED && !monitors.empty()) {
        Vector2D minPos = monitors.front()->position();
        for (auto const& m : monitors) {
            minPos.x = std::min(minPos.x, m->position().x);
            minPos.y = std::min(minPos.y, m->position().y);
        }

        for (auto const& m : monitors) {
            const auto XPOS = m->position() - minPos;
            LOG(Log::DEBUG, "arrangeMonitors: {} xwayland (real layout) [{}, {}]", m->name(), XPOS.x, XPOS.y);
            m->setXWaylandPosition(XPOS);
            m->setXWaylandScale(1.F);
        }
        return;
    }

    if (xwaylandRealLayout && !ALL_UNSCALED)
        LOG(Log::WARN, "arrangeMonitors: xwayland:real_layout ignored, it requires all monitors to use scale 1");

    maxXOffsetRight = 0;
    for (auto const& m : monitors) {
        LOG(Log::DEBUG, "arrangeMonitors: {} xwayland [{}, {}]", m->name(), maxXOffsetRight, 0);
        m->setXWaylandPosition({maxXOffsetRight, 0});
        maxXOffsetRight += xwaylandForceZeroScaling ? m->transformedSize().x : m->size().x;
        m->setXWaylandScale(xwaylandForceZeroScaling ? m->scale() : 1.F);
    }
}
