/* test X client: asks WM to span fullscreen over xinerama screens given on argv (top bottom left right), paints red */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char** argv) {
    long idx[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4 && i + 1 < argc; i++) idx[i] = atol(argv[i + 1]);
    Display* d = XOpenDisplay(NULL);
    if (!d) return 1;
    Window r = DefaultRootWindow(d);
    Window w = XCreateSimpleWindow(d, r, 0, 0, 800, 600, 0, 0, 0xff0000);
    XStoreName(d, w, "spanclient");
    XSelectInput(d, w, StructureNotifyMask | ExposureMask);
    Atom state = XInternAtom(d, "_NET_WM_STATE", False), fs = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
    Atom fsm = XInternAtom(d, "_NET_WM_FULLSCREEN_MONITORS", False);
    /* like wfica: _NET_WM_STATE add fullscreen message, monitors message, then map */
    XEvent e = {0};
    e.xclient.type = ClientMessage; e.xclient.window = w; e.xclient.message_type = state; e.xclient.format = 32;
    e.xclient.data.l[0] = 1; e.xclient.data.l[1] = fs; e.xclient.data.l[3] = 1;
    XSendEvent(d, r, False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
    XEvent m = {0};
    m.xclient.type = ClientMessage; m.xclient.window = w; m.xclient.message_type = fsm; m.xclient.format = 32;
    for (int i = 0; i < 4; i++) m.xclient.data.l[i] = idx[i];
    m.xclient.data.l[4] = 1;
    XSendEvent(d, r, False, SubstructureRedirectMask | SubstructureNotifyMask, &m);
    XMapWindow(d, w);
    XSync(d, False);
    setvbuf(stdout, NULL, _IOLBF, 0);
    for (int t = 0; t < 40; t++) {
        while (XPending(d)) {
            XNextEvent(d, &e);
            if (e.type == ConfigureNotify) printf("configure %dx%d+%d+%d\n", e.xconfigure.width, e.xconfigure.height, e.xconfigure.x, e.xconfigure.y);
        }
        usleep(100000);
    }
    Window child; int x, y; unsigned ww, hh, bw, dep; Window root;
    XGetGeometry(d, w, &root, &x, &y, &ww, &hh, &bw, &dep);
    XTranslateCoordinates(d, w, r, 0, 0, &x, &y, &child);
    printf("final %ux%u+%d+%d\n", ww, hh, x, y);
    pause();
}
