/* wmsystemtray
 * Copyright © 2009-2010  Brad Jorsch <anomie@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/xpm.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <X11/Xmu/SysUtil.h>

#include "wmsystemtray.h"
#include "wmsystemtray.xpm"
#include "fdtray.h"

/* Global variables */
static volatile sig_atomic_t sigusr=0;

static struct app_state app = { .iconsize = 24, .num_windows = 1 };
static const char *PROGNAME=NULL;
static Bool nonwmaker=False;

static char *display_name=NULL;
static char *geometry="";
static int debug_level=DEBUG_WARN;
static int icons_per_row=2;
static int icons_per_col=2;
static Bool id_windows=False;
static Bool point_messages=False;
static int fill_style=0;
static int arrow_style=0;
//static Bool no_messages=False;

static Atom _NET_WM_PING;
static Atom WM_DELETE_WINDOW;
static Atom WM_PROTOCOLS;

static Pixmap pixmap;
static Window *mainwin=NULL;
static int current_page=0;
static Window down_window=None;
static int down_button=0;
static long selwindow_mask = PropertyChangeMask;
static GC gc10x20, gc5x8;
static char *fgcolor = NULL, *bgcolor = NULL;

/* X functions */
typedef int (*x_error_handler)(Display *, XErrorEvent *);
static x_error_handler orig_x_error_handler = NULL;
static Bool x_error_occurred = False;
static int ignore_BadWindow(Display *d, XErrorEvent *e){
    if(e->error_code != BadWindow) orig_x_error_handler(d,e);
    warn(DEBUG_INFO, "Ignoring BadWindow error");
    x_error_occurred = True;
    return 0;
}

void *catch_BadWindow_errors(){
    x_error_occurred = False;
    if(!orig_x_error_handler){
        orig_x_error_handler = XSetErrorHandler(&ignore_BadWindow);
        return (void *)orig_x_error_handler;
    } else {
        return (void *)XSetErrorHandler(&ignore_BadWindow);
    }
}

Bool uncatch_BadWindow_errors(struct app_state *state, void *v){
    XSync(state->display, False);
    XSetErrorHandler((x_error_handler)v);
    return x_error_occurred;
}

Time get_X_time(struct app_state *state){
    XEvent ev;
    XChangeProperty(state->display, state->selwindow, XA_WM_CLASS, XA_STRING, 8, PropModeAppend, NULL, 0);
    XWindowEvent(state->display, state->selwindow, PropertyChangeMask, &ev);
    warn(DEBUG_DEBUG, "X time is %ld", ev.xproperty.time);
    return ev.xproperty.time;
}

Bool icon_begin_message(Window w, int id, int length, int timeout){
    warn(DEBUG_INFO, "begin_icon_message called for window %lx id %d (length=%d timeout=%d)", w, id, length, timeout);
    // XXX
    return False;
}

Bool icon_message_data(Window w, int id, char *data, int datalen){
    warn(DEBUG_INFO, "icon_message_data called for window %lx id %d: %.*s", w, id, datalen, data);
    // XXX
    return False;
}

//static void icon_message_show(struct iconmessage *msg){
//    // XXX
//    // If point_messages is true, look up the icon and if it has x and y then set the corresponding hints
//}

void icon_cancel_message(Window w, int id){
    warn(DEBUG_INFO, "cancel_icon_message called for window %lx id %d", w, id);
    // XXX
}


/* Signal handler */
static void sighandler(int i){
    switch(i){
      case SIGINT:
      case SIGQUIT:
      case SIGKILL: // Useless, but...
      case SIGPIPE:
      case SIGTERM:
      case SIGABRT:
        app.exitapp=True;
        break;
      case SIGHUP:
        // Reload something?
        break;
      case SIGUSR1:
        sigusr--;
        break;
      case SIGUSR2:
        sigusr++;
        break;
    }
    signal(i,sighandler);
}

