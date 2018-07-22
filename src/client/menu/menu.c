/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 *
 * This file implements the non generic part of the menu system, e.g.
 * the menu shown to the player. Beware! This code is very fragile and
 * should only be touched with great care and exessive testing.
 * Otherwise strange things and hard to track down bugs can occure. In a
 * better world someone would rewrite this file to something more like
 * Quake III Team Arena.
 *
 * =======================================================================
 */

#include <ctype.h>
#include "../header/client.h"
#include "header/qmenu.h"

static int m_main_cursor;

/* Number of the frames of the spinning quake logo */
#define NUM_CURSOR_FRAMES 15

static char *menu_in_sound = "misc/menu1.wav";
static char *menu_move_sound = "misc/menu2.wav";
static char *menu_out_sound = "misc/menu3.wav";

void M_Menu_Main_f(void);

static void M_Menu_Game_f(void);

static void M_Menu_PlayerConfig_f(void);

static void M_Menu_Multiplayer_f(void);

static void M_Menu_JoinServer_f(void);

static void M_Menu_AddressBook_f(void);

static void M_Menu_StartServer_f(void);

static void M_Menu_DMOptions_f(void);

static void M_Menu_Video_f(void);

static void M_Menu_Options_f(void);

static void M_Menu_Keys_f(void);

static void M_Menu_Quit_f(void);

qboolean m_entersound; /* play after drawing a frame, so caching won't disrupt
                          the sound */

void (*m_drawfunc)(void);

const char *(*m_keyfunc)(int key);

/* Maximal number of submenus */
#define MAX_MENU_DEPTH 8

typedef struct
{
  void (*draw)(void);

  const char *(*key)(int k);
} menulayer_t;

menulayer_t m_layers[MAX_MENU_DEPTH];
int m_menudepth;

static void M_Banner(char *name)
{
  int w, h;
  float scale = SCR_GetMenuScale();

  Draw_GetPicSize(&w, &h, name);
  Draw_PicScaled(viddef.width / 2 - (w * scale) / 2, viddef.height / 2 - (110 * scale), name, scale);
}

void M_ForceMenuOff(void)
{
  m_drawfunc = NULL;
  m_keyfunc = NULL;
  cls.key_dest = key_game;
  m_menudepth = 0;
  Key_MarkAllUp();
  Cvar_Set("paused", "0");
}

void M_PopMenu(void)
{
  S_StartLocalSound(menu_out_sound);

  if (m_menudepth < 1) {
    Com_Error(ERR_FATAL, "M_PopMenu: depth < 1");
  }

  m_menudepth--;

  m_drawfunc = m_layers[m_menudepth].draw;
  m_keyfunc = m_layers[m_menudepth].key;

  if (!m_menudepth) {
    M_ForceMenuOff();
  }
}

/*
 * This crappy function maintaines a stack of opened menus.
 * The steps in this horrible mess are:
 *
 * 1. But the game into pause if a menu is opened
 *
 * 2. If the requested menu is already open, close it.
 *
 * 3. If the requested menu is already open but not
 *    on top, close all menus above it and the menu
 *    itself. This is necessary since an instance of
 *    the reqeuested menu is in flight and will be
 *    displayed.
 *
 * 4. Save the previous menu on top (which was in flight)
 *    to the stack and make the requested menu the menu in
 *    flight.
 */
static void M_PushMenu(void (*draw)(void), const char *(*key)(int) )
{
  int i;
  int alreadyPresent = 0;

  if ((Cvar_VariableValue("maxclients") == 1) && Com_ServerState()) {
    Cvar_Set("paused", "1");
  }

  /* if this menu is already open (and on top),
     close it => toggling behaviour */
  if ((m_drawfunc == draw) && (m_keyfunc == key)) {
    M_PopMenu();
    return;
  }

  /* if this menu is already present, drop back to
     that level to avoid stacking menus by hotkeys */
  for (i = 0; i < m_menudepth; i++) {
    if ((m_layers[i].draw == draw) && (m_layers[i].key == key)) {
      alreadyPresent = 1;
      break;
    }
  }

  /* menu was already opened further down the stack */
  while (alreadyPresent && i <= m_menudepth) {
    M_PopMenu(); /* decrements m_menudepth */
  }

  if (m_menudepth >= MAX_MENU_DEPTH) {
    Com_Printf("Too many open menus!\n");
    return;
  }

  m_layers[m_menudepth].draw = m_drawfunc;
  m_layers[m_menudepth].key = m_keyfunc;
  m_menudepth++;

  m_drawfunc = draw;
  m_keyfunc = key;

  m_entersound = true;

  cls.key_dest = key_menu;
}

int Key_GetMenuKey(int key)
{
  switch (key) {
  case K_KP_UPARROW:
  case K_UPARROW:
  case K_HAT_UP:
    return K_UPARROW;

  case K_TAB:
  case K_KP_DOWNARROW:
  case K_DOWNARROW:
  case K_HAT_DOWN:
    return K_DOWNARROW;

  case K_KP_LEFTARROW:
  case K_LEFTARROW:
  case K_HAT_LEFT:
  case K_TRIG_LEFT:
    return K_LEFTARROW;

  case K_KP_RIGHTARROW:
  case K_RIGHTARROW:
  case K_HAT_RIGHT:
  case K_TRIG_RIGHT:
    return K_RIGHTARROW;

  case K_MOUSE1:
  case K_MOUSE2:
  case K_MOUSE3:
  case K_MOUSE4:
  case K_MOUSE5:

  case K_JOY1:
  case K_JOY2:
  case K_JOY3:
  case K_JOY4:
  case K_JOY5:
  case K_JOY6:
  case K_JOY7:
  case K_JOY8:
  case K_JOY9:
  case K_JOY10:
  case K_JOY11:
  case K_JOY12:
  case K_JOY13:
  case K_JOY14:
  case K_JOY15:
  case K_JOY16:
  case K_JOY17:
  case K_JOY18:
  case K_JOY19:
  case K_JOY20:
  case K_JOY21:
  case K_JOY22:
  case K_JOY23:
  case K_JOY24:
  case K_JOY25:
  case K_JOY26:
  case K_JOY27:
  case K_JOY28:
  case K_JOY29:
  case K_JOY30:
  case K_JOY31:

  case K_AUX1:
  case K_AUX2:
  case K_AUX3:
  case K_AUX4:
  case K_AUX5:
  case K_AUX6:
  case K_AUX7:
  case K_AUX8:
  case K_AUX9:
  case K_AUX10:
  case K_AUX11:
  case K_AUX12:
  case K_AUX13:
  case K_AUX14:
  case K_AUX15:
  case K_AUX16:
  case K_AUX17:
  case K_AUX18:
  case K_AUX19:
  case K_AUX20:
  case K_AUX21:
  case K_AUX22:
  case K_AUX23:
  case K_AUX24:
  case K_AUX25:
  case K_AUX26:
  case K_AUX27:
  case K_AUX28:
  case K_AUX29:
  case K_AUX30:
  case K_AUX31:
  case K_AUX32:

  case K_KP_ENTER:
  case K_ENTER:
    return K_ENTER;

  case K_ESCAPE:
  case K_JOY_BACK:
    return K_ESCAPE;
  }

  return key;
}

const char *Default_MenuKey(menuframework_s *m, int key)
{
  const char *sound = NULL;
  menucommon_s *item;
  int menu_key = Key_GetMenuKey(key);

  if (m) {
    if ((item = Menu_ItemAtCursor(m)) != 0) {
      if (item->type == MTYPE_FIELD) {
        if (Field_Key((menufield_s *) item, key)) {
          return NULL;
        }
      }
    }
  }

  switch (menu_key) {
  case K_ESCAPE:
    M_PopMenu();
    return menu_out_sound;

  case K_UPARROW:
    if (m) {
      m->cursor--;
      Menu_AdjustCursor(m, -1);
      sound = menu_move_sound;
    }
    break;

  case K_DOWNARROW:
    if (m) {
      m->cursor++;
      Menu_AdjustCursor(m, 1);
      sound = menu_move_sound;
    }
    break;

  case K_LEFTARROW:
    if (m) {
      Menu_SlideItem(m, -1);
      sound = menu_move_sound;
    }
    break;

  case K_RIGHTARROW:
    if (m) {
      Menu_SlideItem(m, 1);
      sound = menu_move_sound;
    }
    break;

  case K_ENTER:
    if (m) {
      Menu_SelectItem(m);
    }
    sound = menu_move_sound;
    break;
  }

  return sound;
}

/*
 * Draws one solid graphics character cx and cy are in 320*240
 * coordinates, and will be centered on higher res screens.
 */
static void M_DrawCharacter(int cx, int cy, int num)
{
  float scale = SCR_GetMenuScale();
  Draw_CharScaled(cx + ((int) (viddef.width - 320 * scale) >> 1), cy + ((int) (viddef.height - 240 * scale) >> 1), num,
                  scale);
}

static void M_Print(int x, int y, char *str)
{
  int cx, cy;
  float scale = SCR_GetMenuScale();

  cx = x;
  cy = y;
  while (*str) {
    if (*str == '\n') {
      cx = x;
      cy += 8;
    } else {
      M_DrawCharacter(cx * scale, cy * scale, (*str) + 128);
      cx += 8;
    }
    str++;
  }
}

/*
 * Draws an animating cursor with the point at
 * x,y. The pic will extend to the left of x,
 * and both above and below y.
 */
static void M_DrawCursor(int x, int y, int f)
{
  char cursorname[80];
  static qboolean cached;
  float scale = SCR_GetMenuScale();

  if (!cached) {
    int i;

    for (i = 0; i < NUM_CURSOR_FRAMES; i++) {
      Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", i);

      Draw_FindPic(cursorname);
    }

    cached = true;
  }

  Com_sprintf(cursorname, sizeof(cursorname), "m_cursor%d", f);
  Draw_PicScaled(x * scale, y * scale, cursorname, scale);
}

