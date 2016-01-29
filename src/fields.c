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
#include <stdio.h>

#include "gtet_config.h"
#include "client.h"
#include "tetrinet.h"
#include "tetris.h"
#include "fields.h"
#include "misc.h"
#include "gtetrinet.h"
#include "string.h"

#define BLOCKSIZE bsize
#define SMALLBLOCKSIZE (BLOCKSIZE/2)

static GtkWidget *fieldwidgets[6], *nextpiecewidget, *fieldlabels[6][6],
    *specialwidget, *speciallabel, *attdefwidget, *lineswidget, *levelwidget,
    *activewidget, *activelabel, *gmsgtext, *gmsginput, *fieldspage, *pagecontents;

static GtkWidget *fields_page_contents (void);

static gint fields_expose_event (GtkWidget *widget, GdkEventExpose *event, int field);
static gint fields_nextpiece_expose (GtkWidget *widget);
static gint fields_specials_expose (GtkWidget *widget);

static void fields_refreshfield (int field);
void fields_drawblock (int field, int x, int y, char block);

static void gmsginput_activate (void);

static GdkPixmap *blockpix;

static GdkColor black = {0, 0, 0, 0};
static GdkBitmap *bitmap;
static GdkCursor *invisible_cursor, *arrow_cursor;

static FIELD displayfields[6]; /* what is actually displayed */
static TETRISBLOCK displayblock;

void fields_init (void)
{
    GtkWidget *mb;
    GdkPixbuf *pb = NULL;
    GError *err = NULL;
    GdkBitmap *mask = NULL;
    
    if (!(pb = gdk_pixbuf_new_from_file(blocksfile, &err))) {
        mb = gtk_message_dialog_new (NULL,
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_OK,
                                     _("Error loading theme: cannot load graphics file\n"
                                       "Falling back to default"));
        gtk_dialog_run (GTK_DIALOG (mb));
        gtk_widget_destroy (mb);
	g_string_assign(currenttheme, DEFAULTTHEME);
        config_loadtheme (DEFAULTTHEME);
        err = NULL;
        if (!(pb = gdk_pixbuf_new_from_file(blocksfile, &err))) {
            /* shouldnt happen */
            fprintf (stderr, _("Error loading default theme: Aborting...\n"
                               "Check for installation errors\n"));
            exit (0);
        }
    }
    /* we dont want the bitmap mask */
    gdk_pixbuf_render_pixmap_and_mask(pb, &blockpix, &mask, 1);
    gdk_pixbuf_unref(pb);
}

void fields_cleanup (void)
{
  g_object_unref(blockpix);
}

/* a mess of functions here for creating the fields page */

GtkWidget *fields_page_new (void)
{
    pagecontents = fields_page_contents ();

    if (fieldspage == NULL) {
        fieldspage = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
        gtk_container_set_border_width (GTK_CONTAINER(fieldspage), 2);
    }
    gtk_container_add (GTK_CONTAINER(fieldspage), pagecontents);

    /* create the cursors */
    bitmap = gdk_bitmap_create_from_data (GTK_WIDGET (fieldspage)->window, "\0", 1, 1);
    invisible_cursor = gdk_cursor_new_from_pixmap (bitmap, bitmap, &black, &black, 0, 0);
    arrow_cursor = gdk_cursor_new (GDK_X_CURSOR);

    return fieldspage;
}

void fields_page_destroy_contents (void)
{
    if (pagecontents) {
        gtk_widget_destroy (pagecontents);
        pagecontents = NULL;
    }
}

