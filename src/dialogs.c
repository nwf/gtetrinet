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
#include <sys/types.h>
#include <dirent.h>

#include "gtetrinet.h"
#include "gtet_config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "misc.h"
#include "sound.h"
#include "string.h"
#include "partyline.h"

extern GConfClient *gconf_client;
extern GtkWidget *app;

/*****************************************************/
/* connecting dialog - a dialog with a cancel button */
/*****************************************************/
static GtkWidget *connectingdialog = 0, *connectdialog;
static GtkWidget *progressbar;
static gint timeouttag = 0;

GtkWidget *team_dialog;

void connectingdialog_button (GtkWidget *dialog, gint button)
{
    dialog = dialog;
    switch (button) {
    case GTK_RESPONSE_CANCEL:
        g_source_remove (timeouttag);
        timeouttag = 0;
        if (connectingdialog == 0) return;
        client_disconnect ();
        gtk_widget_destroy (connectingdialog);
        connectingdialog = 0;
        break;
    }
}

gint connectingdialog_delete (void)
{
    return TRUE; /* dont kill me */
}

gint connectingdialog_timeout (void)
{
    gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progressbar));
    return TRUE;
}

void connectingdialog_new (void)
{
    if (connectingdialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (connectingdialog));
      return;
    }
    connectingdialog = gtk_dialog_new_with_buttons (_("Connect to server"),
                                                    GTK_WINDOW (connectdialog),
                                                    GTK_DIALOG_MODAL,
                                                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                    NULL);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (connectingdialog), TRUE);
    progressbar = gtk_progress_bar_new ();
    gtk_widget_show (progressbar);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(connectingdialog)->vbox),
                        progressbar, TRUE, TRUE, 0);

    timeouttag = g_timeout_add (20, (GtkFunction)connectingdialog_timeout,
                                NULL);
    g_signal_connect (G_OBJECT(connectingdialog), "response",
                        GTK_SIGNAL_FUNC(connectingdialog_button), NULL);
    g_signal_connect (G_OBJECT(connectingdialog), "delete_event",
                        GTK_SIGNAL_FUNC(connectingdialog_delete), NULL);
    gtk_widget_show (connectingdialog);
}

void connectingdialog_destroy (void)
{
    if (timeouttag != 0) gtk_timeout_remove (timeouttag);
    timeouttag = 0;
    if (connectingdialog == 0) return;
    gtk_widget_destroy (connectingdialog);
    connectingdialog = 0;
}

/*******************/
/* the team dialog */
/*******************/
void teamdialog_destroy (void)
{
    gtk_widget_destroy (team_dialog);
    team_dialog = NULL;
}

void teamdialog_button (GtkWidget *button, gint response, gpointer data)
{
    GtkEntry *entry = GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (data)));
    button = button; /* so we get no unused parameter warning */

    switch (response)
    {
      case GTK_RESPONSE_OK :
      {
        gconf_client_set_string (gconf_client, "/apps/gtetrinet/player/team",
                                 gtk_entry_get_text (entry), NULL);
        tetrinet_changeteam (gtk_entry_get_text(entry));
      }; break;
    }

    teamdialog_destroy ();
}

void teamdialog_new (void)
{
    GtkWidget *hbox, *widget, *entry;
    gchar *team_utf8 = team;
  
    if (team_dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (team_dialog));
      return;
    }

    team_dialog = gtk_dialog_new_with_buttons (_("Change team"),
                                               GTK_WINDOW (app),
                                               GTK_DIALOG_NO_SEPARATOR,
                                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                               GTK_STOCK_OK, GTK_RESPONSE_OK,
                                               NULL);
    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (team_dialog), TRUE);
    gtk_dialog_set_default_response (GTK_DIALOG (team_dialog), GTK_RESPONSE_OK);
    gtk_window_set_position (GTK_WINDOW (team_dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_resizable (GTK_WINDOW (team_dialog), FALSE);

    /* entry and label */
    hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
    widget = gtk_label_new (_("Team name:"));
    gtk_box_pack_start (GTK_BOX (hbox), widget ,TRUE, TRUE, 0);
    entry = gnome_entry_new ("Team");
    gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (entry))),
                        team_utf8);
    g_object_set (G_OBJECT (gnome_entry_gtk_entry (GNOME_ENTRY (entry))),
                  "activates_default", TRUE, NULL);
    gtk_box_pack_start (GTK_BOX (hbox), entry  ,TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD_SMALL);
    gtk_box_pack_end (GTK_BOX (GTK_DIALOG (team_dialog)->vbox), hbox ,TRUE, TRUE, 0);

    /* pass the entry in the data pointer */
    g_signal_connect (G_OBJECT(team_dialog), "response",
                        GTK_SIGNAL_FUNC(teamdialog_button), (gpointer)entry);
    g_signal_connect (G_OBJECT(team_dialog), "destroy",
                        GTK_SIGNAL_FUNC(teamdialog_destroy), NULL);
    gtk_widget_show_all (team_dialog);
}

