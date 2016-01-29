/*
 *  GTetrinet
 *  Copyright (C) 1999, 2000, 2001, 2002, 2003  Ka-shu Wong (kswong@zip.com.au)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gtet_config.h"
#include "gtetrinet.h"
#include "client.h"
#include "tetrinet.h"
#include "sound.h"
#include "misc.h"
#include "tetris.h"
#include "fields.h"
#include "partyline.h"

char blocksfile[1024];
int bsize;

GString *currenttheme = NULL;

extern GConfClient *gconf_client;

static char *soundkeys[S_NUM] = {
    "Sounds/Drop",
    "Sounds/Solidify",
    "Sounds/LineClear",
    "Sounds/Tetris",
    "Sounds/Rotate",
    "Sounds/SpecialLine",
    "Sounds/YouWin",
    "Sounds/YouLose",
    "Sounds/Message",
    "Sounds/GameStart",
};

guint defaultkeys[K_NUM] = {
    GDK_Right,
    GDK_Left,
    GDK_Up,
    GDK_Control_R,
    GDK_Down,
    GDK_space,
    GDK_d,
    GDK_t,
    GDK_1,
    GDK_2,
    GDK_3,
    GDK_4,
    GDK_5,
    GDK_6,
    GDK_s
};

guint keys[K_NUM];

/* themedir is assumed to have a trailing slash */
void config_loadtheme (const gchar *themedir)
{
    char buf[1024], *p;
    int i;

    GTET_O_STRCPY(buf, "=");
    GTET_O_STRCAT(buf, themedir);
    GTET_O_STRCAT(buf, "theme.cfg=/");

    gnome_config_push_prefix (buf);

    p = gnome_config_get_string ("Theme/Name");
    if (!p)
      goto bad_theme;
    g_free (p);

    p = gnome_config_get_string ("Graphics/Blocks=blocks.png");
    if (!p)
      goto bad_theme;
    
    GTET_O_STRCPY(blocksfile, themedir);
    GTET_O_STRCAT(blocksfile, p);
    g_free (p);
    bsize = gnome_config_get_int ("Graphics/BlockSize=16");

    p = gnome_config_get_string ("Sounds/MidiFile");
    if (!p)
        midifile[0] = 0;
    else
    {
        GTET_O_STRCPY(midifile, themedir);
        GTET_O_STRCAT(midifile, p);
        g_free (p);
    }

    for (i = 0; i < S_NUM; i ++) {
        p = gnome_config_get_string (soundkeys[i]);
        if (p) {
            GTET_O_STRCPY(soundfiles[i], themedir);
            GTET_O_STRCAT(soundfiles[i], p);
            g_free (p);
        }
        else
            soundfiles[i][0] = 0;
    }

    sound_cache ();

    gnome_config_pop_prefix ();

    return;

 bad_theme:
    {
      GtkWidget *mb;
      mb = gtk_message_dialog_new (NULL,
                                   0,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_OK,
                                   _("Warning: theme does not have a name, "
                                     "reverting to default."));
      gtk_dialog_run (GTK_DIALOG (mb));
      gtk_widget_destroy (mb);
      gnome_config_pop_prefix ();
      g_string_assign(currenttheme, DEFAULTTHEME);
      config_loadtheme (currenttheme->str);
    }
}

/* Arrggh... all these params are sizeof() == 1024 ... this needs a real fix */
int config_getthemeinfo (char *themedir, char *name, char *author, char *desc)
{
    char buf[1024];
    char *p = NULL;

    GTET_O_STRCPY (buf, "=");
    GTET_O_STRCAT (buf, themedir);
    GTET_O_STRCAT (buf, "theme.cfg=/");

    gnome_config_push_prefix (buf);

    p = gnome_config_get_string ("Theme/Name");
    if (p == 0) {
        gnome_config_pop_prefix ();
        return -1;
    }
    else {
        if (name) GTET_STRCPY(name, p, 1024);
        g_free (p);
    }
    if (author && (p = gnome_config_get_string ("Theme/Author="))) {
        GTET_STRCPY(author, p, 1024);
        g_free (p);
    }
    if (desc && (p = gnome_config_get_string ("Theme/Description="))) {
        GTET_STRCPY(desc, p, 1024);
        g_free (p);
    }

    gnome_config_pop_prefix ();

    return 0;
}