static void M_DrawTextBox(int x, int y, int width, int lines)
{
  int cx, cy;
  int n;
  float scale = SCR_GetMenuScale();

  /* draw left side */
  cx = x;
  cy = y;
  M_DrawCharacter(cx * scale, cy * scale, 1);

  for (n = 0; n < lines; n++) {
    cy += 8;
    M_DrawCharacter(cx * scale, cy * scale, 4);
  }

  M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 7);

  /* draw middle */
  cx += 8;

  while (width > 0) {
    cy = y;
    M_DrawCharacter(cx * scale, cy * scale, 2);

    for (n = 0; n < lines; n++) {
      cy += 8;
      M_DrawCharacter(cx * scale, cy * scale, 5);
    }

    M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 8);
    width -= 1;
    cx += 8;
  }

  /* draw right side */
  cy = y;
  M_DrawCharacter(cx * scale, cy * scale, 3);

  for (n = 0; n < lines; n++) {
    cy += 8;
    M_DrawCharacter(cx * scale, cy * scale, 6);
  }

  M_DrawCharacter(cx * scale, cy * scale + 8 * scale, 9);
}

static char *m_popup_string;
static int m_popup_endtime;

static void M_Popup(void)
{
  int x, y, width, lines;
  int n;
  char *str;

  if (!m_popup_string) {
    return;
  }

  if (m_popup_endtime && m_popup_endtime < cls.realtime) {
    m_popup_string = NULL;
    return;
  }

  width = lines = n = 0;
  for (str = m_popup_string; *str; str++) {
    if (*str == '\n') {
      lines++;
      n = 0;
    } else {
      n++;
      if (n > width) {
        width = n;
      }
    }
  }
  if (n) {
    lines++;
  }

  if (width) {
    width += 2;

    x = (320 - (width + 2) * 8) / 2;
    y = (240 - (lines + 2) * 8) / 3;

    M_DrawTextBox(x, y, width, lines);
    M_Print(x + 16, y + 8, m_popup_string);
  }
}

/*
 * MAIN MENU
 */

#define MAIN_ITEMS 5

static void M_Main_Draw(void)
{
  int i;
  int w, h;
  int ystart;
  int xoffset;
  int widest = -1;
  int totalheight = 0;
  char litname[80];
  float scale = SCR_GetMenuScale();
  char *names[] = {"m_main_game", "m_main_multiplayer", "m_main_options", "m_main_video", "m_main_quit", 0};

  for (i = 0; names[i] != 0; i++) {
    Draw_GetPicSize(&w, &h, names[i]);

    if (w > widest) {
      widest = w;
    }

    totalheight += (h + 12);
  }

  ystart = (viddef.height / (2 * scale) - 110);
  xoffset = (viddef.width / scale - widest + 70) / 2;

  for (i = 0; names[i] != 0; i++) {
    if (i != m_main_cursor) {
      Draw_PicScaled(xoffset * scale, (ystart + i * 40 + 13) * scale, names[i], scale);
    }
  }

  strcpy(litname, names[m_main_cursor]);
  strcat(litname, "_sel");
  Draw_PicScaled(xoffset * scale, (ystart + m_main_cursor * 40 + 13) * scale, litname, scale);

  M_DrawCursor(xoffset - 25, ystart + m_main_cursor * 40 + 11, (int) (cls.realtime / 100) % NUM_CURSOR_FRAMES);

  Draw_GetPicSize(&w, &h, "m_main_plaque");
  Draw_PicScaled((xoffset - 30 - w) * scale, ystart * scale, "m_main_plaque", scale);

  Draw_PicScaled((xoffset - 30 - w) * scale, (ystart + h + 5) * scale, "m_main_logo", scale);
}

const char *M_Main_Key(int key)
{
  const char *sound = menu_move_sound;
  int menu_key = Key_GetMenuKey(key);

  switch (menu_key) {
  case K_ESCAPE:
    M_PopMenu();
    break;

  case K_DOWNARROW:
    if (++m_main_cursor >= MAIN_ITEMS) {
      m_main_cursor = 0;
    }
    return sound;

  case K_UPARROW:
    if (--m_main_cursor < 0) {
      m_main_cursor = MAIN_ITEMS - 1;
    }
    return sound;

  case K_ENTER:
    m_entersound = true;

    switch (m_main_cursor) {
    case 0:
      M_Menu_Game_f();
      break;

    case 1:
      M_Menu_Multiplayer_f();
      break;

    case 2:
      M_Menu_Options_f();
      break;

    case 3:
      M_Menu_Video_f();
      break;

    case 4:
      M_Menu_Quit_f();
      break;
    }
  }

  return NULL;
}

void M_Menu_Main_f(void)
{
  M_PushMenu(M_Main_Draw, M_Main_Key);
}

/*
 * MULTIPLAYER MENU
 */

static menuframework_s s_multiplayer_menu;
static menuaction_s s_join_network_server_action;
static menuaction_s s_start_network_server_action;
static menuaction_s s_player_setup_action;

static void Multiplayer_MenuDraw(void)
{
  M_Banner("m_banner_multiplayer");

  Menu_AdjustCursor(&s_multiplayer_menu, 1);
  Menu_Draw(&s_multiplayer_menu);
}

static void PlayerSetupFunc()
{
  M_Menu_PlayerConfig_f();
}

static void JoinNetworkServerFunc()
{
  M_Menu_JoinServer_f();
}

static void StartNetworkServerFunc()
{
  M_Menu_StartServer_f();
}

static void Multiplayer_MenuInit(void)
{
  float scale = SCR_GetMenuScale();

  s_multiplayer_menu.x = (int) (viddef.width * 0.50f) - 64 * scale;
  s_multiplayer_menu.nitems = 0;

  s_join_network_server_action.generic.type = MTYPE_ACTION;
  s_join_network_server_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_join_network_server_action.generic.x = 0;
  s_join_network_server_action.generic.y = 0;
  s_join_network_server_action.generic.name = " join network server";
  s_join_network_server_action.generic.callback = JoinNetworkServerFunc;

  s_start_network_server_action.generic.type = MTYPE_ACTION;
  s_start_network_server_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_start_network_server_action.generic.x = 0;
  s_start_network_server_action.generic.y = 10;
  s_start_network_server_action.generic.name = " start network server";
  s_start_network_server_action.generic.callback = StartNetworkServerFunc;

  s_player_setup_action.generic.type = MTYPE_ACTION;
  s_player_setup_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_player_setup_action.generic.x = 0;
  s_player_setup_action.generic.y = 20;
  s_player_setup_action.generic.name = " player setup";
  s_player_setup_action.generic.callback = PlayerSetupFunc;

  Menu_AddItem(&s_multiplayer_menu, (void *) &s_join_network_server_action);
  Menu_AddItem(&s_multiplayer_menu, (void *) &s_start_network_server_action);
  Menu_AddItem(&s_multiplayer_menu, (void *) &s_player_setup_action);

  Menu_SetStatusBar(&s_multiplayer_menu, NULL);

  Menu_Center(&s_multiplayer_menu);
}

static const char *Multiplayer_MenuKey(int key)
{
  return Default_MenuKey(&s_multiplayer_menu, key);
}

static void M_Menu_Multiplayer_f(void)
{
  Multiplayer_MenuInit();
  M_PushMenu(Multiplayer_MenuDraw, Multiplayer_MenuKey);
}

/*
 * KEYS MENU
 */

char *bindnames[][2] = {
    {"+attack", "attack"},           {"weapnext", "next weapon"},
    {"weapprev", "previous weapon"}, {"+forward", "walk forward"},
    {"+back", "backpedal"},          {"+left", "turn left"},
    {"+right", "turn right"},        {"+speed", "run"},
    {"+moveleft", "step left"},      {"+moveright", "step right"},
    {"+strafe", "sidestep"},         {"+lookup", "look up"},
    {"+lookdown", "look down"},      {"centerview", "center view"},
    {"+mlook", "mouse look"},        {"+klook", "keyboard look"},
    {"+moveup", "up / jump"},        {"+movedown", "down / crouch"},
    {"inven", "inventory"},          {"invuse", "use item"},
    {"invdrop", "drop item"},        {"invprev", "prev item"},
    {"invnext", "next item"},
};

#define NUM_BINDNAMES (sizeof bindnames / sizeof bindnames[0])

static int bind_grab;

static menuframework_s s_keys_menu;
static menuaction_s s_keys_actions[NUM_BINDNAMES];

static void M_UnbindCommand(char *command)
{
  int j;
  int l;
  char *b;

  l = strlen(command);

  for (j = 0; j < 256; j++) {
    b = keybindings[j];

    if (!b) {
      continue;
    }

    if (!strncmp(b, command, l)) {
      Key_SetBinding(j, "");
    }
  }
}

static void M_FindKeysForCommand(char *command, int *twokeys)
{
  int count;
  int j;
  int l;
  char *b;

  twokeys[0] = twokeys[1] = -1;
  l = strlen(command);
  count = 0;

  for (j = 0; j < 256; j++) {
    b = keybindings[j];

    if (!b) {
      continue;
    }

    if (!strncmp(b, command, l)) {
      twokeys[count] = j;
      count++;

      if (count == 2) {
        break;
      }
    }
  }
}

static void KeyCursorDrawFunc(menuframework_s *menu)
{
  float scale = SCR_GetMenuScale();

  if (bind_grab) {
    Draw_CharScaled(menu->x, (menu->y + menu->cursor * 9) * scale, '=', scale);
  } else {
    Draw_CharScaled(menu->x, (menu->y + menu->cursor * 9) * scale, 12 + ((int) (Sys_Milliseconds() / 250) & 1), scale);
  }
}

static void DrawKeyBindingFunc(void *self)
{
  int keys[2];
  menuaction_s *a = (menuaction_s *) self;
  float scale = SCR_GetMenuScale();

  M_FindKeysForCommand(bindnames[a->generic.localdata[0]][0], keys);

  if (keys[0] == -1) {
    Menu_DrawString(a->generic.x + a->generic.parent->x + 16 * scale, a->generic.y + a->generic.parent->y, "???");
  } else {
    int x;
    const char *name;

    name = Key_KeynumToString(keys[0]);

    Menu_DrawString(a->generic.x + a->generic.parent->x + 16 * scale, a->generic.y + a->generic.parent->y, name);

    x = strlen(name) * 8;

    if (keys[1] != -1) {
      Menu_DrawString(a->generic.x + a->generic.parent->x + 24 * scale + (x * scale),
                      a->generic.y + a->generic.parent->y, "or");
      Menu_DrawString(a->generic.x + a->generic.parent->x + 48 * scale + (x * scale),
                      a->generic.y + a->generic.parent->y, Key_KeynumToString(keys[1]));
    }
  }
}