/**********************/
/* the connect dialog */
/**********************/
static int connecting;
static GtkWidget *serveraddressentry, *nicknameentry, *teamnameentry, *spectatorcheck, *passwordentry;
static GtkWidget *passwordlabel, *teamnamelabel;
static GtkWidget *originalradio, *tetrifastradio;
static GSList *gametypegroup;

void connectdialog_button (GtkDialog *dialog, gint button)
{
    gchar *nick; /* intermediate buffer for recoding purposes */
    const gchar *server1;
    GtkWidget *dialog_error;

    switch (button) {
    case GTK_RESPONSE_OK:
        /* connect now */
        server1 = gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (serveraddressentry))));
        if (g_utf8_strlen (server1, -1) <= 0)
        {
          dialog_error = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_OK,
                                                 _("You must specify a server name."));
          gtk_dialog_run (GTK_DIALOG (dialog_error));
          gtk_widget_destroy (dialog_error);
          return;
        }

        spectating = GTK_TOGGLE_BUTTON(spectatorcheck)->active ? TRUE : FALSE;
        if (spectating)
        {
          g_utf8_strncpy (specpassword, gtk_entry_get_text (GTK_ENTRY(passwordentry)),
                          g_utf8_strlen (gtk_entry_get_text (GTK_ENTRY (passwordentry)), -1));
          if (g_utf8_strlen (specpassword, -1) <= 0)
          {
            dialog_error = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_OK,
                                                   _("Please specify a password to connect as spectator."));
            gtk_dialog_run (GTK_DIALOG (dialog_error));
            gtk_widget_destroy (dialog_error);
            return;
          }
        }
        
        GTET_O_STRCPY (team, gtk_entry_get_text(GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(teamnameentry)))));
        
        nick = g_strdup (gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (nicknameentry)))));
        g_strstrip (nick); /* we remove leading and trailing whitespaces */
        if (g_utf8_strlen (nick, -1) > 0)
        {
          client_init (server1, nick);
        }
        else
        {
            dialog_error = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_OK,
                                                   _("Please specify a valid nickname."));
            gtk_dialog_run (GTK_DIALOG (dialog_error));
            gtk_widget_destroy (dialog_error);
            return;
        }
        
        gconf_client_set_string (gconf_client, "/apps/gtetrinet/player/server", server1, NULL);
        gconf_client_set_string (gconf_client, "/apps/gtetrinet/player/nickname", nick, NULL);
        gconf_client_set_string (gconf_client, "/apps/gtetrinet/player/team",
                                 gtk_entry_get_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(teamnameentry)))),
                                 NULL);
        gconf_client_set_bool (gconf_client, "/apps/gtetrinet/player/gamemode", gamemode, NULL);

        g_free (nick);
        break;
    case GTK_RESPONSE_CANCEL:
        gtk_widget_destroy (connectdialog);
        break;
    }
}

void connectdialog_spectoggle (GtkWidget *widget)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
        gtk_widget_set_sensitive (passwordentry, TRUE);
        gtk_widget_set_sensitive (passwordlabel, TRUE);
        gtk_widget_set_sensitive (teamnameentry, FALSE);
        gtk_widget_set_sensitive (teamnamelabel, FALSE);
    }
    else {
        gtk_widget_set_sensitive (passwordentry, FALSE);
        gtk_widget_set_sensitive (passwordlabel, FALSE);
        gtk_widget_set_sensitive (teamnameentry, TRUE);
        gtk_widget_set_sensitive (teamnamelabel, TRUE);
    }
}

void connectdialog_originaltoggle (GtkWidget *widget)
{
    if (GTK_TOGGLE_BUTTON(widget)-> active) {
        gamemode = ORIGINAL;
    }
}

void connectdialog_tetrifasttoggle (GtkWidget *widget)
{
    if (GTK_TOGGLE_BUTTON(widget)-> active) {
        gamemode = TETRIFAST;
    }
}

void connectdialog_connected (void)
{
    if (connectdialog != NULL) gtk_widget_destroy (connectdialog);
}

void connectdialog_destroy (void)
{
    connecting = FALSE;
}

