/*************************************************************************/
/* Copyright (C) 2011-2012 matias <mati86dl@gmail.com>			 */
/* 									 */
/* This program is free software: you can redistribute it and/or modify	 */
/* it under the terms of the GNU General Public License as published by	 */
/* the Free Software Foundation, either version 3 of the License, or	 */
/* (at your option) any later version.					 */
/* 									 */
/* This program is distributed in the hope that it will be useful,	 */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of	 */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	 */
/* GNU General Public License for more details.				 */
/* 									 */
/* You should have received a copy of the GNU General Public License	 */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */
/*************************************************************************/

#include <stdio.h>
#include <pthread.h>
#include "pragha.h"

#ifdef HAVE_LIBCLASTFM

/* Functions related to current playlist. */

void *do_lastfm_current_playlist_love (gpointer data)
{
	gint rv;
	struct musicobject *mobj = NULL;

	struct con_win *cwin = data;

	CDEBUG(DBG_LASTFM, "Love thread of current playlist");

	mobj = get_selected_musicobject(cwin);

	rv = LASTFM_track_love (cwin->clastfm->session_id,
				mobj->tags->title,
				mobj->tags->artist);

	if (rv != LASTFM_STATUS_OK) {
		gdk_threads_enter ();
		set_status_message(_("Love song on Last.fm failed."), cwin);
		gdk_threads_leave ();
	}

	return NULL;
}

void lastfm_track_current_playlist_love_action (GtkAction *action, struct con_win *cwin)
{
	pthread_t tid;

	CDEBUG(DBG_LASTFM, "Love handler to current playlist");

	if(cwin->clastfm->status != LASTFM_STATUS_OK) {
		set_status_message(_("No connection Last.fm has been established."), cwin);
		return;
	}

	pthread_create(&tid, NULL, do_lastfm_current_playlist_love, cwin);
}

void *do_lastfm_current_playlist_unlove (gpointer data)
{
	gint rv;
	struct musicobject *mobj = NULL;

	struct con_win *cwin = data;

	CDEBUG(DBG_LASTFM, "Unlove thread on current playlist");

	mobj = get_selected_musicobject(cwin);

	rv = LASTFM_track_love (cwin->clastfm->session_id,
				mobj->tags->title,
				mobj->tags->artist);

	if (rv != LASTFM_STATUS_OK) {
		gdk_threads_enter ();
		set_status_message(_("Unlove song on Last.fm failed."), cwin);
		gdk_threads_leave ();
	}

	return NULL;
}

void lastfm_track_current_playlist_unlove_action (GtkAction *action, struct con_win *cwin)
{
	pthread_t tid;

	CDEBUG(DBG_LASTFM, "Unlove Handler to current playlist");

	if(cwin->clastfm->status != LASTFM_STATUS_OK) {
		set_status_message(_("No connection Last.fm has been established."), cwin);
		return;
	}

	pthread_create(&tid, NULL, do_lastfm_current_playlist_unlove, cwin);
}

void *do_lastfm_get_similar_current_playlist_action (gpointer data)
{
	LFMList *results = NULL, *li;
	LASTFM_TRACK_INFO *track = NULL;
	gint rv, added, try;
	gchar *summary = NULL;
	struct musicobject *mobj = NULL;

	struct con_win *cwin = data;

	mobj = get_selected_musicobject(cwin);

	set_watch_cursor_on_thread(cwin);

	rv = LASTFM_track_get_similar(cwin->clastfm->session_id,
				      mobj->tags->title,
				      mobj->tags->artist,
				      50, &results);

	if(rv != LASTFM_STATUS_OK) {
		remove_watch_cursor_on_thread("Error searching similar songs on Last.fm.", cwin);
		return NULL;
	}

	gdk_threads_enter();
	for(li=results, added=0, try=0 ; li; li=li->next) {
		track = li->data;
		try++;
		if (append_track_with_artist_and_title (track->artist, track->name, cwin))
			added++;
	}
	gdk_threads_leave();

	if(try > 0)
		summary = g_strdup_printf(_("Added %d songs of %d sugested from Last.fm."), added, try);
	else
		summary = g_strdup_printf(_("Last.fm not suggest any similar song."));

	remove_watch_cursor_on_thread(summary, cwin);

	LASTFM_free_track_info_list (results);
	g_free(summary);

	return NULL;
}

