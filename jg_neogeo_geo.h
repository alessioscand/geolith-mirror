/*
zlib License

Copyright (c) 2020-2025 Rupert Carmichael

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef JG_NEOGEO_H
#define JG_NEOGEO_H

enum jg_neogeo_input_type {
    JG_NEOGEO_UNCONNECTED,
    JG_NEOGEO_JS,
    JG_NEOGEO_SYS,
    JG_NEOGEO_MAHJONG,
    JG_NEOGEO_VLINER
};

static const char *jg_neogeo_input_name[] = {
    "Unconnected",
    "Neo Geo Joystick",
    "Neo Geo System",
    "Neo Geo Mahjong Controller",
    "V-Liner"
};

#define NDEFS_NEOGEOJS 10
static const char *defs_neogeojs[NDEFS_NEOGEOJS] = {
    "Up", "Down", "Left", "Right", "Select", "Start", "A", "B", "C", "D"
};

#define NDEFS_NEOGEOSYS 4
static const char *defs_neogeosys[NDEFS_NEOGEOSYS] = {
    "Coin1", "Coin2", "Service", "Test"
};

#define NDEFS_NEOGEOMJ 21
static const char *defs_neogeomj[NDEFS_NEOGEOMJ] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N",
    "Pon", "Chi", "Kan", "Reach", "Ron", "Select", "Start"
};

#define NDEFS_NEOGEOVLINER 11
static const char *defs_neogeovliner[NDEFS_NEOGEOVLINER] = {
    "Up", "Down", "Left", "Right", "Big", "Small", "D-Up", "Start",
    "Operator", "ClearCredit", "HopperOut"
};

static jg_inputinfo_t jg_neogeo_inputinfo(int index, int type) {
    jg_inputinfo_t ret;
    switch (type) {
        case JG_NEOGEO_JS: {
            ret.type = JG_INPUT_CONTROLLER;
            ret.index = index;
            ret.name = index == 0 ? "neogeojs1" : index == 1 ? "neogeojs2" :
                index == 2 ? "neogeojs3" : "neogeojs4";
            ret.fname = jg_neogeo_input_name[type];
            ret.defs = defs_neogeojs;
            ret.numaxes = 0;
            ret.numbuttons = NDEFS_NEOGEOJS;
            return ret;
        }
        case JG_NEOGEO_SYS: {
            ret.type = JG_INPUT_EXTERNAL;
            ret.index = index;
            ret.name = "neogeosystem";
            ret.fname = jg_neogeo_input_name[type];
            ret.defs = defs_neogeosys;
            ret.numaxes = 0;
            ret.numbuttons = NDEFS_NEOGEOSYS;
            return ret;
        }
        case JG_NEOGEO_MAHJONG: {
            ret.type = JG_INPUT_CONTROLLER;
            ret.index = index;
            ret.name = "neogeomahjong";
            ret.fname = jg_neogeo_input_name[type];
            ret.defs = defs_neogeomj;
            ret.numaxes = 0;
            ret.numbuttons = NDEFS_NEOGEOMJ;
            return ret;
        }
        case JG_NEOGEO_VLINER: {
            ret.type = JG_INPUT_CONTROLLER;
            ret.index = index;
            ret.name = "neogeovliner";
            ret.fname = jg_neogeo_input_name[type];
            ret.defs = defs_neogeovliner;
            ret.numaxes = 0;
            ret.numbuttons = NDEFS_NEOGEOVLINER;
            return ret;
        }
        default: {
            ret.type = JG_INPUT_EXTERNAL;
            ret.index = index;
            ret.name = "unconnected";
            ret.fname = jg_neogeo_input_name[0];
            ret.defs = NULL;
            ret.numaxes = ret.numbuttons = 0;
            return ret;
        }
    }
}

#endif