static void KeyBindingFunc(void *self)
{
  menuaction_s *a = (menuaction_s *) self;
  int keys[2];

  M_FindKeysForCommand(bindnames[a->generic.localdata[0]][0], keys);

  if (keys[1] != -1) {
    M_UnbindCommand(bindnames[a->generic.localdata[0]][0]);
  }

  bind_grab = true;

  Menu_SetStatusBar(&s_keys_menu, "press a key or button for this action");
}

static void Keys_MenuInit(void)
{
  int i;

  s_keys_menu.x = (int) (viddef.width * 0.50f);
  s_keys_menu.nitems = 0;
  s_keys_menu.cursordraw = KeyCursorDrawFunc;

  for (i = 0; i < NUM_BINDNAMES; i++) {
    s_keys_actions[i].generic.type = MTYPE_ACTION;
    s_keys_actions[i].generic.flags = QMF_GRAYED;
    s_keys_actions[i].generic.x = 0;
    s_keys_actions[i].generic.y = (i * 9);
    s_keys_actions[i].generic.ownerdraw = DrawKeyBindingFunc;
    s_keys_actions[i].generic.localdata[0] = i;
    s_keys_actions[i].generic.name = bindnames[s_keys_actions[i].generic.localdata[0]][1];

    Menu_AddItem(&s_keys_menu, (void *) &s_keys_actions[i]);
  }

  Menu_SetStatusBar(&s_keys_menu, "ENTER to change, BACKSPACE to clear");
  Menu_Center(&s_keys_menu);
}

static void Keys_MenuDraw(void)
{
  Menu_AdjustCursor(&s_keys_menu, 1);
  Menu_Draw(&s_keys_menu);
}

static const char *Keys_MenuKey(int key)
{
  menuaction_s *item = (menuaction_s *) Menu_ItemAtCursor(&s_keys_menu);

  if (bind_grab) {
    if ((key != K_ESCAPE) && (key != '`')) {
      char cmd[1024];

      Com_sprintf(cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString(key),
                  bindnames[item->generic.localdata[0]][0]);
      Cbuf_InsertText(cmd);
    }

    Menu_SetStatusBar(&s_keys_menu, "ENTER to change, BACKSPACE to clear");
    bind_grab = false;
    return menu_out_sound;
  }

  switch (key) {
  case K_KP_ENTER:
  case K_ENTER:
    KeyBindingFunc(item);
    return menu_in_sound;
  case K_BACKSPACE: /* delete bindings */
  case K_DEL:       /* delete bindings */
  case K_KP_DEL:
    M_UnbindCommand(bindnames[item->generic.localdata[0]][0]);
    return menu_out_sound;
  default:
    return Default_MenuKey(&s_keys_menu, key);
  }
}

static void M_Menu_Keys_f(void)
{
  Keys_MenuInit();
  M_PushMenu(Keys_MenuDraw, Keys_MenuKey);
}

/*
 * CONTROLS MENU
 */

static menuframework_s s_options_menu;
static menuaction_s s_options_defaults_action;
static menuaction_s s_options_customize_options_action;
static menuslider_s s_options_sensitivity_slider;
static menulist_s s_options_freelook_box;
static menulist_s s_options_alwaysrun_box;
static menulist_s s_options_invertmouse_box;
static menulist_s s_options_lookstrafe_box;
static menulist_s s_options_crosshair_box;
static menuslider_s s_options_sfxvolume_slider;
static menulist_s s_options_quality_list;
static menulist_s s_options_console_action;

static void CrosshairFunc()
{
  Cvar_SetValue("crosshair", (float) s_options_crosshair_box.curvalue);
}

static void CustomizeControlsFunc()
{
  M_Menu_Keys_f();
}

static void AlwaysRunFunc()
{
  Cvar_SetValue("cl_run", (float) s_options_alwaysrun_box.curvalue);
}

static void FreeLookFunc()
{
  Cvar_SetValue("freelook", (float) s_options_freelook_box.curvalue);
}

static void MouseSpeedFunc()
{
  Cvar_SetValue("sensitivity", s_options_sensitivity_slider.curvalue / 2.0F);
}

static float ClampCvar(float min, float max, float value)
{
  if (value < min) {
    return min;
  }

  if (value > max) {
    return max;
  }

  return value;
}

static void ControlsSetMenuItemValues(void)
{
  s_options_sfxvolume_slider.curvalue = Cvar_VariableValue("s_volume") * 10;
  s_options_quality_list.curvalue = (Cvar_VariableValue("s_loadas8bit") == 0);
  s_options_sensitivity_slider.curvalue = sensitivity->value * 2;
  s_options_alwaysrun_box.curvalue = (cl_run->value != 0);
  s_options_invertmouse_box.curvalue = (m_pitch->value < 0);
  s_options_lookstrafe_box.curvalue = (lookstrafe->value != 0);
  s_options_freelook_box.curvalue = (freelook->value != 0);
  s_options_crosshair_box.curvalue = ClampCvar(0, 3, crosshair->value);
}

static void ControlsResetDefaultsFunc()
{
  Cbuf_AddText("exec default.cfg\n");
  Cbuf_Execute();

  ControlsSetMenuItemValues();
}

static void InvertMouseFunc()
{
  Cvar_SetValue("m_pitch", -m_pitch->value);
}

static void LookstrafeFunc()
{
  Cvar_SetValue("lookstrafe", (float) !lookstrafe->value);
}

static void UpdateVolumeFunc()
{
  Cvar_SetValue("s_volume", s_options_sfxvolume_slider.curvalue / 10);
}

extern void Key_ClearTyping(void);

static void ConsoleFunc()
{
  SCR_EndLoadingPlaque(); /* get rid of loading plaque */

  if (cl.attractloop) {
    Cbuf_AddText("killserver\n");
    return;
  }

  Key_ClearTyping();
  Con_ClearNotify();

  M_ForceMenuOff();
  cls.key_dest = key_console;

  if ((Cvar_VariableValue("maxclients") == 1) && Com_ServerState()) {
    Cvar_Set("paused", "1");
  }
}

static void UpdateSoundQualityFunc()
{
  if (s_options_quality_list.curvalue == 0) {
    Cvar_SetValue("s_khz", 22);
    Cvar_SetValue("s_loadas8bit", false);
  } else {
    Cvar_SetValue("s_khz", 44);
    Cvar_SetValue("s_loadas8bit", false);
  }

  m_popup_string = "Restarting the sound system. This\n"
                   "could take up to a minute, so\n"
                   "please be patient.";
  m_popup_endtime = cls.realtime + 2000;
  M_Popup();

  /* the text box won't show up unless we do a buffer swap */
  R_EndFrame();

  CL_Snd_Restart_f();
}

static void Options_MenuInit(void)
{
  static const char *quality_items[] = {"normal", "high", 0};

  static const char *yesno_names[] = {"no", "yes", 0};

  static const char *crosshair_names[] = {"none", "cross", "dot", "angle", 0};

  float scale = SCR_GetMenuScale();

  /* configure controls menu and menu items */
  s_options_menu.x = viddef.width / 2;
  s_options_menu.y = viddef.height / (2 * scale) - 58;
  s_options_menu.nitems = 0;

  s_options_sfxvolume_slider.generic.type = MTYPE_SLIDER;
  s_options_sfxvolume_slider.generic.x = 0;
  s_options_sfxvolume_slider.generic.y = 0;
  s_options_sfxvolume_slider.generic.name = "effects volume";
  s_options_sfxvolume_slider.generic.callback = UpdateVolumeFunc;
  s_options_sfxvolume_slider.minvalue = 0;
  s_options_sfxvolume_slider.maxvalue = 10;

  s_options_quality_list.generic.type = MTYPE_SPINCONTROL;
  s_options_quality_list.generic.x = 0;
  s_options_quality_list.generic.y = 40;
  s_options_quality_list.generic.name = "sound quality";
  s_options_quality_list.generic.callback = UpdateSoundQualityFunc;
  s_options_quality_list.itemnames = quality_items;

  s_options_sensitivity_slider.generic.type = MTYPE_SLIDER;
  s_options_sensitivity_slider.generic.x = 0;
  s_options_sensitivity_slider.generic.y = 60;
  s_options_sensitivity_slider.generic.name = "mouse speed";
  s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
  s_options_sensitivity_slider.minvalue = 2;
  s_options_sensitivity_slider.maxvalue = 22;

  s_options_alwaysrun_box.generic.type = MTYPE_SPINCONTROL;
  s_options_alwaysrun_box.generic.x = 0;
  s_options_alwaysrun_box.generic.y = 70;
  s_options_alwaysrun_box.generic.name = "always run";
  s_options_alwaysrun_box.generic.callback = AlwaysRunFunc;
  s_options_alwaysrun_box.itemnames = yesno_names;

  s_options_invertmouse_box.generic.type = MTYPE_SPINCONTROL;
  s_options_invertmouse_box.generic.x = 0;
  s_options_invertmouse_box.generic.y = 80;
  s_options_invertmouse_box.generic.name = "invert mouse";
  s_options_invertmouse_box.generic.callback = InvertMouseFunc;
  s_options_invertmouse_box.itemnames = yesno_names;

  s_options_lookstrafe_box.generic.type = MTYPE_SPINCONTROL;
  s_options_lookstrafe_box.generic.x = 0;
  s_options_lookstrafe_box.generic.y = 90;
  s_options_lookstrafe_box.generic.name = "lookstrafe";
  s_options_lookstrafe_box.generic.callback = LookstrafeFunc;
  s_options_lookstrafe_box.itemnames = yesno_names;

  s_options_freelook_box.generic.type = MTYPE_SPINCONTROL;
  s_options_freelook_box.generic.x = 0;
  s_options_freelook_box.generic.y = 100;
  s_options_freelook_box.generic.name = "free look";
  s_options_freelook_box.generic.callback = FreeLookFunc;
  s_options_freelook_box.itemnames = yesno_names;

  s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
  s_options_crosshair_box.generic.x = 0;
  s_options_crosshair_box.generic.y = 110;
  s_options_crosshair_box.generic.name = "crosshair";
  s_options_crosshair_box.generic.callback = CrosshairFunc;
  s_options_crosshair_box.itemnames = crosshair_names;

  s_options_customize_options_action.generic.type = MTYPE_ACTION;
  s_options_customize_options_action.generic.x = 0;
  s_options_customize_options_action.generic.y = 140;
  s_options_customize_options_action.generic.name = "customize controls";
  s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

  s_options_defaults_action.generic.type = MTYPE_ACTION;
  s_options_defaults_action.generic.x = 0;
  s_options_defaults_action.generic.y = 150;
  s_options_defaults_action.generic.name = "reset defaults";
  s_options_defaults_action.generic.callback = ControlsResetDefaultsFunc;

  s_options_console_action.generic.type = MTYPE_ACTION;
  s_options_console_action.generic.x = 0;
  s_options_console_action.generic.y = 160;
  s_options_console_action.generic.name = "go to console";
  s_options_console_action.generic.callback = ConsoleFunc;

  ControlsSetMenuItemValues();

  Menu_AddItem(&s_options_menu, (void *) &s_options_sfxvolume_slider);

  Menu_AddItem(&s_options_menu, (void *) &s_options_quality_list);
  Menu_AddItem(&s_options_menu, (void *) &s_options_sensitivity_slider);
  Menu_AddItem(&s_options_menu, (void *) &s_options_alwaysrun_box);
  Menu_AddItem(&s_options_menu, (void *) &s_options_invertmouse_box);
  Menu_AddItem(&s_options_menu, (void *) &s_options_lookstrafe_box);
  Menu_AddItem(&s_options_menu, (void *) &s_options_freelook_box);
  Menu_AddItem(&s_options_menu, (void *) &s_options_crosshair_box);

  Menu_AddItem(&s_options_menu, (void *) &s_options_customize_options_action);
  Menu_AddItem(&s_options_menu, (void *) &s_options_defaults_action);
  Menu_AddItem(&s_options_menu, (void *) &s_options_console_action);
}