void lastfm_get_similar_current_playlist_action (GtkAction *action, struct con_win *cwin)
{
	pthread_t tid;

	CDEBUG(DBG_LASTFM, "Get similar action to current playlist");

	if(cwin->clastfm->session_id == NULL) {
		set_status_message(_("No connection Last.fm has been established."), cwin);
		return;
	}

	pthread_create (&tid, NULL, do_lastfm_get_similar_current_playlist_action, cwin);
}

/* Functions that respond to menu options. */

void lastfm_import_xspf_action (GtkAction *action, struct con_win *cwin)
{
	GtkWidget *dialog;
	GtkFileFilter *media_filter;
	XMLNode *xml = NULL, *xi, *xc, *xt;
	gchar *contents, *summary;
	gint try = 0, added = 0;
	GFile *file;
	gsize size;
	GdkCursor *cursor;

	dialog = gtk_file_chooser_dialog_new (_("Import a XSPF playlist"),
				      GTK_WINDOW(cwin->mainwindow),
				      GTK_FILE_CHOOSER_ACTION_OPEN,
				      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				      NULL);

	media_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(GTK_FILE_FILTER(media_filter), _("Supported media"));
	gtk_file_filter_add_mime_type(GTK_FILE_FILTER(media_filter), "application/xspf+xml");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), GTK_FILE_FILTER(media_filter));

	if(gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy (dialog);
		return;
	}

	file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));

	if (!g_file_load_contents (file, NULL, &contents, &size, NULL, NULL)) {
		goto out;
    	}

	if (g_utf8_validate (contents, -1, NULL) == FALSE) {
		gchar *fixed;
		fixed = g_convert (contents, -1, "UTF-8", "ISO8859-1", NULL, NULL, NULL);
		if (fixed != NULL) {
			g_free (contents);
			contents = fixed;
		}
	}

	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(GDK_WINDOW(cwin->mainwindow->window), cursor);
	gdk_cursor_unref(cursor);

	xml = tinycxml_parse(contents);

	xi = xmlnode_get(xml,CCA { "playlist","trackList","track",NULL},NULL,NULL);
	for(;xi;xi= xi->next) {
		try++;
		xt = xmlnode_get(xi,CCA {"track","title",NULL},NULL,NULL);
		xc = xmlnode_get(xi,CCA {"track","creator",NULL},NULL,NULL);

		if (xt && xc && append_track_with_artist_and_title (xc->content, xt->content, cwin))
			added++;
	}

	gdk_window_set_cursor(GDK_WINDOW(cwin->mainwindow->window), NULL);

	summary = g_strdup_printf(_("Added %d songs from %d of the imported playlist."), added, try);

	set_status_message(summary, cwin);

	gtk_widget_destroy (dialog);
	xmlnode_free(xml);
	g_free (contents);
	g_free(summary);
out:
	g_object_unref (file);
}

void *do_lastfm_add_favorites_action (gpointer data)
{
	LFMList *results = NULL, *li;
	LASTFM_TRACK_INFO *track;
	gint rpages = 0, cpage = 0, try = 0, added = 0;
	gchar *summary = NULL;

	struct con_win *cwin = data;

	set_watch_cursor_on_thread(cwin);

	do {
		rpages = LASTFM_user_get_loved_tracks(cwin->clastfm->session_id,
						     cwin->cpref->lw.lastfm_user,
						     cpage,
						     &results);
		gdk_threads_enter();
		for(li=results; li; li=li->next) {
			track = li->data;
			try++;
			if (append_track_with_artist_and_title (track->artist, track->name, cwin))
				added++;
		}
		gdk_threads_leave();

		LASTFM_free_track_info_list (results);
		cpage++;
	} while(rpages != 0);

	if(try > 0)
		summary = g_strdup_printf(_("Added %d songs of the last %d loved on Last.fm."), added, try);
	else
		summary = g_strdup_printf(_("You had no favorite songs on Last.fm."));

	remove_watch_cursor_on_thread(summary, cwin);

	g_free(summary);

	return NULL;
}