/* Display functions */
static void update(){
    int i, j, x, y, w;
    int icons_per_page=icons_per_row*icons_per_col*app.num_windows;
    char buf[42];
    Window dummy;
    int pages;

    app.need_update = False;

redo:
    pages = (app.num_mapped_icons-1)/icons_per_page+1;
    if(current_page >= pages){
        warn(DEBUG_DEBUG, "Page %d is empty!", current_page+1);
        current_page=pages-1;
    }
    warn(DEBUG_DEBUG, "Updating display: page %d of %d", current_page+1, pages);

    for(i = 0; i<app.num_windows; i++){
        warn(DEBUG_DEBUG, "Drawing window %d (%lx)", i, app.iconwin[i]);
        if(arrow_style==0 ||
           (arrow_style==1 && i==0) ||
           (arrow_style==2 && i==app.num_windows-1)){
            y=(current_page==0)?0:(app.iconwin[i]==down_window && down_button==-1)?20:10;
            XCopyArea(app.display, pixmap, app.iconwin[i], gc5x8, 0,y, 15,10, 4,52);
        }
        if(arrow_style==0 ||
           (arrow_style==1 && i==app.num_windows-1) ||
           (arrow_style==2 && i==app.num_windows-1)){
            sprintf(buf, "%d/%d", current_page+1, pages);
            x = strlen(buf);
            XClearArea(app.display, app.iconwin[i], 19,52, 25,8, False);
            XDrawString(app.display, app.iconwin[i], gc5x8, (current_page>8)?19:24,60, buf, x);
            y=(current_page+1>=pages)?0:(app.iconwin[i]==down_window && down_button==1)?20:10;
            XCopyArea(app.display, pixmap, app.iconwin[i], gc5x8, 15,y, 15,10, 45,52);
        }
        if(id_windows){
            sprintf(buf, "%d", i);
            XDrawString(app.display, app.iconwin[i], gc10x20, 8,50, buf, strlen(buf));
        }
    }

    struct trayicon *icon = app.icons;
    i=-current_page*icons_per_page;
    while(icon){
        void *v=catch_BadWindow_errors();
        if(!icon->mapped || i<0 || i>=icons_per_page){
            warn(DEBUG_DEBUG, "Tray icon %lx is not visible", icon->w);
            icon->visible = False;
            if(icon->parent == None){
                // Parent it somewhere
                warn(DEBUG_DEBUG, "Reparenting %lx to %lx", icon->w, app.iconwin[0]);
                XReparentWindow(app.display, icon->w, app.iconwin[0], 0, 0);
                icon->parent = app.iconwin[0];
            }
            XUnmapWindow(app.display, icon->w);
        } else {
            icon->visible = True;
            j = i;
            switch(fill_style){
              case 0:
                w = j / (icons_per_row * icons_per_col);
                j = j % (icons_per_row * icons_per_col);
                y = j / icons_per_row;
                x = j % icons_per_row;
                break;
              case 1:
                y = j / (icons_per_row * app.num_windows);
                j = j % (icons_per_row * app.num_windows);
                w = j / icons_per_row;
                x = j % icons_per_row;
                break;
              default:
                x=0; y=0; w=0;
                break;
            }
            x=8+x*app.iconsize;
            y=4+y*app.iconsize;
            warn(DEBUG_DEBUG, "[%d] Tray icon %lx at %d %d,%d", i, icon->w, w, x, y);
            if(icon->parent != app.iconwin[w]){
                warn(DEBUG_DEBUG, "Reparenting %lx to %lx", icon->w, app.iconwin[w]);
                XReparentWindow(app.display, icon->w, app.iconwin[w], x, y);
                icon->parent = app.iconwin[w];
            }
            XMoveResizeWindow(app.display, icon->w, x, y, app.iconsize, app.iconsize);
            XClearArea(app.display, icon->w, 0, 0, 0, 0, True);
            XMapRaised(app.display, icon->w);
            XTranslateCoordinates(app.display, icon->w, app.root, app.iconsize/2, app.iconsize/2, &icon->x, &icon->y, &dummy);
        }
        if(uncatch_BadWindow_errors(&app, v)){
            warn(DEBUG_INFO, "Tray icon %lx is invalid, removing and restarting layout", icon->w);
            icon_remove(&app, icon->w);
            goto redo;
        }
        if(icon->mapped) i++;
        icon = icon->next;
    }

    // XXX: if point_messages is true, can we re-point any notifications?
}

void selwindow_add_mask(struct app_state *state, long mask){
    selwindow_mask |= mask;
    warn(DEBUG_DEBUG, "selwindow mask is now %lx", selwindow_mask);
    XSelectInput(state->display, state->selwindow, selwindow_mask);
}