void config_loadconfig (void)
{
    gchar *p;

    currenttheme = g_string_sized_new(100);
    
    /* get the current theme */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/themes/theme_dir", NULL);
    /* if there is no theme configured, then we fallback to DEFAULTTHEME */
    if (!p || !p[0])
    {
      g_string_assign(currenttheme, DEFAULTTHEME);
      gconf_client_set_string (gconf_client, "/apps/gtetrinet/themes/theme_dir", currenttheme->str, NULL);
    }
    else
      g_string_assign(currenttheme, p);
    g_free (p);
    
    /* add trailing slash if none exists */
    if (currenttheme->str[currenttheme->len - 1] != '/')
      g_string_append_c(currenttheme, '/');

    /* get the midi player */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/sound/midi_player", NULL);
    if (p)
    {
      GTET_O_STRCPY(midicmd, p);
      g_free (p);
    }
    
    /* get the other sound options */
    soundenable = gconf_client_get_bool (gconf_client, "/apps/gtetrinet/sound/enable_sound", NULL);
    midienable  = gconf_client_get_bool (gconf_client, "/apps/gtetrinet/sound/enable_midi",  NULL);

    /* get the player nickname */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/player/nickname", NULL);
    if (p) {
        GTET_O_STRCPY(nick, p);
        g_free(p);
    }

    /* get the server name */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/player/server", NULL);
    if (p) {
        GTET_O_STRCPY(server, p);
        g_free(p);
    }

    /* get the team name */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/player/team", NULL);
    if (p) {
        GTET_O_STRCPY(team, p);
        g_free(p);
    }
	 
    /* get the game mode */
    gamemode = gconf_client_get_bool (gconf_client, "/apps/gtetrinet/player/gamemode", NULL);

    /* get the keys */
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/right", NULL);
    if (p)
    {
      keys[K_RIGHT] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_RIGHT] = defaultkeys[K_RIGHT];
    
    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/left", NULL);
    if (p)
    {
      keys[K_LEFT] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_LEFT] = defaultkeys[K_LEFT];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/rotate_right", NULL);
    if (p)
    {
      keys[K_ROTRIGHT] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_ROTRIGHT] = defaultkeys[K_ROTRIGHT];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/rotate_left", NULL);
    if (p)
    {
      keys[K_ROTLEFT] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_ROTLEFT] = defaultkeys[K_ROTLEFT];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/down", NULL);
    if (p)
    {
      keys[K_DOWN] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_DOWN] = defaultkeys[K_DOWN];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/drop", NULL);
    if (p)
    {
      keys[K_DROP] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_DROP] = defaultkeys[K_DROP];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/discard", NULL);
    if (p)
    {
      keys[K_DISCARD] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_DISCARD] = defaultkeys[K_DISCARD];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/message", NULL);
    if (p)
    {
      keys[K_GAMEMSG] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_GAMEMSG] = defaultkeys[K_GAMEMSG];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/special1", NULL);
    if (p)
    {
      keys[K_SPECIAL1] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL1] = defaultkeys[K_SPECIAL1];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/special2", NULL);
    if (p)
    {
      keys[K_SPECIAL2] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL2] = defaultkeys[K_SPECIAL2];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/special3", NULL);
    if (p)
    {
      keys[K_SPECIAL3] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL3] = defaultkeys[K_SPECIAL3];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/special4", NULL);
    if (p)
    {
      keys[K_SPECIAL4] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL4] = defaultkeys[K_SPECIAL4];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/special5", NULL);
    if (p)
    {
      keys[K_SPECIAL5] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL5] = defaultkeys[K_SPECIAL5];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/special6", NULL);
    if (p)
    {
      keys[K_SPECIAL6] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL6] = defaultkeys[K_SPECIAL6];

    p = gconf_client_get_string (gconf_client, "/apps/gtetrinet/keys/special_self", NULL);
    if (p)
    {
      keys[K_SPECIAL_SELF] = gdk_keyval_to_lower (gdk_keyval_from_name (p));
      g_free (p);
    }
    else
      keys[K_SPECIAL_SELF] = defaultkeys[K_SPECIAL_SELF];

    /* Get the timestamp option. */
    timestampsenable = gconf_client_get_bool (gconf_client, "/apps/gtetrinet/partyline/enable_timestamps", NULL);

    /* Get the channel list option */
    list_enabled = gconf_client_get_bool (gconf_client, "/apps/gtetrinet/partyline/enable_channel_list", NULL);
    
    config_loadtheme (currenttheme->str);
}