static void Options_MenuDraw(void)
{
  M_Banner("m_banner_options");
  Menu_AdjustCursor(&s_options_menu, 1);
  Menu_Draw(&s_options_menu);
  M_Popup();
}

static const char *Options_MenuKey(int key)
{
  if (m_popup_string) {
    m_popup_string = NULL;
    return NULL;
  }
  return Default_MenuKey(&s_options_menu, key);
}

static void M_Menu_Options_f(void)
{
  Options_MenuInit();
  M_PushMenu(Options_MenuDraw, Options_MenuKey);
}

/*
 * VIDEO MENU
 */

static void M_Menu_Video_f(void)
{
  VID_MenuInit();
  M_PushMenu(VID_MenuDraw, VID_MenuKey);
}

/*
 * GAME MENU
 */
static menuframework_s s_game_menu;
static menuaction_s s_easy_game_action;
static menuaction_s s_medium_game_action;
static menuaction_s s_hard_game_action;
static menuaction_s s_hardp_game_action;
static menuseparator_s s_blankline;

static void StartGame(void)
{
  cl.servercount = -1;
  M_ForceMenuOff();
  Cvar_SetValue("deathmatch", 0);
  Cvar_SetValue("coop", 0);

  Cbuf_AddText("loading ; killserver ; wait ; newgame\n");
  cls.key_dest = key_game;
}

static void EasyGameFunc()
{
  Cvar_ForceSet("skill", "0");
  StartGame();
}

static void MediumGameFunc()
{
  Cvar_ForceSet("skill", "1");
  StartGame();
}

static void HardGameFunc()
{
  Cvar_ForceSet("skill", "2");
  StartGame();
}

static void HardpGameFunc()
{
  Cvar_ForceSet("skill", "3");
  StartGame();
}

void Game_MenuInit(void)
{
  s_game_menu.x = (int) (viddef.width * 0.50f);
  s_game_menu.nitems = 0;

  s_easy_game_action.generic.type = MTYPE_ACTION;
  s_easy_game_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_easy_game_action.generic.x = 0;
  s_easy_game_action.generic.y = 0;
  s_easy_game_action.generic.name = "easy";
  s_easy_game_action.generic.callback = EasyGameFunc;

  s_medium_game_action.generic.type = MTYPE_ACTION;
  s_medium_game_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_medium_game_action.generic.x = 0;
  s_medium_game_action.generic.y = 10;
  s_medium_game_action.generic.name = "medium";
  s_medium_game_action.generic.callback = MediumGameFunc;

  s_hard_game_action.generic.type = MTYPE_ACTION;
  s_hard_game_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_hard_game_action.generic.x = 0;
  s_hard_game_action.generic.y = 20;
  s_hard_game_action.generic.name = "hard";
  s_hard_game_action.generic.callback = HardGameFunc;

  s_hardp_game_action.generic.type = MTYPE_ACTION;
  s_hardp_game_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_hardp_game_action.generic.x = 0;
  s_hardp_game_action.generic.y = 30;
  s_hardp_game_action.generic.name = "nightmare";
  s_hardp_game_action.generic.callback = HardpGameFunc;

  s_blankline.generic.type = MTYPE_SEPARATOR;

  Menu_AddItem(&s_game_menu, (void *) &s_easy_game_action);
  Menu_AddItem(&s_game_menu, (void *) &s_medium_game_action);
  Menu_AddItem(&s_game_menu, (void *) &s_hard_game_action);
  Menu_AddItem(&s_game_menu, (void *) &s_hardp_game_action);
  Menu_AddItem(&s_game_menu, (void *) &s_blankline);

  Menu_Center(&s_game_menu);
}

static void Game_MenuDraw(void)
{
  M_Banner("m_banner_game");
  Menu_AdjustCursor(&s_game_menu, 1);
  Menu_Draw(&s_game_menu);
}

static const char *Game_MenuKey(int key)
{
  return Default_MenuKey(&s_game_menu, key);
}

static void M_Menu_Game_f(void)
{
  Game_MenuInit();
  M_PushMenu(Game_MenuDraw, Game_MenuKey);
}

/*
 * JOIN SERVER MENU
 */

#define MAX_LOCAL_SERVERS 8

static menuframework_s s_joinserver_menu;
static menuseparator_s s_joinserver_server_title;
static menuaction_s s_joinserver_search_action;
static menuaction_s s_joinserver_address_book_action;
static menuaction_s s_joinserver_server_actions[MAX_LOCAL_SERVERS];

int m_num_servers;
#define NO_SERVER_STRING "<no server>"

/* network address */
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

/* user readable information */
static char local_server_names[MAX_LOCAL_SERVERS][80];
static char local_server_netadr_strings[MAX_LOCAL_SERVERS][80];

void M_AddToServerList(netadr_t adr, char *info)
{
  char *s;
  int i;

  if (m_num_servers == MAX_LOCAL_SERVERS) {
    return;
  }

  while (*info == ' ') {
    info++;
  }

  s = NET_AdrToString(adr);

  /* ignore if duplicated */
  for (i = 0; i < m_num_servers; i++) {
    if (!strcmp(local_server_names[i], info) && !strcmp(local_server_netadr_strings[i], s)) {
      return;
    }
  }

  local_server_netadr[m_num_servers] = adr;
  Q_strlcpy(local_server_names[m_num_servers], info, sizeof(local_server_names[m_num_servers]));
  Q_strlcpy(local_server_netadr_strings[m_num_servers], s, sizeof(local_server_netadr_strings[m_num_servers]));
  m_num_servers++;
}

static void JoinServerFunc(void *self)
{
  char buffer[128];
  int index;

  index = (int) ((menuaction_s *) self - s_joinserver_server_actions);

  if (Q_stricmp(local_server_names[index], NO_SERVER_STRING) == 0) {
    return;
  }

  if (index >= m_num_servers) {
    return;
  }

  Com_sprintf(buffer, sizeof(buffer), "connect %s\n", NET_AdrToString(local_server_netadr[index]));
  Cbuf_AddText(buffer);
  M_ForceMenuOff();
}

static void AddressBookFunc()
{
  M_Menu_AddressBook_f();
}

static void SearchLocalGames(void)
{
  int i;

  m_num_servers = 0;

  for (i = 0; i < MAX_LOCAL_SERVERS; i++) {
    strcpy(local_server_names[i], NO_SERVER_STRING);
    local_server_netadr_strings[i][0] = '\0';
  }

  m_popup_string = "Searching for local servers. This\n"
                   "could take up to a minute, so\n"
                   "please be patient.";
  m_popup_endtime = cls.realtime + 2000;
  M_Popup();

  /* the text box won't show up unless we do a buffer swap */
  R_EndFrame();

  /* send out info packets */
  CL_PingServers_f();
}

static void SearchLocalGamesFunc()
{
  SearchLocalGames();
}

static void JoinServer_MenuInit(void)
{
  int i;
  float scale = SCR_GetMenuScale();

  s_joinserver_menu.x = (int) (viddef.width * 0.50f) - 120 * scale;
  s_joinserver_menu.nitems = 0;

  s_joinserver_address_book_action.generic.type = MTYPE_ACTION;
  s_joinserver_address_book_action.generic.name = "address book";
  s_joinserver_address_book_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_joinserver_address_book_action.generic.x = 0;
  s_joinserver_address_book_action.generic.y = 0;
  s_joinserver_address_book_action.generic.callback = AddressBookFunc;

  s_joinserver_search_action.generic.type = MTYPE_ACTION;
  s_joinserver_search_action.generic.name = "refresh server list";
  s_joinserver_search_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_joinserver_search_action.generic.x = 0;
  s_joinserver_search_action.generic.y = 10;
  s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
  s_joinserver_search_action.generic.statusbar = "search for servers";

  s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
  s_joinserver_server_title.generic.name = "connect to...";
  s_joinserver_server_title.generic.y = 30;
  s_joinserver_server_title.generic.x = 80 * scale;

  for (i = 0; i < MAX_LOCAL_SERVERS; i++) {
    s_joinserver_server_actions[i].generic.type = MTYPE_ACTION;
    s_joinserver_server_actions[i].generic.name = local_server_names[i];
    s_joinserver_server_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
    s_joinserver_server_actions[i].generic.x = 0;
    s_joinserver_server_actions[i].generic.y = 40 + i * 10;
    s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
    s_joinserver_server_actions[i].generic.statusbar = local_server_netadr_strings[i];
  }

  Menu_AddItem(&s_joinserver_menu, &s_joinserver_address_book_action);
  Menu_AddItem(&s_joinserver_menu, &s_joinserver_server_title);
  Menu_AddItem(&s_joinserver_menu, &s_joinserver_search_action);

  for (i = 0; i < 8; i++) {
    Menu_AddItem(&s_joinserver_menu, &s_joinserver_server_actions[i]);
  }

  Menu_Center(&s_joinserver_menu);

  SearchLocalGames();
}

