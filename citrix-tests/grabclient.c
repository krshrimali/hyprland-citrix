#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>
int main(void) {
    Display* d = XOpenDisplay(NULL); if (!d) return 1;
    Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 400, 300, 0, 0, 0x00ff00);
    XStoreName(d, w, "grabclient");
    XSelectInput(d, w, KeyPressMask | FocusChangeMask | StructureNotifyMask);
    XMapRaised(d, w); XSync(d, False);
    sleep(2);
    XSetInputFocus(d, w, RevertToParent, CurrentTime);
    int r = XGrabKeyboard(d, w, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    printf("XGrabKeyboard=%d (0=GrabSuccess)\n", r); fflush(stdout);
    XSync(d, False);
    sleep(4);
    XUngrabKeyboard(d, CurrentTime); XSync(d, False);
    printf("ungrabbed\n"); fflush(stdout);
    sleep(2);
}