GtkWidget *fields_page_contents (void)
{
    GtkWidget *vbox, *table, *widget, *align, *border, *box, *table2, *hbox, *scroll;
    table = gtk_table_new (4, 5, FALSE);
    vbox = gtk_vbox_new (FALSE, 0);

    gtk_table_set_row_spacings (GTK_TABLE(table), 2);
    gtk_table_set_col_spacings (GTK_TABLE(table), 2);

    /* make fields */
    {
        int p[6][4] = { /* table attach positions */
            {0, 1, 0, 2},
            {2, 3, 0, 1},
            {3, 4, 0, 1},
            {4, 5, 0, 1},
            {3, 4, 1, 3},
            {4, 5, 1, 3}
        };
        int i;
        int blocksize;
        float valign;
        for (i = 0; i < 6; i ++) {
            if (i == 0) blocksize = BLOCKSIZE;
            else blocksize = SMALLBLOCKSIZE;
            if (i < 4) valign = 0.0;
            else valign = 1.0;
            /* make the widgets */
            box = gtk_vbox_new (FALSE, 0);
            /* the labels */
            fieldlabels[i][0] = gtk_label_new ("");
            fieldlabels[i][1] = gtk_vseparator_new ();
            fieldlabels[i][2] = gtk_label_new ("");
            fieldlabels[i][3] = gtk_label_new ("");
            fieldlabels[i][4] = gtk_vseparator_new ();
            fieldlabels[i][5] = gtk_label_new ("");

            hbox = gtk_hbox_new (FALSE, 0);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][0], FALSE, FALSE, 2);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][1], FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][2], FALSE, FALSE, 2);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][3], TRUE, TRUE, 0);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][4], FALSE, FALSE, 2);
            gtk_box_pack_start (GTK_BOX(hbox), fieldlabels[i][5], FALSE, FALSE, 0);

            fields_setlabel (i, NULL, NULL, 0);

            widget = gtk_event_box_new ();
            gtk_container_add (GTK_CONTAINER(widget), hbox);
            gtk_widget_set_size_request (widget, blocksize * FIELDWIDTH, -1);
            gtk_box_pack_start (GTK_BOX(box), widget, TRUE, TRUE, 0);
            /* the field */
            fieldwidgets[i] = gtk_drawing_area_new ();
            
            /* attach the signals */
            g_signal_connect (G_OBJECT(fieldwidgets[i]), "expose_event",
                                GTK_SIGNAL_FUNC(fields_expose_event), (gpointer)i);
            gtk_widget_set_events (fieldwidgets[i], GDK_EXPOSURE_MASK);
            /* set the size */
            gtk_widget_set_size_request (fieldwidgets[i],
                                         blocksize * FIELDWIDTH,
                                         blocksize * FIELDHEIGHT);
            gtk_box_pack_start (GTK_BOX(box), fieldwidgets[i], TRUE, TRUE, 0);
            border = gtk_frame_new (NULL);
            gtk_frame_set_shadow_type (GTK_FRAME(border), GTK_SHADOW_IN);
            gtk_container_add (GTK_CONTAINER(border), box);
            /* align it */
            align = gtk_alignment_new (0.5, valign, 0.0, 0.0);
            gtk_container_add (GTK_CONTAINER(align), border);
            gtk_table_attach (GTK_TABLE(table), align,
                              p[i][0], p[i][1], p[i][2], p[i][3],
                              GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                              0, 0);
        }
    }
    /* next block thingy */
    box = gtk_vbox_new (FALSE, 2);

    widget = leftlabel_new (_("Next piece:"));
    gtk_box_pack_start (GTK_BOX(box), widget, TRUE, TRUE, 0);
    /* box that displays the next block */
    border = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(border), GTK_SHADOW_IN);
    nextpiecewidget = gtk_drawing_area_new ();
    g_signal_connect (G_OBJECT(nextpiecewidget), "expose_event",
                        GTK_SIGNAL_FUNC(fields_nextpiece_expose), NULL);
    gtk_widget_set_events (nextpiecewidget, GDK_EXPOSURE_MASK);
    gtk_widget_set_size_request (nextpiecewidget, BLOCKSIZE*9/2, BLOCKSIZE*9/2);
    gtk_container_add (GTK_CONTAINER(border), nextpiecewidget);
    align = gtk_alignment_new (0.5, 0.5, 0, 0);
    gtk_container_add (GTK_CONTAINER(align), border);
    gtk_box_pack_start (GTK_BOX(box), align, TRUE, TRUE, 0);
    /* lines, levels and stuff */
    table2 = gtk_table_new (4, 2, FALSE);
    gtk_table_set_col_spacings (GTK_TABLE(table2), 5);
    widget = leftlabel_new (_("Lines:"));
    gtk_table_attach_defaults (GTK_TABLE(table2), widget, 0, 1, 0, 1);
    widget = gtk_label_new ("");
    gtk_table_attach_defaults (GTK_TABLE(table2), widget, 0, 1, 1, 2);
    widget = leftlabel_new (_("Level:"));
    gtk_table_attach_defaults (GTK_TABLE(table2), widget, 0, 1, 2, 3);
    activelabel = leftlabel_new (_("Active level:"));
    gtk_table_attach_defaults (GTK_TABLE(table2), activelabel, 0, 1, 3, 4);
    lineswidget = leftlabel_new ("");
    gtk_table_attach_defaults (GTK_TABLE(table2), lineswidget, 1, 2, 0, 1);
    widget = gtk_label_new ("");
    gtk_table_attach_defaults (GTK_TABLE(table2), widget, 1, 2, 1, 2);
    levelwidget = leftlabel_new ("");
    gtk_table_attach_defaults (GTK_TABLE(table2), levelwidget, 1, 2, 2, 3);
    activewidget = leftlabel_new ("");
    gtk_table_attach_defaults (GTK_TABLE(table2), activewidget, 1, 2, 3, 4);
    gtk_box_pack_start (GTK_BOX(box), table2, TRUE, TRUE, 0);

    /* align it */
    align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
    gtk_widget_set_size_request (align, BLOCKSIZE*6, BLOCKSIZE*11);
    gtk_container_add (GTK_CONTAINER(align), box);
    gtk_table_attach (GTK_TABLE(table), align, 1, 2, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                      0, 0);

    /* the specials thingy */
    box = gtk_hbox_new (FALSE, 0);
    speciallabel = gtk_label_new ("");
    gtk_widget_show (speciallabel);
    fields_setspeciallabel (NULL);
    align = gtk_alignment_new (1.0, 0.0, 1.0, 1.0);
    gtk_container_add (GTK_CONTAINER (align), speciallabel);
    gtk_box_pack_start (GTK_BOX(box), align, TRUE, TRUE, 0);
    border = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME(border), GTK_SHADOW_IN);
    specialwidget = gtk_drawing_area_new ();
    g_signal_connect (G_OBJECT(specialwidget), "expose_event",
                        GTK_SIGNAL_FUNC(fields_specials_expose), NULL);
    gtk_widget_set_size_request (specialwidget, BLOCKSIZE*18, BLOCKSIZE);
    gtk_widget_show (specialwidget);
    gtk_container_add (GTK_CONTAINER(border), specialwidget);
    gtk_box_pack_end (GTK_BOX(box), border, FALSE, FALSE, 0);
    gtk_widget_set_size_request (box, BLOCKSIZE*24, -1);
    /* align it */
    align = gtk_alignment_new (0.5, 1.0, 0.7, 0.0);
    gtk_container_add (GTK_CONTAINER(align), box);
    gtk_table_attach (GTK_TABLE (table), align, 0, 3, 2, 3,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                      0, 0);

    /* attacks and defenses */

    box = gtk_vbox_new (FALSE, 0);
    widget = gtk_label_new (_("Attacks and defenses:"));
    gtk_box_pack_start (GTK_BOX(box), widget, TRUE, TRUE, 0);
    attdefwidget = gtk_text_view_new_with_buffer(gtk_text_buffer_new(tag_table));
    gtk_widget_set_size_request (attdefwidget, MAX(22*12, BLOCKSIZE*12), BLOCKSIZE*10);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(attdefwidget), GTK_WRAP_WORD);
    GTK_WIDGET_UNSET_FLAGS (attdefwidget, GTK_CAN_FOCUS);
    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER(scroll), attdefwidget);
    gtk_box_pack_start (GTK_BOX(box), scroll, TRUE, TRUE, 0);
    align = gtk_alignment_new (0.5, 0.5, 0.5, 0.0);
    gtk_container_add (GTK_CONTAINER(align), box);
    gtk_table_attach (GTK_TABLE(table), align, 1, 3, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                      0, 0);

    /* game messages */
    table2 = gtk_table_new (1, 2, FALSE);
    gmsgtext = gtk_text_view_new_with_buffer(gtk_text_buffer_new(tag_table));
    gtk_widget_set_size_request (gmsgtext, -1, 48);
    GTK_WIDGET_UNSET_FLAGS (gmsgtext, GTK_CAN_FOCUS);
    scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER(scroll), gmsgtext);
    gtk_table_attach (GTK_TABLE(table2), scroll, 0, 1, 0, 1,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND,
                      0, 0);
    gmsginput = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (gmsginput), 128);
    /* eat up key messages */
    g_signal_connect (G_OBJECT(gmsginput), "activate",
                        GTK_SIGNAL_FUNC(gmsginput_activate), NULL);
    gtk_table_attach (GTK_TABLE(table2), gmsginput, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);
    gtk_widget_set_size_request (table2, -1, 70);
    
    align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
    gtk_container_add (GTK_CONTAINER (align), table);
    gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);
    align = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
    gtk_container_add (GTK_CONTAINER (align), table2);
    gtk_box_pack_start (GTK_BOX (vbox), align, TRUE, TRUE, 0);
