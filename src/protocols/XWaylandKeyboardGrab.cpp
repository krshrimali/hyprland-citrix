#include "XWaylandKeyboardGrab.hpp"
#include <algorithm>
#include "core/Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../debug/log/Logger.hpp"
#ifndef NO_XWAYLAND
#include "../xwayland/XWayland.hpp"
#include "../xwayland/Server.hpp"
#endif

CXWaylandKeyboardGrab::CXWaylandKeyboardGrab(SP<CZwpXwaylandKeyboardGrabV1> resource_, SP<CWLSurfaceResource> surf) : m_resource(resource_), m_surface(surf) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CZwpXwaylandKeyboardGrabV1* r) { PROTO::xwaylandKeyboardGrab->destroyGrab(this); });
    m_resource->setOnDestroy([this](CZwpXwaylandKeyboardGrabV1* r) { PROTO::xwaylandKeyboardGrab->destroyGrab(this); });
}

SP<CWLSurfaceResource> CXWaylandKeyboardGrab::surface() {
    return m_surface.lock();
}

bool CXWaylandKeyboardGrab::good() {
    return m_resource->resource();
}

CXWaylandKeyboardGrabProtocol::CXWaylandKeyboardGrabProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXWaylandKeyboardGrabProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwpXwaylandKeyboardGrabManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwpXwaylandKeyboardGrabManagerV1* p) { onManagerResourceDestroy(p->resource()); });
    RESOURCE->setDestroy([this](CZwpXwaylandKeyboardGrabManagerV1* p) { onManagerResourceDestroy(p->resource()); });
    RESOURCE->setGrabKeyboard([this](CZwpXwaylandKeyboardGrabManagerV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* seat) { onGrab(pMgr, id, surface, seat); });
}

void CXWaylandKeyboardGrabProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CXWaylandKeyboardGrabProtocol::destroyGrab(CXWaylandKeyboardGrab* grab) {
    std::erase_if(m_grabs, [&](const auto& other) { return other.get() == grab; });
}

void CXWaylandKeyboardGrabProtocol::onGrab(CZwpXwaylandKeyboardGrabManagerV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* seat) {
    const auto CLIENT = pMgr->client();
    const auto SURF   = CWLSurfaceResource::fromResource(surface);

    const auto RESOURCE = m_grabs.emplace_back(makeUnique<CXWaylandKeyboardGrab>(makeShared<CZwpXwaylandKeyboardGrabV1>(CLIENT, pMgr->version(), id), SURF)).get();
    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_grabs.pop_back();
        return;
    }

    LOG(Log::DEBUG, "[xwayland-keyboard-grab] grab on surface {:x}", rc<uintptr_t>(SURF.get()));
}

bool CXWaylandKeyboardGrabProtocol::isGrabbed(SP<CWLSurfaceResource> surface) {
    static auto PALLOW = CConfigValue<Config::INTEGER>("xwayland:allow_keyboard_grabs");
    if (!*PALLOW || !surface)
        return false;

#ifndef NO_XWAYLAND
    // only honour grabs from Xwayland itself
    if (!g_pXWayland || !g_pXWayland->m_server || surface->client() != g_pXWayland->m_server->m_xwaylandClient)
        return false;
#else
    return false;
#endif

    return std::ranges::any_of(m_grabs, [&surface](const auto& g) { return g->surface() == surface; });
}