static void JoinServer_MenuDraw(void)
{
  M_Banner("m_banner_join_server");
  Menu_Draw(&s_joinserver_menu);
  M_Popup();
}

static const char *JoinServer_MenuKey(int key)
{
  if (m_popup_string) {
    m_popup_string = NULL;
    return NULL;
  }
  return Default_MenuKey(&s_joinserver_menu, key);
}

static void M_Menu_JoinServer_f(void)
{
  JoinServer_MenuInit();
  M_PushMenu(JoinServer_MenuDraw, JoinServer_MenuKey);
}

/*
 * START SERVER MENU
 */

static menuframework_s s_startserver_menu;
static char **mapnames = NULL;
static int nummaps = 0;

static menuaction_s s_startserver_start_action;
static menuaction_s s_startserver_dmoptions_action;
static menufield_s s_timelimit_field;
static menufield_s s_fraglimit_field;
static menufield_s s_maxclients_field;
static menufield_s s_hostname_field;
static menulist_s s_startmap_list;
static menulist_s s_rules_box;

static void DMOptionsFunc()
{
  M_Menu_DMOptions_f();
}

static void RulesChangeFunc()
{
  /* Deathmatch */
  if (s_rules_box.curvalue == 0) {
    s_maxclients_field.generic.statusbar = NULL;
    s_startserver_dmoptions_action.generic.statusbar = NULL;
  }
}

static void StartServerActionFunc()
{
  char startmap[1024];
  float timelimit;
  float fraglimit;
  float maxclients;

  strcpy(startmap, strchr(mapnames[s_startmap_list.curvalue], '\n') + 1);

  maxclients = (float) strtod(s_maxclients_field.buffer, (char **) NULL);
  timelimit = (float) strtod(s_timelimit_field.buffer, (char **) NULL);
  fraglimit = (float) strtod(s_fraglimit_field.buffer, (char **) NULL);

  Cvar_SetValue("maxclients", ClampCvar(0, maxclients, maxclients));
  Cvar_SetValue("timelimit", ClampCvar(0, timelimit, timelimit));
  Cvar_SetValue("fraglimit", ClampCvar(0, fraglimit, fraglimit));
  Cvar_Set("hostname", s_hostname_field.buffer);

  if ((s_rules_box.curvalue < 2)) {
    Cvar_SetValue("deathmatch", (float) !s_rules_box.curvalue);
    Cvar_SetValue("coop", (float) s_rules_box.curvalue);
  } else {
    Cvar_SetValue("deathmatch", 1); /* deathmatch is always true for rogue games */
    Cvar_SetValue("coop", 0);       /* This works for at least the main game and both addons */
  }

  Cbuf_AddText(va("map %s\n", startmap));

  M_ForceMenuOff();
}

static void StartServer_MenuInit(void)
{
  static const char *dm_coop_names[] = {"deathmatch", "cooperative", 0};

  char *buffer;
  char *s;
  int length;
  int i;
  float scale = SCR_GetMenuScale();

  /* initialize list of maps once, reuse it afterwards (=> it isn't freed) */
  if (mapnames == NULL) {
    /* load the list of map names */
    if ((length = FS_LoadFile("maps.lst", (void **) &buffer)) == -1) {
      Com_Error(ERR_DROP, "couldn't find maps.lst\n");
    }

    s = buffer;
    i = 0;

    while (i < length) {
      if (s[i] == '\r') {
        nummaps++;
      }

      i++;
    }

    if (nummaps == 0) {
      Com_Error(ERR_DROP, "no maps in maps.lst\n");
    }

    mapnames = malloc(sizeof(char *) * (nummaps + 1));
    memset(mapnames, 0, sizeof(char *) * (nummaps + 1));

    s = buffer;

    for (i = 0; i < nummaps; i++) {
      char shortname[MAX_TOKEN_CHARS];
      char longname[MAX_TOKEN_CHARS];
      char scratch[200];
      int j, l;

      strcpy(shortname, COM_Parse(&s));
      l = strlen(shortname);

      for (j = 0; j < l; j++) {
        shortname[j] = toupper(shortname[j]);
      }

      strcpy(longname, COM_Parse(&s));
      Com_sprintf(scratch, sizeof(scratch), "%s\n%s", longname, shortname);

      mapnames[i] = malloc(strlen(scratch) + 1);
      strcpy(mapnames[i], scratch);
    }

    mapnames[nummaps] = 0;
    FS_FreeFile(buffer);
  }

  /* initialize the menu stuff */
  s_startserver_menu.x = (int) (viddef.width * 0.50f);
  s_startserver_menu.nitems = 0;

  s_startmap_list.generic.type = MTYPE_SPINCONTROL;
  s_startmap_list.generic.x = 0;
  s_startmap_list.generic.y = 0;
  s_startmap_list.generic.name = "initial map";
  s_startmap_list.itemnames = (const char **) mapnames;

  s_rules_box.generic.type = MTYPE_SPINCONTROL;
  s_rules_box.generic.x = 0;
  s_rules_box.generic.y = 20;
  s_rules_box.generic.name = "rules";
  s_rules_box.itemnames = dm_coop_names;

  if (Cvar_VariableValue("coop")) {
    s_rules_box.curvalue = 1;
  } else {
    s_rules_box.curvalue = 0;
  }

  s_rules_box.generic.callback = RulesChangeFunc;

  s_timelimit_field.generic.type = MTYPE_FIELD;
  s_timelimit_field.generic.name = "time limit";
  s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
  s_timelimit_field.generic.x = 0;
  s_timelimit_field.generic.y = 36;
  s_timelimit_field.generic.statusbar = "0 = no limit";
  s_timelimit_field.length = 3;
  s_timelimit_field.visible_length = 3;
  strcpy(s_timelimit_field.buffer, Cvar_VariableString("timelimit"));

  s_fraglimit_field.generic.type = MTYPE_FIELD;
  s_fraglimit_field.generic.name = "frag limit";
  s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
  s_fraglimit_field.generic.x = 0;
  s_fraglimit_field.generic.y = 54;
  s_fraglimit_field.generic.statusbar = "0 = no limit";
  s_fraglimit_field.length = 3;
  s_fraglimit_field.visible_length = 3;
  strcpy(s_fraglimit_field.buffer, Cvar_VariableString("fraglimit"));

  /* maxclients determines the maximum number of players that can join
     the game. If maxclients is only "1" then we should default the menu
     option to 8 players, otherwise use whatever its current value is.
     Clamping will be done when the server is actually started. */
  s_maxclients_field.generic.type = MTYPE_FIELD;
  s_maxclients_field.generic.name = "max players";
  s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
  s_maxclients_field.generic.x = 0;
  s_maxclients_field.generic.y = 72;
  s_maxclients_field.generic.statusbar = NULL;
  s_maxclients_field.length = 3;
  s_maxclients_field.visible_length = 3;

  if (Cvar_VariableValue("maxclients") == 1) {
    strcpy(s_maxclients_field.buffer, "8");
  } else {
    strcpy(s_maxclients_field.buffer, Cvar_VariableString("maxclients"));
  }

  s_hostname_field.generic.type = MTYPE_FIELD;
  s_hostname_field.generic.name = "hostname";
  s_hostname_field.generic.flags = 0;
  s_hostname_field.generic.x = 0;
  s_hostname_field.generic.y = 90;
  s_hostname_field.generic.statusbar = NULL;
  s_hostname_field.length = 12;
  s_hostname_field.visible_length = 12;
  strcpy(s_hostname_field.buffer, Cvar_VariableString("hostname"));

  s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
  s_startserver_dmoptions_action.generic.name = " deathmatch flags";
  s_startserver_dmoptions_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_startserver_dmoptions_action.generic.x = 24 * scale;
  s_startserver_dmoptions_action.generic.y = 108;
  s_startserver_dmoptions_action.generic.statusbar = NULL;
  s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

  s_startserver_start_action.generic.type = MTYPE_ACTION;
  s_startserver_start_action.generic.name = " begin";
  s_startserver_start_action.generic.flags = QMF_LEFT_JUSTIFY;
  s_startserver_start_action.generic.x = 24 * scale;
  s_startserver_start_action.generic.y = 128;
  s_startserver_start_action.generic.callback = StartServerActionFunc;

  Menu_AddItem(&s_startserver_menu, &s_startmap_list);
  Menu_AddItem(&s_startserver_menu, &s_rules_box);
  Menu_AddItem(&s_startserver_menu, &s_timelimit_field);
  Menu_AddItem(&s_startserver_menu, &s_fraglimit_field);
  Menu_AddItem(&s_startserver_menu, &s_maxclients_field);
  Menu_AddItem(&s_startserver_menu, &s_hostname_field);
  Menu_AddItem(&s_startserver_menu, &s_startserver_dmoptions_action);
  Menu_AddItem(&s_startserver_menu, &s_startserver_start_action);

  Menu_Center(&s_startserver_menu);

  /* call this now to set proper inital state */
  RulesChangeFunc();
}

static void StartServer_MenuDraw(void)
{
  Menu_Draw(&s_startserver_menu);
}

static const char *StartServer_MenuKey(int key)
{
  return Default_MenuKey(&s_startserver_menu, key);
}

static void M_Menu_StartServer_f(void)
{
  StartServer_MenuInit();
  M_PushMenu(StartServer_MenuDraw, StartServer_MenuKey);
}

/*
 * DMOPTIONS BOOK MENU
 */

static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;

static menulist_s s_friendlyfire_box;
static menulist_s s_falls_box;
static menulist_s s_weapons_stay_box;
static menulist_s s_instant_powerups_box;
static menulist_s s_powerups_box;
static menulist_s s_health_box;
static menulist_s s_spawn_farthest_box;
static menulist_s s_teamplay_box;
static menulist_s s_samelevel_box;
static menulist_s s_force_respawn_box;
static menulist_s s_armor_box;
static menulist_s s_allow_exit_box;
static menulist_s s_infinite_ammo_box;
static menulist_s s_fixed_fov_box;

