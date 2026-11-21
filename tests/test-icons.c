#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <X11/extensions/Xfixes.h>

#include "wmsystemtray.h"

static Bool bad_window;
static int removed;

void warn(int level, char *fmt, ...){
    (void)level;
    (void)fmt;
}

void *catch_BadWindow_errors(void){
    return NULL;
}

Bool uncatch_BadWindow_errors(struct app_state *state, void *previous){
    (void)state;
    (void)previous;
    return bad_window;
}

void XFixesChangeSaveSet(Display *display, Window w, int mode, int target, int map){
    (void)display;
    (void)w;
    (void)mode;
    (void)target;
    (void)map;
}

static void remove_icon(struct app_state *state, struct trayicon *icon){
    (void)state;
    assert(icon->data != NULL);
    removed++;
}

int main(void){
    struct trayfuncs funcs = { .remove_icon = remove_icon };
    struct app_state first = {0}, second = {0};
    int data = 1;
    first.types[0] = &funcs;
    second.types[0] = &funcs;

    bad_window = True;
    for(int i=0; i<100; i++){
        assert(icon_add(&first, 0, 42, &data) == NULL);
    }
    assert(first.icons == NULL);
    assert(!first.need_update);

    bad_window = False;
    struct trayicon *icon = icon_add(&first, 0, 42, &data);
    assert(icon != NULL);
    assert(icon_find(&first, 42) == icon);
    assert(icon_find(&second, 42) == NULL);
    assert(first.need_update);
    icon_set_mapping(&first, icon, True);
    icon_set_mapping(&first, icon, True);
    assert(first.num_mapped_icons == 1);
    icon_remove(&first, 42);
    assert(first.icons == NULL);
    assert(first.num_mapped_icons == 0);
    assert(removed == 1);
    return 0;
}
