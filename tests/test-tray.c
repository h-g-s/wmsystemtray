#include "config.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <X11/Xlib.h>

static void pause_briefly(void){
    struct timespec delay = { .tv_sec = 0, .tv_nsec = 50000000 };
    nanosleep(&delay, NULL);
}

int main(void){
    Display *display = XOpenDisplay(NULL);
    if(!display){
        fprintf(stderr, "Cannot open test display\n");
        return 1;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    char name[50];
    snprintf(name, sizeof(name), "_NET_SYSTEM_TRAY_S%d", screen);
    Atom selection = XInternAtom(display, name, False);
    Atom opcode = XInternAtom(display, "_NET_SYSTEM_TRAY_OPCODE", False);
    Atom xembed = XInternAtom(display, "_XEMBED", False);
    Atom xembed_info = XInternAtom(display, "_XEMBED_INFO", False);
    Window owner = None;
    for(int i=0; i<100 && owner==None; i++){
        owner = XGetSelectionOwner(display, selection);
        if(owner==None) pause_briefly();
    }
    if(owner==None){
        fprintf(stderr, "Tray did not acquire the selection\n");
        XCloseDisplay(display);
        return 1;
    }

    Window icon = XCreateSimpleWindow(display, root, 0, 0, 24, 24, 0, 0, 0);
    XSelectInput(display, icon, StructureNotifyMask);
    unsigned long info[2] = {0, 1};
    XChangeProperty(display, icon, xembed_info, xembed_info, 32, PropModeReplace,
                    (unsigned char *)info, 2);
    XEvent request;
    memset(&request, 0, sizeof(request));
    request.xclient.type = ClientMessage;
    request.xclient.window = owner;
    request.xclient.message_type = opcode;
    request.xclient.format = 32;
    request.xclient.data.l[1] = 0;
    request.xclient.data.l[2] = icon;
    XSendEvent(display, owner, False, NoEventMask, &request);
    XFlush(display);

    int embedded = 0;
    for(int i=0; i<100 && !embedded; i++){
        while(XPending(display)){
            XEvent event;
            XNextEvent(display, &event);
            if(event.type == ClientMessage && event.xclient.message_type == xembed &&
               event.xclient.data.l[1] == 0 && event.xclient.data.l[3] != None){
                embedded = 1;
            }
        }
        if(!embedded) pause_briefly();
    }
    if(!embedded) fprintf(stderr, "Icon did not receive XEMBED_EMBEDDED_NOTIFY\n");
    XDestroyWindow(display, icon);
    XCloseDisplay(display);
    return embedded ? 0 : 1;
}
