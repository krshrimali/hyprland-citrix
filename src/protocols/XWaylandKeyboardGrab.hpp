#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "xwayland-keyboard-grab-unstable-v1.hpp"

class CWLSurfaceResource;

// Xwayland requests this grab when an X11 client actively grabs the keyboard (XGrabKeyboard).
// While the grabbed surface has keyboard focus, compositor shortcuts are inhibited, like on X11.
class CXWaylandKeyboardGrab {
  public:
    CXWaylandKeyboardGrab(SP<CZwpXwaylandKeyboardGrabV1> resource_, SP<CWLSurfaceResource> surf);

    SP<CWLSurfaceResource> surface();
    bool                   good();

  private:
    SP<CZwpXwaylandKeyboardGrabV1> m_resource;
    WP<CWLSurfaceResource>         m_surface;
};

class CXWaylandKeyboardGrabProtocol : public IWaylandProtocol {
  public:
    CXWaylandKeyboardGrabProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    bool         isGrabbed(SP<CWLSurfaceResource> surface);

  private:
    void                                               onManagerResourceDestroy(wl_resource* res);
    void                                               destroyGrab(CXWaylandKeyboardGrab* grab);
    void                                               onGrab(CZwpXwaylandKeyboardGrabManagerV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* seat);

    std::vector<UP<CZwpXwaylandKeyboardGrabManagerV1>> m_managers;
    std::vector<UP<CXWaylandKeyboardGrab>>             m_grabs;

    friend class CXWaylandKeyboardGrab;
};

namespace PROTO {
    inline UP<CXWaylandKeyboardGrabProtocol> xwaylandKeyboardGrab;
};
