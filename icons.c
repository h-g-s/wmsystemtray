/* wmsystemtray
 * Copyright 2009-2010 Brad Jorsch <anomie@users.sourceforge.net>
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

#include <stdlib.h>
#include <X11/extensions/Xfixes.h>

#include "wmsystemtray.h"

static struct trayfuncs *get_type(const struct app_state *state, int id){
    if(id < 0 || id >= NUM_TYPES){
        warn(DEBUG_WARN, "Invalid tray icon type %d", id);
        return NULL;
    }
    return state->types[id];
}

Bool is_icon_parent(const struct app_state *state, Window w){
    for(int i=0; i<state->num_windows; i++){
        if(state->iconwin[i]==w) return True;
    }
    return False;
}

struct trayicon *icon_add(struct app_state *state, int type, Window w, void *data){
    if(state->exitapp) return NULL;
    if(!get_type(state, type)) return NULL;

    struct trayicon *icon = malloc(sizeof(struct trayicon));
    if(!icon){
        warn(DEBUG_ERROR, "Memory allocation failed");
        return NULL;
    }

    void *v=catch_BadWindow_errors();
    XFixesChangeSaveSet(state->display, w, SetModeInsert, SaveSetRoot, SaveSetUnmap);
    if(uncatch_BadWindow_errors(state, v)){
        warn(DEBUG_INFO, "Tray icon %lx is invalid, not adding", w);
        free(icon);
        return NULL;
    }

    warn(DEBUG_DEBUG, "Adding tray icon %lx of type %d", w, type);
    icon->type = type;
    icon->w = w;
    icon->data = data;
    icon->parent = None;
    icon->x = 0; icon->y = 0;
    icon->mapped = False;
    icon->visible = False;
    icon->next = NULL;
    struct trayicon **p;
    for(p = &state->icons; *p; p=&(*p)->next);
    *p = icon;
    state->need_update = True;
    return icon;
}

void icon_remove(struct app_state *state, Window w){
    struct trayicon *i, **n;
    n = &state->icons;
    while(*n){
        i = *n;
        if(i->w == w){
            *n = i->next;
            warn(DEBUG_DEBUG, "Removing tray icon %lx of type %d", i->w, i->type);
            struct trayfuncs *funcs = get_type(state, i->type);
            if(funcs && funcs->remove_icon) funcs->remove_icon(state, i);
            if(i->mapped){
                state->num_mapped_icons--;
                state->need_update = True;
            }
            free(i);
        } else {
            n = &i->next;
        }
    }
}

struct trayicon *icon_find(struct app_state *state, Window w){
    for(struct trayicon *i=state->icons; i; i=i->next){
        if(i->w == w) return i;
    }
    return NULL;
}

Bool icon_set_mapping(struct app_state *state, struct trayicon *icon, Bool map){
    if(icon->mapped == map) return True;

    warn(DEBUG_DEBUG, "%sapping tray icon %lx", map?"M":"Unm", icon->w);
    icon->mapped = map;
    state->num_mapped_icons += map?1:-1;
    state->need_update = True;
    return True;
}