void load_theme (const gchar *theme_dir)
{
  /* load the theme */
  g_string_assign(currenttheme, theme_dir);
  config_loadtheme (theme_dir);

  /* update the fields */
  fields_page_destroy_contents ();
  fields_cleanup ();
  fields_init ();
  fields_page_new ();
  fieldslabelupdate();
  if (ingame)
  {
    sound_stopmidi ();
    sound_playmidi (midifile);
    tetrinet_redrawfields ();
  }
}

void
sound_midi_player_changed (GConfClient *client,
                           guint cnxn_id,
                           GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  GTET_O_STRCPY (midicmd, gconf_value_get_string (gconf_entry_get_value (entry)));
  if (ingame)
  {
    sound_stopmidi ();
    sound_playmidi (midifile);
  }
}

void
sound_enable_sound_changed (GConfClient *client,
                            guint cnxn_id,
                            GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  soundenable = gconf_value_get_bool (gconf_entry_get_value (entry));
  if (!soundenable)
    gconf_client_set_bool (gconf_client, "/apps/gtetrinet/sound/enable_midi", FALSE, NULL);
  else
    sound_cache ();
}

void
sound_enable_midi_changed (GConfClient *client,
                           guint cnxn_id,
                           GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  midienable = gconf_value_get_bool (gconf_entry_get_value (entry));
  if (!midienable)
    sound_stopmidi ();
}

void
themes_theme_dir_changed (GConfClient *client,
                          guint cnxn_id,
                          GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  load_theme (gconf_value_get_string (gconf_entry_get_value (entry)));
}

void
keys_down_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  keys[K_DOWN] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_left_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  keys[K_LEFT] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_right_changed (GConfClient *client,
                    guint cnxn_id,
                    GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  keys[K_RIGHT] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_drop_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  keys[K_DROP] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_rotate_left_changed (GConfClient *client,
                          guint cnxn_id,
                          GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  keys[K_ROTLEFT] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_rotate_right_changed (GConfClient *client,
                           guint cnxn_id,
                           GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  keys[K_ROTRIGHT] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_message_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  keys[K_GAMEMSG] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_discard_changed (GConfClient *client,
                   guint cnxn_id,
                   GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  keys[K_DISCARD] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_special1_changed (GConfClient *client,
                       guint cnxn_id,
                       GConfEntry *entry)
{

  client = client;
  cnxn_id = cnxn_id;

  keys[K_SPECIAL1] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_special2_changed (GConfClient *client,
                       guint cnxn_id,
                       GConfEntry *entry)
{

  client = client;
  cnxn_id = cnxn_id;

  keys[K_SPECIAL2] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_special3_changed (GConfClient *client,
                       guint cnxn_id,
                       GConfEntry *entry)
{

  client = client;
  cnxn_id = cnxn_id;

  keys[K_SPECIAL3] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_special4_changed (GConfClient *client,
                       guint cnxn_id,
                       GConfEntry *entry)
{

  client = client;
  cnxn_id = cnxn_id;

  keys[K_SPECIAL4] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_special5_changed (GConfClient *client,
                       guint cnxn_id,
                       GConfEntry *entry)
{

  client = client;
  cnxn_id = cnxn_id;

  keys[K_SPECIAL5] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_special6_changed (GConfClient *client,
                       guint cnxn_id,
                       GConfEntry *entry)
{

  client = client;
  cnxn_id = cnxn_id;

  keys[K_SPECIAL6] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
keys_special_self_changed (GConfClient *client,
                       guint cnxn_id,
                       GConfEntry *entry)
{

  client = client;
  cnxn_id = cnxn_id;

  keys[K_SPECIAL_SELF] = gdk_keyval_to_lower (gdk_keyval_from_name (gconf_value_get_string (gconf_entry_get_value (entry))));
}

void
partyline_enable_timestamps_changed (GConfClient *client,
                                     guint cnxn_id,
                                     GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  timestampsenable = gconf_value_get_bool (gconf_entry_get_value (entry));
  if (!timestampsenable)
    gconf_client_set_bool (gconf_client, "/apps/gtetrinet/partyline/enable_timestamps", FALSE, NULL);
}

void
partyline_enable_channel_list_changed (GConfClient *client,
                                       guint cnxn_id,
                                       GConfEntry *entry)
{

  client = client;	/* Suppress compile warnings */
  cnxn_id = cnxn_id;	/* Suppress compile warnings */

  partyline_show_channel_list (gconf_value_get_bool (gconf_entry_get_value (entry)));
}