void connectdialog_new (void)
{
    GtkWidget *widget, *table1, *table2, *frame;
    /* check if dialog is already displayed */
    if (connecting) 
    {
      gtk_window_present (GTK_WINDOW (connectdialog));
      return;
    }
    connecting = TRUE;

    /* make dialog that asks for address/nickname */
    connectdialog = gtk_dialog_new_with_buttons (_("Connect to server"),
                                                 GTK_WINDOW (app),
                                                 GTK_DIALOG_NO_SEPARATOR,
                                                 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                 GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                 NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (connectdialog), GTK_RESPONSE_OK);
    gtk_window_set_resizable (GTK_WINDOW (connectdialog), FALSE);
    g_signal_connect (G_OBJECT(connectdialog), "response",
                        GTK_SIGNAL_FUNC(connectdialog_button), NULL);

    /* main table */
    table1 = gtk_table_new (2, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE(table1), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table1), GNOME_PAD_SMALL);

    /* server address */
    table2 = gtk_table_new (2, 1, FALSE);

    serveraddressentry = gnome_entry_new ("Server");
    g_object_set(G_OBJECT(gnome_entry_gtk_entry(GNOME_ENTRY(serveraddressentry))),
                 "activates_default", TRUE, NULL);
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(serveraddressentry))),
                        server);
    gtk_widget_show (serveraddressentry);
    gtk_table_attach (GTK_TABLE(table2), serveraddressentry,
                      0, 1, 0, 1, GTK_FILL | GTK_EXPAND,
                      GTK_FILL | GTK_EXPAND, 0, 0);
    /* game type radio buttons */
    originalradio = gtk_radio_button_new_with_mnemonic (NULL, _("O_riginal"));
    gametypegroup = gtk_radio_button_get_group (GTK_RADIO_BUTTON(originalradio));
    tetrifastradio = gtk_radio_button_new_with_mnemonic (gametypegroup, _("Tetri_Fast"));
    switch (gamemode) {
    case ORIGINAL:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(originalradio), TRUE);
        break;
    case TETRIFAST:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tetrifastradio), TRUE);
        break;
    }
    gtk_widget_show (originalradio);
    gtk_widget_show (tetrifastradio);
    widget = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
    gtk_box_pack_start (GTK_BOX(widget), originalradio, 0, 0, 0);
    gtk_box_pack_start (GTK_BOX(widget), tetrifastradio, 0, 0, 0);
    gtk_widget_show (widget);
    gtk_table_attach (GTK_TABLE(table2), widget,
                      0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_container_set_border_width (GTK_CONTAINER(table2), GNOME_PAD);
    gtk_widget_show (table2);
    frame = gtk_frame_new (_("Server address"));
    gtk_container_add (GTK_CONTAINER(frame), table2);
    gtk_widget_show (frame);
    gtk_table_attach (GTK_TABLE(table1), frame, 0, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    /* spectator checkbox + password */
    table2 = gtk_table_new (1, 1, FALSE);

    spectatorcheck = gtk_check_button_new_with_mnemonic (_("Connect as a _spectator"));
    gtk_widget_show (spectatorcheck);
    gtk_table_attach (GTK_TABLE(table2), spectatorcheck, 0, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
    passwordlabel = gtk_label_new_with_mnemonic (_("_Password:"));
    gtk_widget_show (passwordlabel);
    gtk_table_attach (GTK_TABLE(table2), passwordlabel, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);
    passwordentry = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (passwordlabel), passwordentry);
    gtk_entry_set_visibility (GTK_ENTRY(passwordentry), FALSE);
    g_object_set(G_OBJECT(passwordentry),
                 "activates_default", TRUE, NULL);
    gtk_widget_show (passwordentry);
    gtk_table_attach (GTK_TABLE(table2), passwordentry, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_container_set_border_width (GTK_CONTAINER(table2), GNOME_PAD);
    gtk_widget_show (table2);
    frame = gtk_frame_new (_("Spectate game"));
    gtk_container_add (GTK_CONTAINER(frame), table2);
    gtk_widget_show (frame);
    gtk_table_attach (GTK_TABLE(table1), frame, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    /* nickname and teamname entries */
    table2 = gtk_table_new (1, 1, FALSE);

    widget = gtk_label_new_with_mnemonic (_("_Nick name:"));
    gtk_widget_show (widget);
    gtk_table_attach (GTK_TABLE(table2), widget, 0, 1, 0, 1,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    nicknameentry = gnome_entry_new ("Nickname");
    gtk_label_set_mnemonic_widget (GTK_LABEL (widget), nicknameentry);
    g_object_set(G_OBJECT(gnome_entry_gtk_entry(GNOME_ENTRY(nicknameentry))),
                 "activates_default", TRUE, NULL);
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(nicknameentry))),
                        nick);
    /* g_free (aux);*/
    gtk_widget_show (nicknameentry);
    gtk_table_attach (GTK_TABLE(table2), nicknameentry, 1, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    teamnamelabel = gtk_label_new_with_mnemonic (_("_Team name:"));
    gtk_widget_show (teamnamelabel);
    gtk_table_attach (GTK_TABLE(table2), teamnamelabel, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    teamnameentry = gnome_entry_new ("Teamname");
    gtk_label_set_mnemonic_widget (GTK_LABEL (teamnamelabel), teamnameentry);
    g_object_set(G_OBJECT(gnome_entry_gtk_entry(GNOME_ENTRY(teamnameentry))),
                 "activates_default", TRUE, NULL);
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(teamnameentry))),
                        team);
    /*g_free (aux);*/
    gtk_widget_show (teamnameentry);
    gtk_table_attach (GTK_TABLE(table2), teamnameentry, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table2), GNOME_PAD_SMALL);
    gtk_container_set_border_width (GTK_CONTAINER(table2), GNOME_PAD);
    gtk_widget_show (table2);
    frame = gtk_frame_new (_("Player information"));
    gtk_container_add (GTK_CONTAINER(frame), table2);
    gtk_widget_show (frame);
    gtk_table_attach (GTK_TABLE(table1), frame, 1, 2, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    gtk_widget_show (table1);

    gtk_container_set_border_width (GTK_CONTAINER (table1), GNOME_PAD_SMALL);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(connectdialog)->vbox),
                        table1, TRUE, TRUE, 0);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(spectatorcheck), spectating);
    connectdialog_spectoggle (spectatorcheck);
    g_signal_connect (G_OBJECT(connectdialog), "destroy",
                        GTK_SIGNAL_FUNC(connectdialog_destroy), NULL);
    g_signal_connect (G_OBJECT(spectatorcheck), "toggled",
                        GTK_SIGNAL_FUNC(connectdialog_spectoggle), NULL);
    g_signal_connect (G_OBJECT(originalradio), "toggled",
                        GTK_SIGNAL_FUNC(connectdialog_originaltoggle), NULL);
    g_signal_connect (G_OBJECT(tetrifastradio), "toggled",
                        GTK_SIGNAL_FUNC(connectdialog_tetrifasttoggle), NULL);
    gtk_widget_show (connectdialog);
}

