/*  gngeo a neogeo emulator
 *  Copyright (C) 2001 Peponas Mathieu
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "emu.h"

static struct {
	CONF_ITEM **conf;
	int size, nb_item;
} cf_hash[128];

void cf_cache_conf(void) {
}

static CONF_ITEM * create_conf_item(const char *name, const char *help, char short_opt, int (*action)(struct CONF_ITEM *self)) {
	int a;
	static int val = 0x100;
	CONF_ITEM *t = (CONF_ITEM*) calloc(1, sizeof (CONF_ITEM));

	a = tolower((int) name[0]);

	t->name = strdup(name);
	t->help = strdup(help);
	t->modified = 0;
	if (short_opt == 0) {
		val++;
		t->short_opt = val;
	} else
		t->short_opt = short_opt;

	if (action) {
		t->action = action;
	}

	if (cf_hash[a].size <= cf_hash[a].nb_item) {
		cf_hash[a].size += 10;
		cf_hash[a].conf = (CONF_ITEM**) realloc(cf_hash[a].conf, cf_hash[a].size * sizeof (CONF_ITEM*));
	}

	cf_hash[a].conf[cf_hash[a].nb_item] = t;
	cf_hash[a].nb_item++;
	return t;
}

/*
 * Create a boolean confirutation item
 * name: Name of conf item
 * help: Brief description of the item
 * short_opt: charatech use for short option
 * def: default value
 */
void cf_create_bool_item(const char *name, const char *help, char short_opt, int def) {
	CONF_ITEM *t = create_conf_item(name, help, short_opt, NULL);
	t->type = CFT_BOOLEAN;
	t->data.dt_bool.boolean = def;
	t->data.dt_bool.default_bool = def;
}

void cf_create_string_item(const char *name, const char *help, const char *hlp_arg, char short_opt, const char *def) {
	CONF_ITEM *t = create_conf_item(name, help, short_opt, NULL);
	t->type = CFT_STRING;
	strcpy(t->data.dt_str.str, def);
	t->data.dt_str.default_str = strdup(def);
	t->help_arg = (char *) hlp_arg;
}

void cf_create_int_item(const char *name, const char *help, const char *hlp_arg, char short_opt, int def) {
	CONF_ITEM *t = create_conf_item(name, help, short_opt, NULL);
	t->type = CFT_INT;
	t->data.dt_int.val = def;
	t->data.dt_int.default_val = def;
	t->help_arg = (char *) hlp_arg;
}

CONF_ITEM* cf_get_item_by_name(const char *name) {
	int i;
	int a = tolower((int) name[0]);

	for (i = 0; i < cf_hash[a].nb_item; i++) {
		if (strcasecmp(cf_hash[a].conf[i]->name, name) == 0)
			return cf_hash[a].conf[i];
	}
	return NULL;
}

CONF_ITEM* cf_get_item_by_val(int val) {
	int i, j;

	for (i = 0; i < 128; i++) {
		for (j = 0; j < cf_hash[i].nb_item; j++) {
			if (cf_hash[i].conf[j]->short_opt == val)
				return cf_hash[i].conf[j];
		}
	}
	return NULL;
}

void cf_init(void) {
}