/*    gtk_table_attach (GTK_TABLE(table), table2, 0, 5, 3, 4,
                      GTK_FILL | GTK_EXPAND, 0, 0, 0);*/

    gtk_widget_show_all (vbox);

    fields_setlines (-1);
    fields_setlevel (-1);
    fields_setactivelevel (-1);
    fields_gmsginput (FALSE);

    return vbox;
}


gint fields_expose_event (GtkWidget *widget, GdkEventExpose *event, int field)
{
    widget = widget;
    event = event;
    fields_refreshfield (field);
    /* hide the cursor */
    if (ingame)
      gdk_window_set_cursor (widget->window, invisible_cursor);
    else
      gdk_window_set_cursor (widget->window, arrow_cursor);

    return FALSE;
}

void fields_refreshfield (int field)
{
    int x, y;
    for (y = 0; y < FIELDHEIGHT; y ++)
        for (x = 0; x < FIELDWIDTH; x ++)
            fields_drawblock (field, x, y, displayfields[field][y][x]);
}

void fields_drawfield (int field, FIELD newfield)
{
    int x, y;
    for (y = 0; y < FIELDHEIGHT; y ++)
        for (x = 0; x < FIELDWIDTH; x ++)
                fields_drawblock (field, x, y, newfield[y][x]);
                displayfields[field][y][x] = newfield[y][x];
}