GtkWidget *prefdialog;

/*************************/
/* the change key dialog */
/*************************/

void key_dialog_callback (GtkWidget *widget, GdkEventKey *key)
{
    gtk_dialog_response (GTK_DIALOG (widget), gdk_keyval_to_lower(key->keyval));
}

gint key_dialog (char *msg)
{
    GtkWidget *dialog, *label;
    gint keydialog_key;

    dialog = gtk_dialog_new_with_buttons (_("Change Key"), GTK_WINDOW (prefdialog),
                                          GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CLOSE,
                                          NULL);
    label = gtk_label_new (msg);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                        label, TRUE, TRUE, GNOME_PAD_SMALL);
    g_signal_connect (G_OBJECT (dialog), "key-press-event",
                        GTK_SIGNAL_FUNC (key_dialog_callback), NULL);
    gtk_widget_set_events (dialog, GDK_KEY_PRESS_MASK);
    keydialog_key = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_hide (dialog);
    gtk_widget_destroy (dialog);
    if (keydialog_key != GTK_RESPONSE_CLOSE )
      return keydialog_key;
    else
      return 0;
}

/**************************/
/* the preferences dialog */
/**************************/
GtkWidget *themelist, *keyclist;
GtkWidget *timestampcheck;
GtkWidget *midientry, *miditable, *midicheck, *soundcheck;
GtkWidget *namelabel, *authlabel, *desclabel;

gchar *actions[K_NUM];

struct themelistentry {
    char dir[1024];
    char name[1024];
} themes[64];

int themecount;
int theme_select;

void prefdialog_destroy (void)
{
    gtk_widget_destroy (prefdialog);
    prefdialog = NULL;
}