static void create_dock_windows(int argc, char **argv){
    XClassHint     *classHint;
    XWMHints       *wmHints;
    XSizeHints     *sizeHints;
    Atom           wmProtocols[2];
    char           hostname[256];
    XTextProperty  machineName;
    XRectangle     rects[2] = {
        { .x = 8, .y = 4, .width = 48, .height = 48 },
        { .x = 4, .y = 52, .width = 56, .height = 10 }
    };
    Atom           _NET_WM_PID;
    XGCValues      gcv;
    unsigned long  gcm;
    char           buf[1024];
    int            err, dummy=0, pid;
    Pixel          fgpix, bgpix;

    pid = getpid();
    warn(DEBUG_DEBUG, "My pid is %d", pid);

    warn(DEBUG_INFO, "Opening display '%s'", XDisplayName(display_name));
    if(!(app.display = XOpenDisplay(display_name)))
        die("Can't open display %s", XDisplayName(display_name));
    app.screen = DefaultScreen(app.display);
    app.root = RootWindow(app.display, app.screen);

    fgpix = BlackPixel(app.display, app.screen);
    bgpix = WhitePixel(app.display, app.screen);
    if(fgcolor != NULL){
        warn(DEBUG_DEBUG, "Allocating colormap entry for fgcolor '%s'", fgcolor);
        XColor color;
        XWindowAttributes a;
        XGetWindowAttributes(app.display, app.root, &a);
        color.pixel = 0;
        if(!XParseColor(app.display, a.colormap, fgcolor, &color)){
            warn(DEBUG_ERROR, "Specified foreground color '%s' could not be parsed, using Black", fgcolor);
            fgcolor = NULL;
        } else if(!XAllocColor(app.display, a.colormap, &color)) {
            warn(DEBUG_ERROR, "Cannot allocate colormap entry for foreground color '%s', using Black", fgcolor);
            fgcolor = NULL;
        } else {
            fgpix = color.pixel;
        }
    }
    if(bgcolor != NULL){
        warn(DEBUG_DEBUG, "Allocating colormap entry for bgcolor '%s'", bgcolor);
        XColor color;
        XWindowAttributes a;
        XGetWindowAttributes(app.display, app.root, &a);
        color.pixel = 0;
        if(!XParseColor(app.display, a.colormap, bgcolor, &color)){
            warn(DEBUG_ERROR, "Specified background color '%s' could not be parsed, using default", bgcolor);
            bgcolor = NULL;
        } else if(!XAllocColor(app.display, a.colormap, &color)) {
            warn(DEBUG_ERROR, "Cannot allocate colormap entry for background color '%s', using default", bgcolor);
            bgcolor = NULL;
        } else {
            bgpix = color.pixel;
        }
    }

    warn(DEBUG_DEBUG, "Interning atoms");
    _NET_WM_PING = XInternAtom(app.display, "_NET_WM_PING", False);
    _NET_WM_PID = XInternAtom(app.display, "_NET_WM_PID", False);
    WM_PROTOCOLS = XInternAtom(app.display, "WM_PROTOCOLS", False);
    WM_DELETE_WINDOW = XInternAtom(app.display, "WM_DELETE_WINDOW", False);

    /* Load images */
    warn(DEBUG_DEBUG, "Loading XPM");
    err = XpmCreatePixmapFromData(app.display, app.root, wmsystemtray_xpm, &pixmap, NULL, NULL);
    if(err != XpmSuccess) die("Could not load xpm (%d)", err);

    /* Create hints */
    warn(DEBUG_DEBUG, "Allocating window hints");
    sizeHints = XAllocSizeHints();
    classHint = XAllocClassHint();
    wmHints = XAllocWMHints();
    if(!sizeHints || !classHint || !wmHints) die("Memory allocation failed");

    sizeHints->flags = USSize | USPosition | PSize | PBaseSize | PMinSize | PMaxSize;
    sizeHints->x = 0;
    sizeHints->y = 0;
    sizeHints->width = 64;
    sizeHints->height = 64;
    sizeHints->base_width = sizeHints->width;
    sizeHints->base_height = sizeHints->height;
    sizeHints->min_width = 64;
    sizeHints->min_height = 64;
    sizeHints->max_width = 64;
    sizeHints->max_height = 64;
    warn(DEBUG_DEBUG, "Parsing geometry string '%s'", geometry);
    XWMGeometry(app.display, app.screen, geometry, NULL, 1, sizeHints,
                &sizeHints->x, &sizeHints->y, &sizeHints->width, &sizeHints->height, &dummy);
    sizeHints->base_width = sizeHints->width;
    sizeHints->base_height = sizeHints->height;
    warn(DEBUG_DEBUG,"%d %d %d %d", sizeHints->x, sizeHints->y, sizeHints->base_width, sizeHints->base_height);

    classHint->res_class = "wmsystemtray";
    classHint->res_name = buf;

    if(nonwmaker){
        wmHints->flags = StateHint | WindowGroupHint;
        wmHints->initial_state = NormalState;
    } else {
        wmHints->flags = StateHint | IconWindowHint | IconPositionHint | WindowGroupHint;
        wmHints->initial_state = WithdrawnState;
        wmHints->icon_x = sizeHints->x;
        wmHints->icon_y = sizeHints->y;
    }

    wmProtocols[0] = _NET_WM_PING;
    wmProtocols[1] = WM_DELETE_WINDOW;

    machineName.encoding = XA_STRING;
    machineName.format = 8;
    machineName.nitems = XmuGetHostname(hostname, sizeof(hostname));
    machineName.value = (unsigned char *) hostname;

    /* Create windows */
    warn(DEBUG_DEBUG, "Allocating space for %d windows", app.num_windows);
    mainwin = malloc(app.num_windows * sizeof(*mainwin));
    app.iconwin = nonwmaker?mainwin:malloc(app.num_windows * sizeof(*app.iconwin));
    if(!mainwin || !app.iconwin) die("Memory allocation failed");

    warn(DEBUG_DEBUG, "Creating selection window");
    app.selwindow = XCreateSimpleWindow(app.display, app.root, -1,-1,1,1,0,0,0);
    XSelectInput(app.display, app.selwindow, selwindow_mask);
    strncpy(buf, "selwindow", sizeof(buf));
    XSetClassHint(app.display, app.selwindow, classHint);
    XStoreName(app.display, app.selwindow, PROGNAME);
    XSetCommand(app.display, app.selwindow, argv, argc);
    XSetWMProtocols(app.display, app.selwindow, wmProtocols, 2);
    XSetWMClientMachine(app.display, app.selwindow, &machineName);
    XChangeProperty(app.display, app.selwindow, _NET_WM_PID, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
    XShapeCombineRectangles(app.display, app.selwindow, ShapeBounding, 0, 0, NULL, 0, ShapeSet, YXBanded);

    warn(DEBUG_DEBUG, "Creating GCs");
    gcm = GCForeground | GCBackground | GCGraphicsExposures | GCFont;
    gcv.foreground = fgpix;
    gcv.background = bgpix;
    gcv.graphics_exposures = True;
    gcv.font = XLoadFont(app.display, "10x20");
    gc10x20 = XCreateGC(app.display, app.root, gcm, &gcv);
    gcv.font = XLoadFont(app.display, "5x8");
    gc5x8 = XCreateGC(app.display, app.root, gcm, &gcv);

    for(int i=0; i<app.num_windows; i++){
        if(nonwmaker){
            app.iconwin[i] = XCreateSimpleWindow(app.display, app.root, sizeHints->x, sizeHints->y, sizeHints->width, sizeHints->height, 1, fgpix, bgpix);
        } else {
            mainwin[i] = XCreateSimpleWindow(app.display, app.root, -1,-1,1,1,0,0,0);
            app.iconwin[i] = XCreateSimpleWindow(app.display, mainwin[i], sizeHints->x, sizeHints->y, sizeHints->width, sizeHints->height, 1, fgpix, bgpix);
        }
        warn(DEBUG_DEBUG, "Dock window #%d is %lx", i, app.iconwin[i]);

        XSetWMNormalHints(app.display, mainwin[i], sizeHints);
        XSetWMNormalHints(app.display, app.iconwin[i], sizeHints);

        snprintf(buf, sizeof(buf), "%s%d", PROGNAME, i);
        XSetClassHint(app.display, mainwin[i], classHint);
        XSetClassHint(app.display, app.iconwin[i], classHint);

        XSelectInput(app.display, app.iconwin[i], ButtonPressMask | ExposureMask | ButtonReleaseMask | StructureNotifyMask | VisibilityChangeMask);

        XStoreName(app.display, mainwin[i], PROGNAME);
        XStoreName(app.display, app.iconwin[i], PROGNAME);
        XSetCommand(app.display, mainwin[i], argv, argc);
        XSetCommand(app.display, app.iconwin[i], argv, argc);

        if(!nonwmaker) wmHints->icon_window = app.iconwin[i];
        wmHints->window_group = mainwin[i];
        XSetWMHints(app.display, mainwin[i], wmHints);

        XSetWMProtocols(app.display, mainwin[i], wmProtocols, 2);
        XSetWMProtocols(app.display, app.iconwin[i], wmProtocols, 2);

        XSetWMClientMachine(app.display, mainwin[i], &machineName);
        XChangeProperty(app.display, mainwin[i], _NET_WM_PID, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);
        XSetWMClientMachine(app.display, app.iconwin[i], &machineName);
        XChangeProperty(app.display, app.iconwin[i], _NET_WM_PID, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&pid, 1);

        if(!nonwmaker || bgcolor == NULL){
            XSetWindowBackgroundPixmap(app.display, app.iconwin[i], ParentRelative);
            XShapeCombineRectangles(app.display, app.iconwin[i], ShapeBounding, 0, 0, rects, 2, ShapeSet, YXBanded);
        }

        XMapWindow(app.display, mainwin[i]);
    }

    warn(DEBUG_DEBUG, "Freeing X hints");
    XFree(wmHints);
    XFree(sizeHints);
    XFree(classHint);

    update();
}

static void cleanup(){
    warn(DEBUG_INFO, "Cleaning up for exit");
    for(int i=0; i<NUM_TYPES; i++){
        if(app.types[i] && app.types[i]->closing) app.types[i]->closing(&app);
    }
    warn(DEBUG_DEBUG, "Removing all icons");
    while(app.icons) icon_remove(&app, app.icons->w);
    warn(DEBUG_DEBUG, "Deinitializing protocols");
    for(int i=0; i<NUM_TYPES; i++){
        if(app.types[i] && app.types[i]->deinit) app.types[i]->deinit(&app);
    }
    warn(DEBUG_DEBUG, "Closing display");
    if(mainwin) free(mainwin);
    if(app.iconwin && !nonwmaker) free(app.iconwin);
    if(app.display) XCloseDisplay(app.display);
}

/* Error and warning functions */
void warn(int level, char *fmt, ...){
    va_list ap;

    if(debug_level<level) return;
    va_start(ap, fmt);
    fprintf(stderr, "%s: ", PROGNAME);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

void die(char *fmt, ...){
    va_list ap;

    cleanup();

    va_start(ap, fmt);
    fprintf(stderr, "%s: ", PROGNAME);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static void usage(int exitcode) __attribute__ ((noreturn));
static void usage(int exitcode){
    fprintf(stderr, "USAGE: %s [<options>]\n", PROGNAME);
    fprintf(stderr, "  -display <display>    X display to connect to\n");
    fprintf(stderr, "  -geometry <geom>      Initial window geometry\n");
    fprintf(stderr, "      --help            This message\n");
    fprintf(stderr, "  -V, --version         Print the version number and exit\n");
    fprintf(stderr, "  -v, --verbose         Print more messages (can be repeated)\n");
    fprintf(stderr, "  -q, --quiet           Print fewer messages (can be repeated)\n");
    fprintf(stderr, "  -s, --small           Use small (16x16) icons\n");
    fprintf(stderr, "  -w, --windows <n>     Number of dock windows to create\n");
    fprintf(stderr, "      --id-windows      Identify the individual windows\n");
//    fprintf(stderr, "  -P, --point-messages  If an icon sends a popup message through the systray\n                        protocol, have the notification point to the icon");
    fprintf(stderr, "      --fill-rows       Fill the top row of all windows first\n");
    fprintf(stderr, "      --arrows <place>  How to place the arrows: all, horizontal, vertical\n");
    fprintf(stderr, "  -c, --fgcolor <color> Text color used, default is black\n");
    fprintf(stderr, "      --bgcolor <color> The window background color in non-Window Maker mode\n");
    fprintf(stderr, "      --non-wmaker      Use in a non-Window Maker window manager\n");
    fprintf(stderr, "\n");
    exit(exitcode);
}

static int parse_args(int argc, char **argv){
    char *c;

    if((PROGNAME=strrchr(argv[0], '/'))==NULL || !PROGNAME[1])
        PROGNAME=argv[0];
    else PROGNAME++;

    /* Parse command line args */
    int j=1;
    for(int i=1; i<argc; i++){
        argv[j++]=argv[i];
        if(!strcmp(argv[i],"-display") || !strcmp(argv[i],"--display")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            display_name=argv[++i];
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"-geometry") || !strcmp(argv[i],"--geometry")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            geometry=argv[++i];
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"--help")){
            usage(0);
        } else if(!strcmp(argv[i],"-V") || !strcmp(argv[i],"--version")){
            printf("%s\n", VERSION);
            exit(0);
        } else if(!strcmp(argv[i],"-v") || !strcmp(argv[i],"--verbose")){
            debug_level++;
        } else if(!strcmp(argv[i],"-q") || !strcmp(argv[i],"--quiet")){
            debug_level--;
        } else if(!strcmp(argv[i],"--non-wmaker")){
            nonwmaker=True;
        } else if(!strcmp(argv[i],"-s") || !strcmp(argv[i],"--small")){
            app.iconsize=16;
            icons_per_row=3;
            icons_per_col=3;
        } else if(!strcmp(argv[i],"-w") || !strcmp(argv[i],"--windows")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            app.num_windows=strtol(argv[i+1],&c,0);
            if(app.num_windows<1 || *c) die("%s requires a positive integer", argv[i]);
            i++;
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"--id-windows")){
            id_windows=True;
            j--;
        } else if(!strcmp(argv[i],"-P") || !strcmp(argv[i],"--point-messages")){
            point_messages=True;
        } else if(!strcmp(argv[i],"--fill-rows")){
            fill_style=1;
        } else if(!strcmp(argv[i],"--arrows")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            if(!strcmp(argv[i+1],"all")){
                arrow_style=0;
            } else if(!strcmp(argv[i+1],"horizontal")){
                arrow_style=1;
            } else if(!strcmp(argv[i+1],"vertical")){
                arrow_style=2;
            } else {
                die("Invalid value for %s", argv[i]);
            }
            i++;
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"-c") || !strcmp(argv[i],"--fgcolor")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            fgcolor = argv[++i];
            argv[j++]=argv[i];
        } else if(!strcmp(argv[i],"--bgcolor")){
            if(i+1 >= argc) die("%s requires an argument", argv[i]);
            bgcolor = argv[++i];
            argv[j++]=argv[i];
        } else {
            debug_level=DEBUG_ERROR;
            warn(DEBUG_ERROR, "Unrecognized command line argument %s", argv[i]);
            usage(1);
        }
    }
    return j;
}