void fields_drawblock (int field, int x, int y, char block)
{
    int srcx, srcy, destx, desty, blocksize;

    if (field == 0) {
        blocksize = BLOCKSIZE;
        if (block == 0) {
            srcx = blocksize*x;
            srcy = BLOCKSIZE+SMALLBLOCKSIZE + blocksize*y;
        }
        else {
            srcx = (block-1) * blocksize;
            srcy = 0;
        }
    }
    else {
        blocksize = SMALLBLOCKSIZE;
        if (block == 0) {
            srcx = BLOCKSIZE*FIELDWIDTH + blocksize*x;
            srcy = BLOCKSIZE+SMALLBLOCKSIZE + blocksize*y;
        }
        else {
            srcx = (block-1) * blocksize;
            srcy = BLOCKSIZE;
        }
    }
    destx = blocksize * x;
    desty = blocksize * y;

    gdk_draw_drawable (fieldwidgets[field]->window,
                       fieldwidgets[field]->style->black_gc,
                       blockpix, srcx, srcy, destx, desty,
                       blocksize, blocksize);
}

void fields_setlabel (int field, char *name, char *team, int num)
{
    char buf[11];

    g_snprintf (buf, sizeof(buf), "%d", num);
    
    if (name == NULL) {
        gtk_widget_hide (fieldlabels[field][0]);
        gtk_widget_hide (fieldlabels[field][1]);
        gtk_widget_hide (fieldlabels[field][2]);
        gtk_widget_show (fieldlabels[field][3]);
        gtk_widget_hide (fieldlabels[field][4]);
        gtk_widget_hide (fieldlabels[field][5]);
        gtk_label_set_text (GTK_LABEL(fieldlabels[field][0]), "");
        gtk_label_set_text (GTK_LABEL(fieldlabels[field][2]), "");
        gtk_label_set_text (GTK_LABEL(fieldlabels[field][3]), _("Not playing"));
        gtk_label_set_text (GTK_LABEL(fieldlabels[field][5]), "");
    }
    else {
        gtk_widget_show (fieldlabels[field][0]);
        gtk_widget_show (fieldlabels[field][1]);
        gtk_widget_show (fieldlabels[field][2]);
        gtk_widget_hide (fieldlabels[field][3]);
        gtk_label_set_text (GTK_LABEL(fieldlabels[field][0]), buf);
        gtk_label_set_text (GTK_LABEL(fieldlabels[field][2]), name);
        gtk_label_set_text (GTK_LABEL(fieldlabels[field][3]), "");
        if (team == NULL || team[0] == 0) {
            gtk_widget_hide (fieldlabels[field][4]);
            gtk_widget_hide (fieldlabels[field][5]);
            gtk_label_set_text (GTK_LABEL(fieldlabels[field][5]), "");
        }
        else {
            gtk_widget_show (fieldlabels[field][4]);
            gtk_widget_show (fieldlabels[field][5]);
            gtk_label_set_text (GTK_LABEL(fieldlabels[field][5]), team);
        }
    }
}