void prefdialog_drawkeys (void)
{
    gchar *gconf_keys[K_NUM];
    int i;
    GtkTreeIter iter;
    GtkListStore *keys_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (keyclist)));

    actions[K_RIGHT]    = _("Move right");
    actions[K_LEFT]     = _("Move left");
    actions[K_DOWN]     = _("Move down");
    actions[K_ROTRIGHT] = _("Rotate right");
    actions[K_ROTLEFT]  = _("Rotate left");
    actions[K_DROP]     = _("Drop piece");
    actions[K_DISCARD]  = _("Discard special");
    actions[K_GAMEMSG]  = _("Send message");
    actions[K_SPECIAL1] = _("Special to field 1");
    actions[K_SPECIAL2] = _("Special to field 2");
    actions[K_SPECIAL3] = _("Special to field 3");
    actions[K_SPECIAL4] = _("Special to field 4");
    actions[K_SPECIAL5] = _("Special to field 5");
    actions[K_SPECIAL6] = _("Special to field 6");
    actions[K_SPECIAL_SELF] = _("Special to self");
  
    gconf_keys[K_RIGHT]    = g_strdup ("/apps/gtetrinet/keys/right");
    gconf_keys[K_LEFT]     = g_strdup ("/apps/gtetrinet/keys/left");
    gconf_keys[K_DOWN]     = g_strdup ("/apps/gtetrinet/keys/down");
    gconf_keys[K_ROTRIGHT] = g_strdup ("/apps/gtetrinet/keys/rotate_right");
    gconf_keys[K_ROTLEFT]  = g_strdup ("/apps/gtetrinet/keys/rotate_left");
    gconf_keys[K_DROP]     = g_strdup ("/apps/gtetrinet/keys/drop");
    gconf_keys[K_DISCARD]  = g_strdup ("/apps/gtetrinet/keys/discard");
    gconf_keys[K_GAMEMSG]  = g_strdup ("/apps/gtetrinet/keys/message");
    gconf_keys[K_SPECIAL1] = g_strdup ("/apps/gtetrinet/keys/special1");
    gconf_keys[K_SPECIAL2] = g_strdup ("/apps/gtetrinet/keys/special2");
    gconf_keys[K_SPECIAL3] = g_strdup ("/apps/gtetrinet/keys/special3");
    gconf_keys[K_SPECIAL4] = g_strdup ("/apps/gtetrinet/keys/special4");
    gconf_keys[K_SPECIAL5] = g_strdup ("/apps/gtetrinet/keys/special5");
    gconf_keys[K_SPECIAL6] = g_strdup ("/apps/gtetrinet/keys/special6");
    gconf_keys[K_SPECIAL_SELF] = g_strdup ("/apps/gtetrinet/keys/special_self");

    for (i = 0; i < K_NUM; i ++) {
        gtk_list_store_append (keys_store, &iter);
        gtk_list_store_set (keys_store, &iter,
                            0, actions[i],
                            1, gdk_keyval_name (keys[i]),
                            2, i,
                            3, gconf_keys[i], -1);
    }
    
    for (i = 0; i < K_NUM; i++) g_free (gconf_keys[i]);
}

void prefdialog_restorekeys (void)
{
    GtkTreeIter iter;
    GtkListStore *keys_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (keyclist)));
    gboolean valid;
    gchar *gconf_key;
    gint i;

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (keys_store), &iter);
    while (valid)
    {
        gtk_tree_model_get (GTK_TREE_MODEL (keys_store), &iter, 2, &i, 3, &gconf_key, -1);
        gtk_list_store_set (keys_store, &iter, 1, gdk_keyval_name (defaultkeys[i]), -1);
        gconf_client_set_string (gconf_client, gconf_key, gdk_keyval_name (defaultkeys[i]), NULL);
        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (keys_store), &iter);
        g_free (gconf_key);
    }
}

void prefdialog_changekey (void)
{
    gchar buf[256], *key, *gconf_key;
    gint k;
    GtkListStore *keys_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (keyclist)));
    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (keyclist));
    GtkTreeIter selected_row;

    if (!gtk_tree_selection_get_selected (selection, NULL, &selected_row)) return;

    gtk_tree_model_get (GTK_TREE_MODEL (keys_store), &selected_row,
                        0, &key, 3, &gconf_key, -1);
    g_snprintf (buf, sizeof(buf), _("Press new key for \"%s\""), key);
    k = key_dialog (buf);
    if (k) {
        gtk_list_store_set (keys_store, &selected_row, 1, gdk_keyval_name (k), -1);
        gconf_client_set_string (gconf_client, gconf_key, gdk_keyval_name (k), NULL);
    }
    
    g_free (gconf_key);
}

void prefdialog_midion ()
{
    gtk_widget_set_sensitive (miditable, TRUE);
}

void prefdialog_midioff ()
{
    gtk_widget_set_sensitive (miditable, FALSE);
}

void prefdialog_soundon ()
{
    gtk_widget_set_sensitive (midicheck, TRUE);
    if (GTK_TOGGLE_BUTTON(midicheck)->active) {
        prefdialog_midion ();
    }
}

void prefdialog_soundoff ()
{
    gtk_widget_set_sensitive (midicheck, FALSE);
    prefdialog_midioff ();
}

void prefdialog_soundtoggle (GtkWidget *widget)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
        prefdialog_soundon ();
    }
    else {
        prefdialog_soundoff ();
    }
    gconf_client_set_bool (gconf_client, "/apps/gtetrinet/sound/enable_sound",
                           GTK_TOGGLE_BUTTON (widget)->active, NULL);
}

void prefdialog_channeltoggle (GtkWidget *widget)
{
  gconf_client_set_bool (gconf_client, "/apps/gtetrinet/partyline/enable_channel_list",
			 GTK_TOGGLE_BUTTON (widget)->active, NULL);
}

void prefdialog_miditoggle (GtkWidget *widget)
{
    if (GTK_TOGGLE_BUTTON(widget)->active) {
        prefdialog_midion ();
    }
    else {
        prefdialog_midioff ();
    }
    gconf_client_set_bool (gconf_client, "/apps/gtetrinet/sound/enable_midi",
                           GTK_TOGGLE_BUTTON (widget)->active, NULL);
}


void prefdialog_midichanged (void)
{
    gconf_client_set_string (gconf_client, "/apps/gtetrinet/sound/midi_player",
                             gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (midientry)))),
                             NULL);
}