static void DMFlagCallback(void *self)
{
  menulist_s *f = (menulist_s *) self;
  int flags;
  int bit = 0;

  flags = Cvar_VariableValue("dmflags");

  if (f == &s_friendlyfire_box) {
    if (f->curvalue) {
      flags &= ~DF_NO_FRIENDLY_FIRE;
    } else {
      flags |= DF_NO_FRIENDLY_FIRE;
    }

    goto setvalue;
  } else if (f == &s_falls_box) {
    if (f->curvalue) {
      flags &= ~DF_NO_FALLING;
    } else {
      flags |= DF_NO_FALLING;
    }

    goto setvalue;
  } else if (f == &s_weapons_stay_box) {
    bit = DF_WEAPONS_STAY;
  } else if (f == &s_instant_powerups_box) {
    bit = DF_INSTANT_ITEMS;
  } else if (f == &s_allow_exit_box) {
    bit = DF_ALLOW_EXIT;
  } else if (f == &s_powerups_box) {
    if (f->curvalue) {
      flags &= ~DF_NO_ITEMS;
    } else {
      flags |= DF_NO_ITEMS;
    }

    goto setvalue;
  } else if (f == &s_health_box) {
    if (f->curvalue) {
      flags &= ~DF_NO_HEALTH;
    } else {
      flags |= DF_NO_HEALTH;
    }

    goto setvalue;
  } else if (f == &s_spawn_farthest_box) {
    bit = DF_SPAWN_FARTHEST;
  } else if (f == &s_teamplay_box) {
    if (f->curvalue == 1) {
      flags |= DF_SKINTEAMS;
      flags &= ~DF_MODELTEAMS;
    } else if (f->curvalue == 2) {
      flags |= DF_MODELTEAMS;
      flags &= ~DF_SKINTEAMS;
    } else {
      flags &= ~(DF_MODELTEAMS | DF_SKINTEAMS);
    }

    goto setvalue;
  } else if (f == &s_samelevel_box) {
    bit = DF_SAME_LEVEL;
  } else if (f == &s_force_respawn_box) {
    bit = DF_FORCE_RESPAWN;
  } else if (f == &s_armor_box) {
    if (f->curvalue) {
      flags &= ~DF_NO_ARMOR;
    } else {
      flags |= DF_NO_ARMOR;
    }

    goto setvalue;
  } else if (f == &s_infinite_ammo_box) {
    bit = DF_INFINITE_AMMO;
  } else if (f == &s_fixed_fov_box) {
    bit = DF_FIXED_FOV;
  }

  if (f) {
    if (f->curvalue == 0) {
      flags &= ~bit;
    } else {
      flags |= bit;
    }
  }

setvalue:
  Cvar_SetValue("dmflags", (float) flags);

  Com_sprintf(dmoptions_statusbar, sizeof(dmoptions_statusbar), "dmflags = %d", flags);
}

static void DMOptions_MenuInit(void)
{
  static const char *yes_no_names[] = {"no", "yes", 0};
  static const char *teamplay_names[] = {"disabled", "by skin", "by model", 0};
  int dmflags = Cvar_VariableValue("dmflags");
  int y = 0;

  s_dmoptions_menu.x = (int) (viddef.width * 0.50f);
  s_dmoptions_menu.nitems = 0;

  s_falls_box.generic.type = MTYPE_SPINCONTROL;
  s_falls_box.generic.x = 0;
  s_falls_box.generic.y = y;
  s_falls_box.generic.name = "falling damage";
  s_falls_box.generic.callback = DMFlagCallback;
  s_falls_box.itemnames = yes_no_names;
  s_falls_box.curvalue = (dmflags & DF_NO_FALLING) == 0;

  s_weapons_stay_box.generic.type = MTYPE_SPINCONTROL;
  s_weapons_stay_box.generic.x = 0;
  s_weapons_stay_box.generic.y = y += 10;
  s_weapons_stay_box.generic.name = "weapons stay";
  s_weapons_stay_box.generic.callback = DMFlagCallback;
  s_weapons_stay_box.itemnames = yes_no_names;
  s_weapons_stay_box.curvalue = (dmflags & DF_WEAPONS_STAY) != 0;

  s_instant_powerups_box.generic.type = MTYPE_SPINCONTROL;
  s_instant_powerups_box.generic.x = 0;
  s_instant_powerups_box.generic.y = y += 10;
  s_instant_powerups_box.generic.name = "instant powerups";
  s_instant_powerups_box.generic.callback = DMFlagCallback;
  s_instant_powerups_box.itemnames = yes_no_names;
  s_instant_powerups_box.curvalue = (dmflags & DF_INSTANT_ITEMS) != 0;

  s_powerups_box.generic.type = MTYPE_SPINCONTROL;
  s_powerups_box.generic.x = 0;
  s_powerups_box.generic.y = y += 10;
  s_powerups_box.generic.name = "allow powerups";
  s_powerups_box.generic.callback = DMFlagCallback;
  s_powerups_box.itemnames = yes_no_names;
  s_powerups_box.curvalue = (dmflags & DF_NO_ITEMS) == 0;

  s_health_box.generic.type = MTYPE_SPINCONTROL;
  s_health_box.generic.x = 0;
  s_health_box.generic.y = y += 10;
  s_health_box.generic.callback = DMFlagCallback;
  s_health_box.generic.name = "allow health";
  s_health_box.itemnames = yes_no_names;
  s_health_box.curvalue = (dmflags & DF_NO_HEALTH) == 0;

  s_armor_box.generic.type = MTYPE_SPINCONTROL;
  s_armor_box.generic.x = 0;
  s_armor_box.generic.y = y += 10;
  s_armor_box.generic.name = "allow armor";
  s_armor_box.generic.callback = DMFlagCallback;
  s_armor_box.itemnames = yes_no_names;
  s_armor_box.curvalue = (dmflags & DF_NO_ARMOR) == 0;

  s_spawn_farthest_box.generic.type = MTYPE_SPINCONTROL;
  s_spawn_farthest_box.generic.x = 0;
  s_spawn_farthest_box.generic.y = y += 10;
  s_spawn_farthest_box.generic.name = "spawn farthest";
  s_spawn_farthest_box.generic.callback = DMFlagCallback;
  s_spawn_farthest_box.itemnames = yes_no_names;
  s_spawn_farthest_box.curvalue = (dmflags & DF_SPAWN_FARTHEST) != 0;

  s_samelevel_box.generic.type = MTYPE_SPINCONTROL;
  s_samelevel_box.generic.x = 0;
  s_samelevel_box.generic.y = y += 10;
  s_samelevel_box.generic.name = "same map";
  s_samelevel_box.generic.callback = DMFlagCallback;
  s_samelevel_box.itemnames = yes_no_names;
  s_samelevel_box.curvalue = (dmflags & DF_SAME_LEVEL) != 0;

  s_force_respawn_box.generic.type = MTYPE_SPINCONTROL;
  s_force_respawn_box.generic.x = 0;
  s_force_respawn_box.generic.y = y += 10;
  s_force_respawn_box.generic.name = "force respawn";
  s_force_respawn_box.generic.callback = DMFlagCallback;
  s_force_respawn_box.itemnames = yes_no_names;
  s_force_respawn_box.curvalue = (dmflags & DF_FORCE_RESPAWN) != 0;

  s_teamplay_box.generic.type = MTYPE_SPINCONTROL;
  s_teamplay_box.generic.x = 0;
  s_teamplay_box.generic.y = y += 10;
  s_teamplay_box.generic.name = "teamplay";
  s_teamplay_box.generic.callback = DMFlagCallback;
  s_teamplay_box.itemnames = teamplay_names;

  s_allow_exit_box.generic.type = MTYPE_SPINCONTROL;
  s_allow_exit_box.generic.x = 0;
  s_allow_exit_box.generic.y = y += 10;
  s_allow_exit_box.generic.name = "allow exit";
  s_allow_exit_box.generic.callback = DMFlagCallback;
  s_allow_exit_box.itemnames = yes_no_names;
  s_allow_exit_box.curvalue = (dmflags & DF_ALLOW_EXIT) != 0;

  s_infinite_ammo_box.generic.type = MTYPE_SPINCONTROL;
  s_infinite_ammo_box.generic.x = 0;
  s_infinite_ammo_box.generic.y = y += 10;
  s_infinite_ammo_box.generic.name = "infinite ammo";
  s_infinite_ammo_box.generic.callback = DMFlagCallback;
  s_infinite_ammo_box.itemnames = yes_no_names;
  s_infinite_ammo_box.curvalue = (dmflags & DF_INFINITE_AMMO) != 0;

  s_fixed_fov_box.generic.type = MTYPE_SPINCONTROL;
  s_fixed_fov_box.generic.x = 0;
  s_fixed_fov_box.generic.y = y += 10;
  s_fixed_fov_box.generic.name = "fixed FOV";
  s_fixed_fov_box.generic.callback = DMFlagCallback;
  s_fixed_fov_box.itemnames = yes_no_names;
  s_fixed_fov_box.curvalue = (dmflags & DF_FIXED_FOV) != 0;

  s_friendlyfire_box.generic.type = MTYPE_SPINCONTROL;
  s_friendlyfire_box.generic.x = 0;
  s_friendlyfire_box.generic.y = y += 10;
  s_friendlyfire_box.generic.name = "friendly fire";
  s_friendlyfire_box.generic.callback = DMFlagCallback;
  s_friendlyfire_box.itemnames = yes_no_names;
  s_friendlyfire_box.curvalue = (dmflags & DF_NO_FRIENDLY_FIRE) == 0;

  Menu_AddItem(&s_dmoptions_menu, &s_falls_box);
  Menu_AddItem(&s_dmoptions_menu, &s_weapons_stay_box);
  Menu_AddItem(&s_dmoptions_menu, &s_instant_powerups_box);
  Menu_AddItem(&s_dmoptions_menu, &s_powerups_box);
  Menu_AddItem(&s_dmoptions_menu, &s_health_box);
  Menu_AddItem(&s_dmoptions_menu, &s_armor_box);
  Menu_AddItem(&s_dmoptions_menu, &s_spawn_farthest_box);
  Menu_AddItem(&s_dmoptions_menu, &s_samelevel_box);
  Menu_AddItem(&s_dmoptions_menu, &s_force_respawn_box);
  Menu_AddItem(&s_dmoptions_menu, &s_teamplay_box);
  Menu_AddItem(&s_dmoptions_menu, &s_allow_exit_box);
  Menu_AddItem(&s_dmoptions_menu, &s_infinite_ammo_box);
  Menu_AddItem(&s_dmoptions_menu, &s_fixed_fov_box);
  Menu_AddItem(&s_dmoptions_menu, &s_friendlyfire_box);

  Menu_Center(&s_dmoptions_menu);

  /* set the original dmflags statusbar */
  DMFlagCallback(0);
  Menu_SetStatusBar(&s_dmoptions_menu, dmoptions_statusbar);
}