void fields_setspeciallabel (char *label)
{
    if (label == NULL) {
        gtk_label_set_text (GTK_LABEL(speciallabel), _("Specials:"));
    }
    else {
        gtk_label_set_text (GTK_LABEL(speciallabel), label);
    }
}

gint fields_nextpiece_expose (GtkWidget *widget)
{
    fields_drawnextblock (NULL);
    if (ingame)
      gdk_window_set_cursor (widget->window, invisible_cursor);
    else
      gdk_window_set_cursor (widget->window, arrow_cursor);
    return FALSE;
}

gint fields_specials_expose (GtkWidget *widget)
{
    fields_drawspecials ();
    if (ingame)
      gdk_window_set_cursor (widget->window, invisible_cursor);
    else
      gdk_window_set_cursor (widget->window, arrow_cursor);
    return FALSE;
}

void fields_drawspecials (void)
{
    int i;
    for (i = 0; i < 18; i ++) {
        if (i < specialblocknum) {
            gdk_draw_drawable (specialwidget->window,
                               specialwidget->style->black_gc,
                               blockpix, (specialblocks[i]-1)*BLOCKSIZE,
                               0, BLOCKSIZE*i, 0, BLOCKSIZE, BLOCKSIZE);
        }
        else {
            gdk_draw_rectangle (specialwidget->window, specialwidget->style->black_gc,
                                TRUE, BLOCKSIZE*i, 0,
                                BLOCKSIZE*(i+1), BLOCKSIZE);
        }
    }
}

void fields_drawnextblock (TETRISBLOCK block)
{
    int x, y, xstart = 4, ystart = 4;
    if (block == NULL) block = displayblock;
    gdk_draw_rectangle (nextpiecewidget->window, nextpiecewidget->style->black_gc,
                        TRUE, 0, 0, BLOCKSIZE*9/2, BLOCKSIZE*9/2);
    for (y = 0; y < 4; y ++)
        for (x = 0; x < 4; x ++)
            if (block[y][x]) {
                if (y < ystart) ystart = y;
                if (x < xstart) xstart = x;
            }
    for (y = ystart; y < 4; y ++)
        for (x = xstart; x < 4; x ++) {
            if (block[y][x]) {
                gdk_draw_drawable (nextpiecewidget->window,
                                   nextpiecewidget->style->black_gc,
                                   blockpix, (block[y][x]-1)*BLOCKSIZE, 0,
                                   BLOCKSIZE*(x-xstart)+BLOCKSIZE/4,
                                   BLOCKSIZE*(y-ystart)+BLOCKSIZE/4,
                                   BLOCKSIZE, BLOCKSIZE);
            }
        }
    memcpy (displayblock, block, 16);
}