void prefdialog_restoremidi (void)
{
    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(midientry))),
                        DEFAULTMIDICMD);
    gconf_client_set_string (gconf_client, "/apps/gtetrinet/sound/midi_player",
                             gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (midientry)))),
                             NULL);
}

void prefdialog_timestampstoggle (GtkWidget *widget)
{
    gconf_client_set_bool (gconf_client, "/apps/gtetrinet/partyline/enable_timestamps",
                           GTK_TOGGLE_BUTTON (widget)->active, NULL);
}

void prefdialog_themelistselect (int n)
{
    char author[1024], desc[1024];

    /* update theme description */
    config_getthemeinfo (themes[n].dir, NULL, author, desc);
    leftlabel_set (namelabel, themes[n].name);
    leftlabel_set (authlabel, author);
    leftlabel_set (desclabel, desc);
  
    gconf_client_set_string (gconf_client, "/apps/gtetrinet/themes/theme_dir", themes[n].dir, NULL);
}

void prefdialog_themeselect (GtkTreeSelection *treeselection)
{
    GtkListStore *model;
    GtkTreeIter iter;
    gint row;
  
    if (gtk_tree_selection_get_selected (treeselection, NULL, &iter))
    {
      model = GTK_LIST_STORE (gtk_tree_view_get_model (gtk_tree_selection_get_tree_view (treeselection)));
      gtk_tree_model_get (GTK_TREE_MODEL(model), &iter, 1, &row, -1);
      prefdialog_themelistselect (row);
    }
}

static int themelistcomp (const void *a1, const void *b1)
{
    const struct themelistentry *a = a1, *b = b1;
    return strcmp (a->name, b->name);
}

void prefdialog_themelist ()
{
    DIR *d;
    struct dirent *de;
    char str[1024], buf[1024];
    gchar *dir;
    int i;
    char *basedir[2];
    GtkListStore *theme_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (themelist)));
    GtkTreeSelection *theme_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (themelist));
    GtkTreeIter iter, iter_selected;

    dir = g_build_filename (getenv ("HOME"), ".gtetrinet", "themes", NULL);

    basedir[0] = dir; /* load users themes first ... in case we run out */
    basedir[1] = GTETRINET_THEMES;

    themecount = 0;

    for (i = 0; i < 2; i ++) {
        d = opendir (basedir[i]);
        if (d) {
            while ((de = readdir(d))) {
                GTET_O_STRCPY (buf, basedir[i]);
                GTET_O_STRCAT (buf, "/");
                GTET_O_STRCAT (buf, de->d_name);
                GTET_O_STRCAT (buf, "/");

                if (config_getthemeinfo(buf, str, NULL, NULL) == 0) {
                    GTET_O_STRCPY (themes[themecount].dir, buf);
                    GTET_O_STRCPY (themes[themecount].name, str);
                    themecount ++;
                    if (themecount == (sizeof(themes) / sizeof(themes[0])))
                    { /* FIXME: should be dynamic */
                      g_warning("Too many theme files.\n");
                      closedir (d);
                      goto too_many_themes;
                    }
                }
            }
            closedir (d);
        }
    }
    g_free (dir);
 too_many_themes:
    qsort (themes, themecount, sizeof(struct themelistentry), themelistcomp);

    theme_select = 0;
    gtk_list_store_clear (theme_store);
    for (i = 0; i < themecount; i ++) {
        char *text[2];
        text[0] = themes[i].name;
        text[1] = 0;
        gtk_list_store_append (theme_store, &iter);
        gtk_list_store_set (theme_store, &iter, 0, themes[i].name, 1, i, -1);
        if (strcmp (themes[i].dir, currenttheme->str) == 0)
        {
            iter_selected = iter;
            theme_select = i;
        }
    }
    if (theme_select != 0) 
    {
      gtk_tree_selection_select_iter (theme_selection, &iter_selected);
      prefdialog_themelistselect (theme_select);
    }
}

void prefdialog_response (GtkDialog *dialog,
                          gint arg1)
{

  dialog = dialog;	/* Supress compile warning */

  switch (arg1)
  {
    case GTK_RESPONSE_CLOSE: prefdialog_destroy (); break;
    case GTK_RESPONSE_HELP:  /* here we should open yelp */ break;
  }
}