static void DMOptions_MenuDraw(void)
{
  Menu_Draw(&s_dmoptions_menu);
}

const char *DMOptions_MenuKey(int key)
{
  return Default_MenuKey(&s_dmoptions_menu, key);
}

static void M_Menu_DMOptions_f(void)
{
  DMOptions_MenuInit();
  M_PushMenu(DMOptions_MenuDraw, DMOptions_MenuKey);
}

/*
 * ADDRESS BOOK MENU
 */

#define NUM_ADDRESSBOOK_ENTRIES 9

static menuframework_s s_addressbook_menu;
static menufield_s s_addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];

static void AddressBook_MenuInit(void)
{
  int i;
  float scale = SCR_GetMenuScale();

  s_addressbook_menu.x = viddef.width / 2 - (142 * scale);
  s_addressbook_menu.y = viddef.height / (2 * scale) - 58;
  s_addressbook_menu.nitems = 0;

  for (i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++) {
    cvar_t *adr;
    char buffer[20];

    Com_sprintf(buffer, sizeof(buffer), "adr%d", i);

    adr = Cvar_Get(buffer, "", CVAR_ARCHIVE);

    s_addressbook_fields[i].generic.type = MTYPE_FIELD;
    s_addressbook_fields[i].generic.name = 0;
    s_addressbook_fields[i].generic.callback = 0;
    s_addressbook_fields[i].generic.x = 0;
    s_addressbook_fields[i].generic.y = i * 18 + 0;
    s_addressbook_fields[i].generic.localdata[0] = i;
    s_addressbook_fields[i].cursor = 0;
    s_addressbook_fields[i].length = 60;
    s_addressbook_fields[i].visible_length = 30;

    strcpy(s_addressbook_fields[i].buffer, adr->string);

    Menu_AddItem(&s_addressbook_menu, &s_addressbook_fields[i]);
  }
}

const char *AddressBook_MenuKey(int key)
{
  if (key == K_ESCAPE) {
    int index;
    char buffer[20];

    for (index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++) {
      Com_sprintf(buffer, sizeof(buffer), "adr%d", index);
      Cvar_Set(buffer, s_addressbook_fields[index].buffer);
    }
  }

  return Default_MenuKey(&s_addressbook_menu, key);
}

static void AddressBook_MenuDraw(void)
{
  M_Banner("m_banner_addressbook");
  Menu_Draw(&s_addressbook_menu);
}

static void M_Menu_AddressBook_f(void)
{
  AddressBook_MenuInit();
  M_PushMenu(AddressBook_MenuDraw, AddressBook_MenuKey);
}

/*
 * PLAYER CONFIG MENU
 */

static menuframework_s s_player_config_menu;
static menufield_s s_player_name_field;
static menulist_s s_player_model_box;
static menulist_s s_player_skin_box;
static menulist_s s_player_handedness_box;
static menulist_s s_player_rate_box;
static menuseparator_s s_player_skin_title;
static menuseparator_s s_player_model_title;
static menuseparator_s s_player_hand_title;
static menuseparator_s s_player_rate_title;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024

typedef struct
{
  int nskins;
  char **skindisplaynames;
  char displayname[MAX_DISPLAYNAME];
  char directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = {2500, 3200, 5000, 10000, 25000, 0};
static const char *rate_names[] = {
    "28.8 Modem", "33.6 Modem", "Single ISDN", "Dual ISDN/Cable", "T1/LAN", "User defined", 0};

static void HandednessCallback()
{
  Cvar_SetValue("hand", (float) s_player_handedness_box.curvalue);
}

static void RateCallback()
{
  if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1) {
    Cvar_SetValue("rate", (float) rate_tbl[s_player_rate_box.curvalue]);
  }
}

static void ModelCallback()
{
  s_player_skin_box.itemnames = (const char **) s_pmi[s_player_model_box.curvalue].skindisplaynames;
  s_player_skin_box.curvalue = 0;
}

static void FreeFileList(char **list, int n)
{
  int i;

  for (i = 0; i < n; i++) {
    if (list[i]) {
      free(list[i]);
      list[i] = 0;
    }
  }

  free(list);
}

static qboolean IconOfSkinExists(char *skin, char **pcxfiles, int npcxfiles)
{
  int i;
  char scratch[1024];

  strcpy(scratch, skin);
  *strrchr(scratch, '.') = 0;
  strcat(scratch, "_i.pcx");

  for (i = 0; i < npcxfiles; i++) {
    if (strcmp(pcxfiles[i], scratch) == 0) {
      return true;
    }
  }

  return false;
}

extern char **FS_ListFiles(char *, int *, unsigned, unsigned);

static qboolean PlayerConfig_ScanDirectories(void)
{
  char scratch[1024];
  int ndirs = 0, npms = 0;
  char **dirnames = NULL;
  int i;

  s_numplayermodels = 0;

  /* get a list of directories */
  if ((dirnames = FS_ListFiles2("players/*", &ndirs, SFF_SUBDIR, 0)) == NULL) {
    return false;
  }

  /* go through the subdirectories */
  npms = ndirs;

  if (npms > MAX_PLAYERMODELS) {
    npms = MAX_PLAYERMODELS;
  }

  for (i = 0; i < npms; i++) {
    int k, s;
    char *a, *b, *c;
    char **pcxnames;
    char **skinnames;
    fileHandle_t f;
    int npcxfiles;
    int nskins = 0;

    if (dirnames[i] == 0) {
      continue;
    }

    /* verify the existence of tris.md2 */
    strcpy(scratch, dirnames[i]);
    strcat(scratch, "/tris.md2");

    if (FS_FOpenFile(scratch, &f, false) == 0) {
      free(dirnames[i]);
      dirnames[i] = 0;
      continue;
    } else {
      FS_FCloseFile(f);
    }

    /* verify the existence of at least one pcx skin */
    strcpy(scratch, dirnames[i]);
    strcat(scratch, "/*.pcx");

    if ((pcxnames = FS_ListFiles2(scratch, &npcxfiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM)) == NULL) {
      free(dirnames[i]);
      dirnames[i] = 0;
      continue;
    }

    /* count valid skins, which consist of a skin with a matching "_i" icon */
    for (k = 0; k < npcxfiles - 1; k++) {
      if (!strstr(pcxnames[k], "_i.pcx")) {
        if (IconOfSkinExists(pcxnames[k], pcxnames, npcxfiles - 1)) {
          nskins++;
        }
      }
    }

    if (!nskins) {
      continue;
    }

    skinnames = malloc(sizeof(char *) * (nskins + 1));
    memset(skinnames, 0, sizeof(char *) * (nskins + 1));

    /* copy the valid skins */
    for (s = 0, k = 0; k < npcxfiles - 1; k++) {
      char *a, *b, *c;

      if (!strstr(pcxnames[k], "_i.pcx")) {
        if (IconOfSkinExists(pcxnames[k], pcxnames, npcxfiles - 1)) {
          a = strrchr(pcxnames[k], '/');
          b = strrchr(pcxnames[k], '\\');

          if (a > b) {
            c = a;
          } else {
            c = b;
          }

          strcpy(scratch, c + 1);

          if (strrchr(scratch, '.')) {
            *strrchr(scratch, '.') = 0;
          }

          skinnames[s] = strdup(scratch);
          s++;
        }
      }
    }

    /* at this point we have a valid player model */
    s_pmi[s_numplayermodels].nskins = nskins;
    s_pmi[s_numplayermodels].skindisplaynames = skinnames;

    /* make short name for the model */
    a = strrchr(dirnames[i], '/');
    b = strrchr(dirnames[i], '\\');

    if (a > b) {
      c = a;
    } else {
      c = b;
    }

    Q_strlcpy(s_pmi[s_numplayermodels].displayname, c + 1, sizeof(s_pmi[s_numplayermodels].displayname));
    Q_strlcpy(s_pmi[s_numplayermodels].directory, c + 1, sizeof(s_pmi[s_numplayermodels].directory));

    FreeFileList(pcxnames, npcxfiles);

    s_numplayermodels++;
  }

  if (dirnames) {
    FreeFileList(dirnames, ndirs);
  }

  return true;
}

static int pmicmpfnc(const void *_a, const void *_b)
{
  const playermodelinfo_s *a = (const playermodelinfo_s *) _a;
  const playermodelinfo_s *b = (const playermodelinfo_s *) _b;

  /* sort by male, female, then alphabetical */
  if (strcmp(a->directory, "male") == 0) {
    return -1;
  } else if (strcmp(b->directory, "male") == 0) {
    return 1;
  }

  if (strcmp(a->directory, "female") == 0) {
    return -1;
  } else if (strcmp(b->directory, "female") == 0) {
    return 1;
  }

  return strcmp(a->directory, b->directory);
}