void fields_attdefmsg (char *text)
{
    textbox_addtext (GTK_TEXT_VIEW(attdefwidget), text);
    adjust_bottom_text_view (GTK_TEXT_VIEW(attdefwidget));
}

void fields_attdeffmt (const char *fmt, ...)
{
    va_list ap;
    char *text = NULL;

    va_start(ap, fmt);
    text = g_strdup_vprintf(fmt,ap);
    va_end(ap);

    fields_attdefmsg (text); g_free(text);
}

void fields_attdefclear (void)
{
  gtk_text_buffer_set_text(GTK_TEXT_VIEW(attdefwidget)->buffer, "", 0);
}

void fields_setlines (int l)
{
    char buf[16] = "";
    if (l >= 0)
        g_snprintf (buf, sizeof(buf), "%d", l);
    leftlabel_set (lineswidget, buf);
}

void fields_setlevel (int l)
{
    char buf[16] = "";
    if (l > 0)
        g_snprintf (buf, sizeof(buf), "%d", l);
    leftlabel_set (levelwidget, buf);
}

void fields_setactivelevel (int l)
{
    char buf[16] = "";
    if (l <= 0) {
        gtk_widget_hide (activelabel);
        gtk_widget_hide (activewidget);
    }
    else {
        g_snprintf (buf, sizeof(buf), "%d", l);
        leftlabel_set (activewidget, buf);
        gtk_widget_show (activelabel);
        gtk_widget_show (activewidget);
    }
}

void fields_gmsgadd (const char *str)
{
    textbox_addtext (GTK_TEXT_VIEW(gmsgtext), str);
    adjust_bottom_text_view (GTK_TEXT_VIEW(gmsgtext));
}

void fields_gmsgclear (void)
{
  gtk_text_buffer_set_text(GTK_TEXT_VIEW(gmsgtext)->buffer, "", 0);
}

void fields_gmsginput (gboolean i)
{
    if (i) {
        gtk_widget_show (gmsginput);
    }
    else
        gtk_widget_hide (gmsginput);
}

void fields_gmsginputclear (void)
{
    gtk_entry_set_text (GTK_ENTRY (gmsginput), "");
    gtk_editable_set_position (GTK_EDITABLE (gmsginput), 0);
}

void fields_gmsginputactivate (int t)
{
    if (t)
    {
        fields_gmsginputclear ();
        gtk_widget_grab_focus (gmsginput);
    }
    else
        { /* do nothing */; }
}

void gmsginput_activate (void)
{
    gchar buf[512]; /* Increased from 256 to ease up for utf-8 sequences. - vidar */
    const gchar *s;

    if (gmsgstate == 0)
    {
        fields_gmsginputclear ();
        return;
    }
    s = fields_gmsginputtext ();
    if (strlen(s) > 0) {
        if (strncmp("/me ", s, 4) == 0) {
            /* post /me thingy */
            g_snprintf (buf, sizeof(buf), "* %s %s", nick, s+4);
            client_outmessage (OUT_GMSG,buf);
        }
        else {
            /* post message */
            g_snprintf (buf, sizeof(buf), "<%s> %s", nick, s);
            client_outmessage (OUT_GMSG, buf);
        }
    }
    fields_gmsginputclear ();
    fields_gmsginput (FALSE);
    unblock_keyboard_signal ();
    gmsgstate = 0;
}

const char *fields_gmsginputtext (void)
{
    return gtk_entry_get_text (GTK_ENTRY(gmsginput));
}