void prefdialog_new (void)
{
    GtkWidget *label, *table, *frame, *button, *button1, *widget, *table1, *divider, *notebook;
    GtkWidget *themelist_scroll, *key_scroll, *url;
    GtkWidget *channel_list_check;
    GtkListStore *theme_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
    GtkListStore *keys_store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    GtkTreeSelection *theme_selection;
  
    if (prefdialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (prefdialog));
      return;
    }

    prefdialog = gtk_dialog_new_with_buttons (_("GTetrinet Preferences"),
                                              GTK_WINDOW (app),
                                              GTK_DIALOG_NO_SEPARATOR | GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                              GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                              NULL);
    notebook = gtk_notebook_new ();
    gtk_window_set_resizable (GTK_WINDOW (prefdialog), FALSE);

    /* themes */
    themelist = gtk_tree_view_new_with_model (GTK_TREE_MODEL (theme_store));
    themelist_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (themelist_scroll),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER(themelist_scroll), themelist);
    theme_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (themelist));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (themelist), FALSE);
    gtk_widget_set_size_request (themelist, 160, 200);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (themelist), -1, "theme", renderer,
                                                 "text", 0, NULL);

    label = leftlabel_new (_("Select a theme from the list.\n"
                             "Install new themes in ~/.gtetrinet/themes/"));

    table1 = gtk_table_new (3, 2, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(table1), GNOME_PAD_SMALL);
    gtk_table_set_row_spacings (GTK_TABLE(table1), 0);
    gtk_table_set_col_spacings (GTK_TABLE(table1), GNOME_PAD_SMALL);
    widget = leftlabel_new (_("Name:"));
    gtk_table_attach (GTK_TABLE(table1), widget, 0, 1, 0, 1,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    widget = leftlabel_new (_("Author:"));
    gtk_table_attach (GTK_TABLE(table1), widget, 0, 1, 1, 2,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    widget = leftlabel_new (_("Description:"));
    gtk_table_attach (GTK_TABLE(table1), widget, 0, 1, 2, 3,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    namelabel = leftlabel_new ("");
    gtk_table_attach (GTK_TABLE(table1), namelabel, 1, 2, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    authlabel = leftlabel_new ("");
    gtk_table_attach (GTK_TABLE(table1), authlabel, 1, 2, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    desclabel = leftlabel_new ("");
    gtk_table_attach (GTK_TABLE(table1), desclabel, 1, 2, 2, 3,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

    frame = gtk_frame_new (_("Selected Theme"));
    gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_set_border_width (GTK_CONTAINER(frame), GNOME_PAD_SMALL);
    gtk_widget_set_size_request (frame, 240, 100);
    gtk_container_add (GTK_CONTAINER(frame), table1);
    
    table = gtk_table_new (3, 2, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(table), GNOME_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), themelist_scroll, 0, 1, 0, 3,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(table), label, 1, 2, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(table), frame, 1, 2, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    url = gnome_href_new ("http://gtetrinet.sourceforge.net/themes.html", _("Download new themes"));
    gtk_table_attach (GTK_TABLE(table), url, 1, 2, 2, 3,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_SHRINK, 0, 0);
    gtk_widget_show_all (table);
    
    label = gtk_label_new (_("Themes"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

    /* partyline */
    timestampcheck = gtk_check_button_new_with_mnemonic (_("Enable _Timestamps"));
    gtk_widget_show(timestampcheck);
    channel_list_check = gtk_check_button_new_with_mnemonic (_("Enable Channel _List"));
    gtk_widget_show (channel_list_check);

    frame = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(frame), timestampcheck, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(frame), channel_list_check, FALSE, FALSE, 0);
    gtk_widget_show (frame);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(timestampcheck),
                                  timestampsenable);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (channel_list_check),
				  list_enabled);

    g_signal_connect (G_OBJECT(timestampcheck), "toggled",
                      G_CALLBACK(prefdialog_timestampstoggle), NULL);
    g_signal_connect (G_OBJECT (channel_list_check), "toggled",
		      G_CALLBACK (prefdialog_channeltoggle), NULL);

    table = gtk_table_new (3, 1, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(table), GNOME_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), frame, 0, 1, 0, 1,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_widget_show (table);

    label = gtk_label_new (_("Partyline"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

    /* keyboard */
    keyclist = GTK_WIDGET (gtk_tree_view_new_with_model (GTK_TREE_MODEL(keys_store)));
    key_scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (key_scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER(key_scroll), keyclist);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (keyclist), -1, _("Action"), renderer,
                                                 "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (keyclist), -1, _("Key"), renderer,
                                                 "text", 1, NULL);

    gtk_widget_set_size_request (key_scroll, 240, 200);
    gtk_widget_show (key_scroll);

    label = gtk_label_new (_("Select an action from the list and press Change "
                             "Key to change the key associated with the action."));
    gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap (GTK_LABEL(label), TRUE);
    gtk_widget_show (label);
    gtk_widget_set_size_request (label, 180, 100);

    button = gtk_button_new_with_mnemonic (_("Change _key..."));
    g_signal_connect (G_OBJECT(button), "clicked",
                      GTK_SIGNAL_FUNC (prefdialog_changekey), NULL);
    gtk_widget_show (button);

    button1 = gtk_button_new_with_mnemonic (_("_Restore defaults"));
    g_signal_connect (G_OBJECT(button1), "clicked",
                      GTK_SIGNAL_FUNC (prefdialog_restorekeys), NULL);
    gtk_widget_show (button1);

    table = gtk_table_new (2, 2, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(table), GNOME_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), key_scroll, 0, 1, 0, 2,
                      GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(table), label, 1, 2, 0, 1,
                      GTK_FILL, 0, 0, 0);
    frame = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
    gtk_box_pack_end (GTK_BOX(frame), button1, FALSE, FALSE, 0);
    gtk_box_pack_end (GTK_BOX(frame), button, FALSE, FALSE, 0);
    gtk_widget_show (frame);
    gtk_table_attach (GTK_TABLE(table), frame, 1, 2, 1, 2,
                      GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_show (table);

    label = gtk_label_new (_("Keyboard"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

    /* sound */
    soundcheck = gtk_check_button_new_with_mnemonic (_("Enable _Sound"));
    gtk_widget_show (soundcheck);

    midicheck = gtk_check_button_new_with_mnemonic (_("Enable _MIDI"));
    gtk_widget_show (midicheck);

    frame = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX(frame), soundcheck, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(frame), midicheck, FALSE, FALSE, 0);
    gtk_widget_show (frame);

    divider = gtk_hseparator_new ();
    gtk_widget_show (divider);

    midientry = gnome_entry_new ("MidiCmd");
    gtk_widget_show (midientry);

    widget = leftlabel_new (_("Enter command to play a midi file:"));
    gtk_widget_show (widget);

    label = gtk_label_new (_("The above command is run when a midi file is "
                             "to be played.  The name of the midi file is "
                             "placed in the environment variable MIDIFILE."));
    gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_label_set_line_wrap (GTK_LABEL(label), TRUE);
    gtk_widget_show (label);

    button = gtk_button_new_with_mnemonic (_("_Restore defaults"));
    g_signal_connect (G_OBJECT(button), "clicked",
                      GTK_SIGNAL_FUNC (prefdialog_restoremidi), NULL);
    gtk_widget_show (button);

    miditable = gtk_table_new (4, 2, FALSE);
    gtk_table_set_row_spacings (GTK_TABLE(miditable), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(miditable), GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(miditable), widget, 0, 2, 0, 1,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(miditable), midientry, 0, 2, 1, 2,
                      GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
    gtk_table_attach (GTK_TABLE(miditable), label, 0, 1, 2, 4,
                      GTK_FILL, GTK_FILL, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(miditable), button, 1, 2, 2, 3,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_widget_show (miditable);

    table = gtk_table_new (3, 1, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER(table), GNOME_PAD);
    gtk_table_set_row_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_set_col_spacings (GTK_TABLE(table), GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), frame, 0, 1, 0, 1,
                      GTK_EXPAND | GTK_FILL, 0, 0, 0);
    gtk_table_attach (GTK_TABLE(table), divider, 0, 1, 1, 2,
                      GTK_EXPAND | GTK_FILL, 0, 0, GNOME_PAD_SMALL);
    gtk_table_attach (GTK_TABLE(table), miditable, 0, 1, 2, 3,
                      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_show (table);

    label = gtk_label_new (_("Sound"));
    gtk_widget_show (label);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), table, label);

    /* init stuff */
    prefdialog_themelist ();

    gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(GNOME_ENTRY(midientry))), midicmd);

    prefdialog_drawkeys ();

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(soundcheck), soundenable);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(midicheck), midienable);

#ifdef HAVE_ESD
    if (midienable) prefdialog_midion ();
    else prefdialog_midioff ();
    if (soundenable) prefdialog_soundon ();
    else prefdialog_soundoff ();
#else
    prefdialog_soundoff ();
    gtk_widget_set_sensitive (soundcheck, FALSE);
#endif
    
//    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (prefdialog)->action_area), 6);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prefdialog)->vbox), notebook, FALSE, FALSE, 0);

    g_signal_connect (G_OBJECT(soundcheck), "toggled",
                      GTK_SIGNAL_FUNC(prefdialog_soundtoggle), NULL);
    g_signal_connect (G_OBJECT(midicheck), "toggled",
                      GTK_SIGNAL_FUNC(prefdialog_miditoggle), NULL);
    g_signal_connect (G_OBJECT(gnome_entry_gtk_entry(GNOME_ENTRY(midientry))),
                      "changed", GTK_SIGNAL_FUNC(prefdialog_midichanged), NULL);
    g_signal_connect (G_OBJECT(theme_selection), "changed",
                      GTK_SIGNAL_FUNC (prefdialog_themeselect), NULL);
    g_signal_connect (G_OBJECT(prefdialog), "destroy",
                      GTK_SIGNAL_FUNC(prefdialog_destroy), NULL);
    g_signal_connect (G_OBJECT(prefdialog), "response",
                      GTK_SIGNAL_FUNC(prefdialog_response), NULL);
    gtk_widget_show_all (prefdialog);
}