int main(int argc, char *argv[]){
    warn(DEBUG_DEBUG, "Parsing args");
    argc = parse_args(argc, argv);

    warn(DEBUG_DEBUG, "Creating windows");
    create_dock_windows(argc, argv);

    warn(DEBUG_DEBUG, "Initializing protocols");
    for(int i=0; i<NUM_TYPES; i++) app.types[i]=NULL;
    if(!(app.types[0] = fdtray_init(&app, 0)))
        die("Could not initialize the freedesktop.org tray protocol");

    warn(DEBUG_DEBUG, "Setting signal handlers");
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGKILL, sighandler); // Useless, but...
    signal(SIGPIPE, sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGABRT, sighandler);
    signal(SIGHUP, sighandler);
    signal(SIGUSR1, sighandler);
    signal(SIGUSR2, sighandler);

    XEvent ev;
    int fd;
    fd_set rfds;
    warn(DEBUG_DEBUG, "Entering main loop");
    while(!app.exitapp){
        if (sigusr) {
            warn(DEBUG_INFO, "Handling SIGUSR1/SIGUSR2, delta = %d", sigusr);
            int pages = (app.num_mapped_icons-1)/icons_per_row/icons_per_col/app.num_windows + 1;
            current_page += sigusr;
            sigusr = 0;
            while(current_page < 0) current_page += pages;
            while(current_page >= pages) current_page -= pages;
            app.need_update=True;
        }

        while(XPending(app.display)){
            struct trayicon *icon = NULL;
            XNextEvent(app.display, &ev);
            warn(DEBUG_DEBUG, "Got X event %d", ev.type);
            switch(ev.type){
              case GraphicsExpose:
              case Expose:
              case MapRequest:
                app.need_update=True;
                break;

              case MapNotify:
                icon = icon_find(&app, ev.xmap.window);
                if(icon && !icon->visible){
                    warn(DEBUG_WARN, "A poorly-behaved application tried to map window %lx!", ev.xmap.window);
                    app.need_update=True;
                }
                break;

              case UnmapNotify:
                icon = icon_find(&app, ev.xunmap.window);
                if(icon && icon->visible){
                    warn(DEBUG_WARN, "A poorly-behaved application tried to unmap window %lx!", ev.xmap.window);
                    app.need_update=True;
                }
                break;

              case DestroyNotify:
                if(app.exitapp) break;
                if(app.selwindow==ev.xdestroywindow.window){
                    warn(DEBUG_WARN, "Selection window %lx destroyed!", ev.xdestroywindow.window);
                    app.exitapp=1;
                }
                for(int i=0; !app.exitapp && i<app.num_windows; i++){
                    if(mainwin[i]==ev.xdestroywindow.window || app.iconwin[i]==ev.xdestroywindow.window){
                        warn(DEBUG_WARN, "Window %lx destroyed!", ev.xdestroywindow.window);
                        app.exitapp=1;
                    }
                }
                break;

              case ButtonPress:
                switch (ev.xbutton.button) {
                  case 1:
                    down_window = ev.xbutton.window;
                    down_button = 0;
                    if(ev.xbutton.y >= 53 && ev.xbutton.y < 61){
                        if(ev.xbutton.x >= 5 && ev.xbutton.x < 18){
                            warn(DEBUG_INFO, "Left button mouse down");
                            down_button = -1;
                        } else if(ev.xbutton.x >= 46 && ev.xbutton.x < 59){
                            warn(DEBUG_INFO, "Right button mouse down");
                            down_button = 1;
                        }
                    }
                    app.need_update = True;
                    break;
                  case 4:
                  case 6:
                    warn(DEBUG_INFO, "Left/Up scroll wheel");
                    if(current_page > 0){
                        current_page--;
                        app.need_update=True;
                    }
                    break;
                  case 5:
                  case 7:
                    warn(DEBUG_INFO, "Right/Down scroll wheel");
                    if(current_page < (app.num_mapped_icons-1)/icons_per_row/icons_per_col/app.num_windows){
                        current_page++;
                        app.need_update=True;
                    }
                    break;
                }
                break;

              case ButtonRelease:
                switch (ev.xbutton.button) {
                  case 1:
                    if(ev.xbutton.y >= 53 && ev.xbutton.y < 61){
                        if(ev.xbutton.x >= 5 && ev.xbutton.x < 18 && down_button == -1){
                            warn(DEBUG_INFO, "Left button mouse up");
                            if(current_page > 0){
                                current_page--;
                                app.need_update=True;
                            }
                        } else if(ev.xbutton.x >= 46 && ev.xbutton.x < 59 && down_button == 1){
                            warn(DEBUG_INFO, "Right button mouse up");
                            if(current_page < (app.num_mapped_icons-1)/icons_per_row/icons_per_col/app.num_windows){
                                current_page++;
                                app.need_update=True;
                            }
                        }
                    }
                    down_window = None;
                    down_button = 0;
                    app.need_update = True;
                    break;
                }
                break;

              case ClientMessage:
                if(ev.xclient.message_type == WM_PROTOCOLS && ev.xclient.format == 32){
                    if(ev.xclient.data.l[0] == _NET_WM_PING){
                        warn(DEBUG_DEBUG, "_NET_WM_PING!");
                        ev.xclient.window = app.root;
                        XSendEvent(app.display, app.root, False, SubstructureNotifyMask|SubstructureRedirectMask, &ev);
                    } else if(ev.xclient.data.l[0] == WM_DELETE_WINDOW){
                        warn(DEBUG_DEBUG, "WM_DELETE_WINDOW called for %lx!", ev.xclient.window);
                        app.exitapp=1;
                    }
                }
                break;
            }
            for(int i=0; i<NUM_TYPES; i++){
                if(app.types[i]->handle_event) app.types[i]->handle_event(&app, &ev);
            }
        }
        if(app.exitapp) break;
        warn(DEBUG_DEBUG, "Need update? %s", app.need_update?"Yes":"No");
        if(app.need_update) update();

        if(XPending(app.display)) continue;
        fd=ConnectionNumber(app.display);
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        select(fd+1, &rfds, NULL, NULL, NULL);
    }
    warn(DEBUG_DEBUG, "Main loop ended");

    cleanup();

    warn(DEBUG_DEBUG, "Exiting app");
    return 0;
}