void lastfm_add_favorites_action (GtkAction *action, struct con_win *cwin)
{
	pthread_t tid;

	CDEBUG(DBG_LASTFM, "Add Favorites action");

	if ((cwin->clastfm->session_id == NULL) ||
	    (strlen(cwin->cpref->lw.lastfm_user) == 0)) {
		set_status_message(_("No connection Last.fm has been established."), cwin);
		return;
	}

	pthread_create (&tid, NULL, do_lastfm_add_favorites_action, cwin);
}

void *do_lastfm_get_similar_action (gpointer data)
{
	LFMList *results = NULL, *li;
	LASTFM_TRACK_INFO *track = NULL;
	gint rv, added, try;
	gchar *summary = NULL;

	struct con_win *cwin = data;

	set_watch_cursor_on_thread(cwin);

	rv = LASTFM_track_get_similar(cwin->clastfm->session_id,
			cwin->cstate->curr_mobj->tags->title,
			cwin->cstate->curr_mobj->tags->artist,
			50, &results);

	if(rv != LASTFM_STATUS_OK) {
		remove_watch_cursor_on_thread("Error searching similar songs on Last.fm.", cwin);
		return NULL;
	}

	gdk_threads_enter();
	for(li=results, added=0, try=0 ; li; li=li->next) {
		track = li->data;
		try++;
		if (append_track_with_artist_and_title (track->artist, track->name, cwin))
			added++;
	}
	gdk_threads_leave();

	if(try > 0)
		summary = g_strdup_printf(_("Added %d songs of %d sugested from Last.fm."), added, try);
	else
		summary = g_strdup_printf(_("Last.fm not suggest any similar song."));

	remove_watch_cursor_on_thread(summary, cwin);

	LASTFM_free_track_info_list (results);
	g_free(summary);

	return NULL;
}

void lastfm_get_similar_action (GtkAction *action, struct con_win *cwin)
{
	pthread_t tid;

	CDEBUG(DBG_LASTFM, "Get similar action");

	if(cwin->cstate->state == ST_STOPPED)
		return;

	if(cwin->clastfm->session_id == NULL) {
		set_status_message(_("No connection Last.fm has been established."), cwin);
		return;
	}

	pthread_create (&tid, NULL, do_lastfm_get_similar_action, cwin);
}

void *do_lastfm_love (gpointer data)
{
	gint rv;
	struct con_win *cwin = data;

	CDEBUG(DBG_LASTFM, "Love thread");

	rv = LASTFM_track_love (cwin->clastfm->session_id,
		cwin->cstate->curr_mobj->tags->title,
		cwin->cstate->curr_mobj->tags->artist);

	if (rv != LASTFM_STATUS_OK) {
		gdk_threads_enter ();
		set_status_message(_("Love song on Last.fm failed."), cwin);
		gdk_threads_leave ();
	}

	return NULL;
}

void lastfm_track_love_action (GtkAction *action, struct con_win *cwin)
{
	pthread_t tid;

	CDEBUG(DBG_LASTFM, "Love Handler");

	if(cwin->cstate->state == ST_STOPPED)
		return;

	if(cwin->clastfm->status != LASTFM_STATUS_OK) {
		set_status_message(_("No connection Last.fm has been established."), cwin);
		return;
	}

	pthread_create(&tid, NULL, do_lastfm_love, cwin);
}