static qboolean PlayerConfig_MenuInit(void)
{
  extern cvar_t *name;
  extern cvar_t *skin;
  char currentdirectory[1024];
  char currentskin[1024];
  int i = 0;
  float scale = SCR_GetMenuScale();

  int currentdirectoryindex = 0;
  int currentskinindex = 0;

  cvar_t *hand = Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);

  static const char *handedness[] = {"right", "left", "center", 0};

  PlayerConfig_ScanDirectories();

  if (s_numplayermodels == 0) {
    return false;
  }

  strcpy(currentdirectory, skin->string);

  if (strchr(currentdirectory, '/')) {
    strcpy(currentskin, strchr(currentdirectory, '/') + 1);
    *strchr(currentdirectory, '/') = 0;
  } else if (strchr(currentdirectory, '\\')) {
    strcpy(currentskin, strchr(currentdirectory, '\\') + 1);
    *strchr(currentdirectory, '\\') = 0;
  } else {
    strcpy(currentdirectory, "male");
    strcpy(currentskin, "grunt");
  }

  qsort(s_pmi, s_numplayermodels, sizeof(s_pmi[0]), pmicmpfnc);

  memset(s_pmnames, 0, sizeof(s_pmnames));

  for (i = 0; i < s_numplayermodels; i++) {
    s_pmnames[i] = s_pmi[i].displayname;

    if (Q_stricmp(s_pmi[i].directory, currentdirectory) == 0) {
      int j;

      currentdirectoryindex = i;

      for (j = 0; j < s_pmi[i].nskins; j++) {
        if (Q_stricmp(s_pmi[i].skindisplaynames[j], currentskin) == 0) {
          currentskinindex = j;
          break;
        }
      }
    }
  }

  s_player_config_menu.x = viddef.width / 2 - 95 * scale;
  s_player_config_menu.y = viddef.height / (2 * scale) - 97;
  s_player_config_menu.nitems = 0;

  s_player_name_field.generic.type = MTYPE_FIELD;
  s_player_name_field.generic.name = "name";
  s_player_name_field.generic.callback = 0;
  s_player_name_field.generic.x = 0;
  s_player_name_field.generic.y = 0;
  s_player_name_field.length = 20;
  s_player_name_field.visible_length = 20;
  strcpy(s_player_name_field.buffer, name->string);
  s_player_name_field.cursor = strlen(name->string);

  s_player_model_title.generic.type = MTYPE_SEPARATOR;
  s_player_model_title.generic.name = "model";
  s_player_model_title.generic.x = -8 * scale;
  s_player_model_title.generic.y = 60;

  s_player_model_box.generic.type = MTYPE_SPINCONTROL;
  s_player_model_box.generic.x = -56 * scale;
  s_player_model_box.generic.y = 70;
  s_player_model_box.generic.callback = ModelCallback;
  s_player_model_box.generic.cursor_offset = -48;
  s_player_model_box.curvalue = currentdirectoryindex;
  s_player_model_box.itemnames = (const char **) s_pmnames;

  s_player_skin_title.generic.type = MTYPE_SEPARATOR;
  s_player_skin_title.generic.name = "skin";
  s_player_skin_title.generic.x = -16 * scale;
  s_player_skin_title.generic.y = 84;

  s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
  s_player_skin_box.generic.x = -56 * scale;
  s_player_skin_box.generic.y = 94;
  s_player_skin_box.generic.name = 0;
  s_player_skin_box.generic.callback = 0;
  s_player_skin_box.generic.cursor_offset = -48;
  s_player_skin_box.curvalue = currentskinindex;
  s_player_skin_box.itemnames = (const char **) s_pmi[currentdirectoryindex].skindisplaynames;

  s_player_hand_title.generic.type = MTYPE_SEPARATOR;
  s_player_hand_title.generic.name = "handedness";
  s_player_hand_title.generic.x = 32 * scale;
  s_player_hand_title.generic.y = 108;

  s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
  s_player_handedness_box.generic.x = -56 * scale;
  s_player_handedness_box.generic.y = 118;
  s_player_handedness_box.generic.name = 0;
  s_player_handedness_box.generic.cursor_offset = -48;
  s_player_handedness_box.generic.callback = HandednessCallback;
  s_player_handedness_box.curvalue = ClampCvar(0, 2, hand->value);
  s_player_handedness_box.itemnames = handedness;

  for (i = 0; i < sizeof(rate_tbl) / sizeof(*rate_tbl) - 1; i++) {
    if (Cvar_VariableValue("rate") == rate_tbl[i]) {
      break;
    }
  }

  s_player_rate_title.generic.type = MTYPE_SEPARATOR;
  s_player_rate_title.generic.name = "connect speed";
  s_player_rate_title.generic.x = 56 * scale;
  s_player_rate_title.generic.y = 156;

  s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
  s_player_rate_box.generic.x = -56 * scale;
  s_player_rate_box.generic.y = 166;
  s_player_rate_box.generic.name = 0;
  s_player_rate_box.generic.cursor_offset = -48;
  s_player_rate_box.generic.callback = RateCallback;
  s_player_rate_box.curvalue = i;
  s_player_rate_box.itemnames = rate_names;

  Menu_AddItem(&s_player_config_menu, &s_player_name_field);
  Menu_AddItem(&s_player_config_menu, &s_player_model_title);
  Menu_AddItem(&s_player_config_menu, &s_player_model_box);

  if (s_player_skin_box.itemnames) {
    Menu_AddItem(&s_player_config_menu, &s_player_skin_title);
    Menu_AddItem(&s_player_config_menu, &s_player_skin_box);
  }

  Menu_AddItem(&s_player_config_menu, &s_player_hand_title);
  Menu_AddItem(&s_player_config_menu, &s_player_handedness_box);
  Menu_AddItem(&s_player_config_menu, &s_player_rate_title);
  Menu_AddItem(&s_player_config_menu, &s_player_rate_box);

  return true;
}

extern float CalcFov(float fov_x, float w, float h);

static void PlayerConfig_MenuDraw(void)
{
  refdef_t refdef;
  char scratch[MAX_QPATH];
  float scale = SCR_GetMenuScale();

  memset(&refdef, 0, sizeof(refdef));

  refdef.x = viddef.width / 2;
  refdef.y = viddef.height / 2 - 72 * scale;
  refdef.width = 144 * scale;
  refdef.height = 168 * scale;
  refdef.fov_x = 40;
  refdef.fov_y = CalcFov(refdef.fov_x, (float) refdef.width, (float) refdef.height);
  refdef.time = cls.realtime * 0.001f;

  if (s_pmi[s_player_model_box.curvalue].skindisplaynames) {
    static int yaw;
    entity_t entity;

    memset(&entity, 0, sizeof(entity));

    Com_sprintf(scratch, sizeof(scratch), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory);
    entity.model = R_RegisterModel(scratch);
    Com_sprintf(scratch, sizeof(scratch), "players/%s/%s.pcx", s_pmi[s_player_model_box.curvalue].directory,
                s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);
    entity.skin = R_RegisterSkin(scratch);
    entity.flags = RF_FULLBRIGHT;
    entity.origin[0] = 80;
    entity.origin[1] = 0;
    entity.origin[2] = 0;
    VectorCopy(entity.origin, entity.oldorigin);
    entity.frame = 0;
    entity.oldframe = 0;
    entity.backlerp = 0.0;
    entity.angles[1] = (float) yaw;

    if (++yaw > 360) {
      yaw -= 360;
    }

    refdef.areabits = 0;
    refdef.num_entities = 1;
    refdef.entities = &entity;
    refdef.lightstyles = 0;
    refdef.rdflags = RDF_NOWORLDMODEL;

    Menu_Draw(&s_player_config_menu);

    M_DrawTextBox(((int) (refdef.x) * (320.0F / viddef.width) - 8),
                  (int) ((viddef.height / 2) * (240.0F / viddef.height) - 77), refdef.width / (8 * scale),
                  refdef.height / (8 * scale));
    refdef.height += 4 * scale;

    R_RenderFrame(&refdef);

    Com_sprintf(scratch, sizeof(scratch), "/players/%s/%s_i.pcx", s_pmi[s_player_model_box.curvalue].directory,
                s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);
    Draw_PicScaled(s_player_config_menu.x - 40 * scale, refdef.y, scratch, scale);
  }
}

static const char *PlayerConfig_MenuKey(int key)
{
  int i;

  if (key == K_ESCAPE) {
    char scratch[1024];

    Cvar_Set("name", s_player_name_field.buffer);

    Com_sprintf(scratch, sizeof(scratch), "%s/%s", s_pmi[s_player_model_box.curvalue].directory,
                s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue]);

    Cvar_Set("skin", scratch);

    for (i = 0; i < s_numplayermodels; i++) {
      int j;

      for (j = 0; j < s_pmi[i].nskins; j++) {
        if (s_pmi[i].skindisplaynames[j]) {
          free(s_pmi[i].skindisplaynames[j]);
        }

        s_pmi[i].skindisplaynames[j] = 0;
      }

      free(s_pmi[i].skindisplaynames);
      s_pmi[i].skindisplaynames = 0;
      s_pmi[i].nskins = 0;
    }
  }

  return Default_MenuKey(&s_player_config_menu, key);
}

static void M_Menu_PlayerConfig_f(void)
{
  if (!PlayerConfig_MenuInit()) {
    Menu_SetStatusBar(&s_multiplayer_menu, "no valid player models found");
    return;
  }

  Menu_SetStatusBar(&s_multiplayer_menu, NULL);
  M_PushMenu(PlayerConfig_MenuDraw, PlayerConfig_MenuKey);
}

/*
 * QUIT MENU
 */

static void M_Menu_Quit_f(void)
{
  CL_Quit_f();
}

void M_Init(void)
{
  Cmd_AddCommand("menu_main", M_Menu_Main_f);
  Cmd_AddCommand("menu_game", M_Menu_Game_f);
  Cmd_AddCommand("menu_joinserver", M_Menu_JoinServer_f);
  Cmd_AddCommand("menu_addressbook", M_Menu_AddressBook_f);
  Cmd_AddCommand("menu_startserver", M_Menu_StartServer_f);
  Cmd_AddCommand("menu_dmoptions", M_Menu_DMOptions_f);
  Cmd_AddCommand("menu_playerconfig", M_Menu_PlayerConfig_f);
  Cmd_AddCommand("menu_multiplayer", M_Menu_Multiplayer_f);
  Cmd_AddCommand("menu_video", M_Menu_Video_f);
  Cmd_AddCommand("menu_options", M_Menu_Options_f);
  Cmd_AddCommand("menu_keys", M_Menu_Keys_f);
  Cmd_AddCommand("menu_quit", M_Menu_Quit_f);
}

void M_Draw(void)
{
  if (cls.key_dest != key_menu) {
    return;
  }

  /* repaint everything next frame */
  SCR_DirtyScreen();
  Draw_FadeScreen();

  m_drawfunc();

  /* delay playing the enter sound until after the
     menu has been drawn, to avoid delay while
     caching images */
  if (m_entersound) {
    S_StartLocalSound(menu_in_sound);
    m_entersound = false;
  }
}

void M_Keydown(int key)
{
  const char *s;

  if (m_keyfunc) {
    if ((s = m_keyfunc(key)) != 0) {
      S_StartLocalSound((char *) s);
    }
  }
}