void *do_lastfm_unlove (gpointer data)
{
	gint rv;
	struct con_win *cwin = data;

	CDEBUG(DBG_LASTFM, "Unlove thread");

	rv = LASTFM_track_love (cwin->clastfm->session_id,
		cwin->cstate->curr_mobj->tags->title,
		cwin->cstate->curr_mobj->tags->artist);

	if (rv != LASTFM_STATUS_OK) {
		gdk_threads_enter ();
		set_status_message(_("Unlove song on Last.fm failed."), cwin);
		gdk_threads_leave ();
	}

	return NULL;
}

void lastfm_track_unlove_action (GtkAction *action, struct con_win *cwin)
{
	pthread_t tid;

	CDEBUG(DBG_LASTFM, "Unlove Handler");

	if(cwin->cstate->state == ST_STOPPED)
		return;

	if(cwin->clastfm->status != LASTFM_STATUS_OK) {
		set_status_message(_("No connection Last.fm has been established."), cwin);
		return;
	}

	pthread_create(&tid, NULL, do_lastfm_unlove, cwin);
}

void *do_lastfm_scrob (gpointer data)
{
	gint rv;
	struct con_win *cwin = data;

	CDEBUG(DBG_LASTFM, "Scrobbler thread");

	rv = LASTFM_track_scrobble (cwin->clastfm->session_id,
		cwin->cstate->curr_mobj->tags->title,
		cwin->cstate->curr_mobj->tags->album,
		cwin->cstate->curr_mobj->tags->artist,
		cwin->clastfm->playback_started,
		cwin->cstate->curr_mobj->tags->length,
		cwin->cstate->curr_mobj->tags->track_no,
		0, NULL);

	gdk_threads_enter ();
	if (rv != LASTFM_STATUS_OK)
		set_status_message("Last.fm submission failed", cwin);
	else
		set_status_message("Track scrobbled on Last.fm", cwin);
	gdk_threads_leave ();


	return NULL;
}

gboolean lastfm_scrob_handler(gpointer data)
{
	pthread_t tid;
	struct con_win *cwin = data;

	CDEBUG(DBG_LASTFM, "Scrobbler Handler");

	if(cwin->cstate->state == ST_STOPPED)
		return FALSE;

	if(cwin->clastfm->status != LASTFM_STATUS_OK) {
		set_status_message(_("No connection Last.fm has been established."), cwin);
		return FALSE;
	}

	pthread_create(&tid, NULL, do_lastfm_scrob, cwin);

	return FALSE;
}

void *do_lastfm_now_playing (gpointer data)
{
	gint rv;

	struct con_win *cwin = data;

	CDEBUG(DBG_LASTFM, "Update now playing thread");

	rv = LASTFM_track_update_now_playing (cwin->clastfm->session_id,
		cwin->cstate->curr_mobj->tags->title,
		cwin->cstate->curr_mobj->tags->album,
		cwin->cstate->curr_mobj->tags->artist,
		cwin->cstate->curr_mobj->tags->length,
		cwin->cstate->curr_mobj->tags->track_no,
		0);

	if (rv != LASTFM_STATUS_OK) {
		gdk_threads_enter ();
		set_status_message(_("Update current song on Last.fm failed."), cwin);
		gdk_threads_leave ();
	}

	return NULL;
}

void lastfm_now_playing_handler (struct con_win *cwin)
{
	pthread_t tid;
	gint length;

	CDEBUG(DBG_LASTFM, "Update now playing Handler");

	if(cwin->cstate->state == ST_STOPPED)
		return;

	if((strlen(cwin->cpref->lw.lastfm_user) == 0) ||
	   (strlen(cwin->cpref->lw.lastfm_pass) == 0))
		return;

	if(cwin->clastfm->status != LASTFM_STATUS_OK) {
		set_status_message(_("No connection Last.fm has been established."), cwin);
		return;
	}

	if ((strlen(cwin->cstate->curr_mobj->tags->artist) == 0) ||
	    (strlen(cwin->cstate->curr_mobj->tags->title) == 0))
		return;

	if(cwin->cstate->curr_mobj->tags->length < 30)
		return;

	pthread_create(&tid, NULL, do_lastfm_now_playing, cwin);

	/* Kick the lastfm scrobbler on
	 * Note: Only scrob if tracks is more than 30s.
	 * and scrob when track is at 50% or 4mins, whichever comes
	 * first */

	if((cwin->cstate->curr_mobj->tags->length / 2) > (240 - WAIT_UPDATE)) {
		length = 240 - WAIT_UPDATE;
	}
	else {
		length = (cwin->cstate->curr_mobj->tags->length / 2) - WAIT_UPDATE;
	}

	cwin->related_timeout_id = gdk_threads_add_timeout_seconds_full(
			G_PRIORITY_DEFAULT_IDLE, length,
			lastfm_scrob_handler, cwin, NULL);

	return;
}

/* Init lastfm with a simple thread when change preferences and show error messages. */

gboolean do_just_init_lastfm(gpointer data)
{
	struct con_win *cwin = data;

	cwin->clastfm->session_id = LASTFM_init(LASTFM_API_KEY, LASTFM_SECRET);

	if (cwin->clastfm->session_id != NULL) {
		if((strlen(cwin->cpref->lw.lastfm_user) != 0) &&
		   (strlen(cwin->cpref->lw.lastfm_pass) != 0)) {
			cwin->clastfm->status = LASTFM_login (cwin->clastfm->session_id,
							      cwin->cpref->lw.lastfm_user,
							      cwin->cpref->lw.lastfm_pass);

			if(cwin->clastfm->status != LASTFM_STATUS_OK) {
				CDEBUG(DBG_INFO, "Failure to login on lastfm");
				set_status_message(_("No connection Last.fm has been established."), cwin);
			}
		}
	}
	else {
		CDEBUG(DBG_INFO, "Failure to init libclastfm");
		set_status_message(_("No connection Last.fm has been established."), cwin);
	}

	return FALSE;
}

gint just_init_lastfm (struct con_win *cwin)
{
	if (cwin->cpref->lw.lastfm_support) {
		CDEBUG(DBG_INFO, "Initializing LASTFM");
		gdk_threads_add_idle (do_just_init_lastfm, cwin);
	}
	return 0;
}

/* When just launch pragha init lastfm immediately if has internet or otherwise waiting 30 seconds.
 * And no show any error. */

gboolean do_init_lastfm_idle(gpointer data)
{
	struct con_win *cwin = data;

	cwin->clastfm->session_id = LASTFM_init(LASTFM_API_KEY, LASTFM_SECRET);

	if (cwin->clastfm->session_id != NULL) {
		if((strlen(cwin->cpref->lw.lastfm_user) != 0) &&
		   (strlen(cwin->cpref->lw.lastfm_pass) != 0)) {
			cwin->clastfm->status = LASTFM_login (cwin->clastfm->session_id,
							      cwin->cpref->lw.lastfm_user,
							      cwin->cpref->lw.lastfm_pass);

			if(cwin->clastfm->status != LASTFM_STATUS_OK)
				CDEBUG(DBG_INFO, "Failure to login on lastfm");
		}
	}
	else {
		CDEBUG(DBG_INFO, "Failure to init libclastfm");
	}

	return FALSE;
}

gint init_lastfm_idle(struct con_win *cwin)
{
	if (cwin->cpref->lw.lastfm_support) {
		CDEBUG(DBG_INFO, "Initializing LASTFM");

		if(nm_is_online () == TRUE)
			gdk_threads_add_idle (do_init_lastfm_idle, cwin);
		else
			gdk_threads_add_timeout_seconds_full(
					G_PRIORITY_DEFAULT_IDLE, 30,
					do_init_lastfm_idle, cwin, NULL);
	}
	return 0;
}
#endif
