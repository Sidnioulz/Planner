/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio AB
 * Copyright (C) 2002-2003 CodeFactory AB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002-2003 Mikael Hallendal <micke@imendio.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <string.h>
#include <math.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <libplanner/mrp-error.h>
#include <libplanner/mrp-project.h>
#include <libplanner/mrp-paths.h>
#include "planner-marshal.h"
#include "planner-conf.h"
#include "planner-sidebar.h"
#include "planner-window.h"
#include "planner-plugin-loader.h"
#include "planner-project-properties.h"
#include "planner-phase-dialog.h"
#include "planner-calendar-dialog.h"
#include "planner-day-type-dialog.h"
#include "planner-print-dialog.h"
#include "planner-view.h"
#include "planner-gantt-view.h"
#include "planner-task-view.h"
#include "planner-resource-view.h"
#include "planner-usage-view.h"
#include "planner-util.h"

#define d(x)

struct _PlannerWindowPriv {
	PlannerApplication  *application;

	GtkUIManager        *ui_manager;
	GtkActionGroup      *actions;
	GtkActionGroup      *view_actions;

	PlannerCmdManager   *cmd_manager;

	MrpProject          *project;

	GtkWidget           *statusbar;
	GtkWidget           *ui_box;
	GtkWidget           *sidebar;
	GtkWidget           *notebook;

	GtkWidget           *properties_dialog;
	GtkWidget           *calendar_dialog;
	GtkWidget           *day_type_dialog;
	GtkWidget           *phase_dialog;

	PlannerView         *current_view;
	GList               *views;
	GList               *plugins;
	GTimer              *last_saved;
};

/* Drop targets. */
enum {
	TARGET_STRING,
	TARGET_URI_LIST
};

static const GtkTargetEntry drop_target_types[] = {
	{ "STRING",     0, TARGET_STRING },
	{ "text/plain", 0, TARGET_STRING },
	{ "text/uri-list", 0, TARGET_URI_LIST },
};

/* Signals */
enum {
	CLOSED,
	NEW_WINDOW,
	EXIT,
	LAST_SIGNAL
};

/* Links type */
enum {
	LINK_TYPE_EMAIL,
	LINK_TYPE_URL
};

static void       window_class_init                      (PlannerWindowClass           *klass);
static void       window_init                            (PlannerWindow                *window);
static void       window_finalize                        (GObject                      *object);
static void       window_populate                        (PlannerWindow                *window);
static void       window_view_selected                   (PlannerSidebar               *sidebar,
							  gint                          index,
							  PlannerWindow                *window);
static void       window_view_cb                         (GtkAction                    *action,
							  GtkRadioAction               *current,
							  gpointer                      data);
static void       window_new_cb                          (GtkAction                    *action,
							  gpointer                      data);
static void       window_open_cb                         (GtkAction                    *action,
							  gpointer                      data);
static void       window_save_as_cb                      (GtkAction                    *action,
							  gpointer                      data);
static void       window_save_cb                         (GtkAction                    *action,
							  gpointer                      data);
static void       window_page_setup_cb                   (GtkAction                    *action,
							  gpointer                      data);
static void       window_print_cb                        (GtkAction                    *action,
							  gpointer                      data);
static void       window_close_cb                        (GtkAction                    *action,
							  gpointer                      data);
static void       window_exit_cb                         (GtkAction                    *action,
							  gpointer                      data);
static void       window_undo_cb                         (GtkAction                    *action,
							  gpointer                      data);
static void       window_redo_cb                         (GtkAction                    *action,
							  gpointer                      data);
static void       window_project_props_cb                (GtkAction                    *action,
							  gpointer                      data);
static void       window_manage_calendars_cb             (GtkAction                    *action,
							  gpointer                      data);
static void       window_edit_day_types_cb               (GtkAction                    *action,
							  gpointer                      data);
static void       window_edit_phases_cb                  (GtkAction                    *action,
							  gpointer                      data);
static void       window_help_cb                         (GtkAction                    *action,
							  gpointer                      data);
static void       window_about_cb                        (GtkAction                    *action,
							  gpointer                      data);
static gboolean   window_delete_event_cb                 (PlannerWindow                *window,
							  gpointer                      user_data);
static void       window_undo_state_changed_cb           (PlannerCmdManager            *manager,
							  gboolean                      state,
							  const gchar                  *label,
							  PlannerWindow                *window);
static void       window_redo_state_changed_cb           (PlannerCmdManager            *manager,
							  gboolean                      state,
							  const gchar                  *label,
							  PlannerWindow                *window);
static void       window_drag_data_received_cb           (GtkWidget                    *widget,
							  GdkDragContext               *context,
							  int                           x,
							  int                           y,
							  GtkSelectionData             *data,
							  guint                         info,
							  guint                         time,
							  PlannerWindow                *window);
static void       window_project_needs_saving_changed_cb (MrpProject                   *project,
							  gboolean                      needs_saving,
							  PlannerWindow                *window);
static void       window_project_notify_name_cb          (MrpProject                   *project,
							  GParamSpec                   *pspec,
							  PlannerWindow                *window);
static gboolean   window_confirm_exit_run                (PlannerWindow                *window);
static gboolean   window_do_save                         (PlannerWindow                *window,
							  gboolean                      force);
static gboolean   window_do_save_as                      (PlannerWindow                *window);
static gchar *    window_get_name                        (PlannerWindow                *window);
static void       window_update_title                    (PlannerWindow                *window);
static GtkWidget *window_create_dialog_button            (const gchar                  *icon_name,
							  const gchar                  *text);
static void       window_recent_add_item                 (PlannerWindow                *window,
							  const gchar                  *uri);
static void       window_save_state                      (PlannerWindow *window);
static void       window_restore_state                   (PlannerWindow *window);

static void window_disconnect_proxy_cb (GtkUIManager  *manager,
					GtkAction     *action,
					GtkWidget     *proxy,
					PlannerWindow *window);

static void window_connect_proxy_cb (GtkUIManager  *manager,
				     GtkAction     *action,
				     GtkWidget     *proxy,
				     PlannerWindow *window);

#define CONF_WINDOW_DIR       "/ui"
#define CONF_WINDOW_MAXIMIZED "/ui/main_window_maximized"
#define CONF_WINDOW_WIDTH     "/ui/main_window_width"
#define CONF_WINDOW_HEIGHT    "/ui/main_window_height"
#define CONF_WINDOW_POS_X     "/ui/main_window_position_x"
#define CONF_WINDOW_POS_Y     "/ui/main_window_position_y"
#define CONF_ACTIVE_VIEW           "/ui/active_view"
#define CONF_LAST_DIR         "/general/last_dir"

#define DEFAULT_WINDOW_WIDTH  800
#define DEFAULT_WINDOW_HEIGHT 550

#define VIEW_PATH "/menu/View/Views placeholder"
#define VIEW_GROUP "view group"

static GtkWindowClass *parent_class = NULL;
static guint signals[LAST_SIGNAL];


static GtkActionEntry entries[] = {
	{ "File",
	  NULL, N_("_File"), NULL, NULL,
	  NULL
	},
	{ "FileNew",
	  GTK_STOCK_NEW,           N_("_New Project"),             "<Control>n",        N_("Create a new project"),
	  G_CALLBACK (window_new_cb)
	},
	{ "FileOpen",
	  GTK_STOCK_OPEN,          N_("_Open..."),                 "<Control>o",        N_("Open a project"),
	  G_CALLBACK (window_open_cb) },
	{ "FileOpenRecent",
	  NULL,                    N_("Open _Recent"),             NULL,                NULL,
	  NULL },
	{ "Import",
	  NULL,                    N_("_Import"),                  NULL,                NULL,
	  NULL },

	{ "FileSave",
	  GTK_STOCK_SAVE,          N_("_Save"),                    "<Control>s",        N_("Save the current project"),
	  G_CALLBACK (window_save_cb) },
	{ "Export",
	  NULL,                    N_("_Export"),                  NULL,                NULL,
	  NULL },
	{ "FileSaveAs",
	  GTK_STOCK_SAVE_AS,       N_("Save _As..."),              "<Shift><Control>s", N_("Save the current project with a different name"),
	  G_CALLBACK (window_save_as_cb) },
	{ "FilePageSetup",
	  NULL,                    N_("Page Set_up..."),           NULL,                N_("Setup the page settings for your current printer"),
	  G_CALLBACK (window_page_setup_cb) },
	{ "FilePrint",
	  GTK_STOCK_PRINT,         N_("_Print..."),                "<Control>p",        N_("Print the current project"),
	  G_CALLBACK (window_print_cb) },
	{ "FilePrintPreview",
	  GTK_STOCK_PRINT_PREVIEW, N_("Print Pre_view"),           "<Shift><Control>p", N_("Print preview of the current project"),
	  G_CALLBACK (window_print_cb) },
	{ "FileClose",
	  GTK_STOCK_CLOSE,         N_("_Close Project"),           "<Control>q",        N_("Close the current file"),
	  G_CALLBACK (window_close_cb) },
	{ "FileExit",
	  GTK_STOCK_QUIT,          N_("_Quit"),                    "<Shift><Control>q", N_("Exit the program"),
	  G_CALLBACK (window_exit_cb) },

	{ "Edit",
	  NULL,                    N_("_Edit"),                    NULL,                NULL,
	  NULL },
	{ "EditUndo",
	  GTK_STOCK_UNDO,          N_("_Undo"),                    "<Control>z",        N_("Undo the last action"),
	  G_CALLBACK (window_undo_cb) },
	{ "EditRedo",
	  GTK_STOCK_REDO,          N_("_Redo"),                    "<Shift><Control>z", N_("Redo the undone action"),
	  G_CALLBACK (window_redo_cb) },

	{ "View",
	  NULL,                    N_("_View"),                    NULL,                NULL,
	  NULL },

	{ "Actions",
	  NULL,                    N_("_Actions"),                 NULL,                NULL,
	  NULL },

	{ "Project",
	  NULL,                    N_("_Project"),                 NULL,                NULL,
	  NULL },
	{ "ManageCalendars",
	  NULL,                    N_("_Manage Calendars"),        NULL,                NULL,
	  G_CALLBACK (window_manage_calendars_cb) },
	{ "EditDayTypes",
	  NULL,                    N_("Edit Day _Types"),          NULL,                NULL,
	  G_CALLBACK (window_edit_day_types_cb) },
	{ "EditPhases",
	  NULL,                    N_("Edit Project _Phases"),     NULL,                NULL,
	  G_CALLBACK (window_edit_phases_cb) },
	{ "EditProjectProps",
	  GTK_STOCK_PROPERTIES,    N_("_Edit Project Properties"), NULL,                N_("Edit the project properties"),
	  G_CALLBACK (window_project_props_cb) },

/*	{ "PreferencesEditPreferences",
	NULL,                    NULL,                           NULL,                NULL,
	G_CALLBACK (window_preferences_cb) },*/

	{ "Help",
	  NULL,                    N_("_Help"),                    NULL,                NULL,
	  NULL },
	{ "HelpHelp",
	  GTK_STOCK_HELP,          N_("_User Guide"),              "F1",                N_("Show the Planner User Guide"),
	  G_CALLBACK (window_help_cb) },
	{ "HelpAbout",
	  GTK_STOCK_ABOUT,         N_("_About"),                   NULL,                N_("About this application"),
	  G_CALLBACK (window_about_cb) },
};

static guint n_entries = G_N_ELEMENTS(entries);


GType
planner_window_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (PlannerWindowClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) window_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (PlannerWindow),
			0,              /* n_preallocs */
			(GInstanceInitFunc) window_init
		};

		type = g_type_register_static (GTK_TYPE_WINDOW,
					       "PlannerWindow", &info, 0);
	}

	return type;
}

static void
window_class_init (PlannerWindowClass *klass)
{
	GObjectClass *o_class;

	parent_class = g_type_class_peek_parent (klass);

	o_class = (GObjectClass *) klass;

	/* GObject functions */
	o_class->finalize = window_finalize;

	/* Signals */
	signals[CLOSED] = g_signal_new ("closed",
					G_TYPE_FROM_CLASS (klass),
					G_SIGNAL_RUN_LAST,
					0, /*G_STRUCT_OFFSET (PlannerWindowClass, method), */
					NULL, NULL,
					planner_marshal_VOID__VOID,
					G_TYPE_NONE, 0);
}

static void
window_init (PlannerWindow *window)
{
	PlannerWindowPriv *priv;

	priv = g_new0 (PlannerWindowPriv, 1);
	window->priv = priv;

	priv->views = NULL;

	/* Setup drag-n-drop. */
	gtk_drag_dest_set (GTK_WIDGET (window),
			   GTK_DEST_DEFAULT_ALL,
			   drop_target_types,
			   G_N_ELEMENTS (drop_target_types),
			   GDK_ACTION_COPY);

	g_signal_connect (window,
			  "drag_data_received",
			  G_CALLBACK (window_drag_data_received_cb),
			  window);

	priv->cmd_manager = planner_cmd_manager_new ();

	g_signal_connect (priv->cmd_manager,
			  "undo_state_changed",
			  G_CALLBACK (window_undo_state_changed_cb),
			  window);

	g_signal_connect (priv->cmd_manager,
			  "redo_state_changed",
			  G_CALLBACK (window_redo_state_changed_cb),
			  window);
}

static void
window_finalize (GObject *object)
{
	PlannerWindow     *window = PLANNER_WINDOW (object);
	PlannerWindowPriv *priv = window->priv;

	if (priv->last_saved) {
		g_timer_destroy (priv->last_saved);
	}
	if (priv->plugins) {
		planner_plugin_loader_unload (priv->plugins);
		g_list_free (priv->plugins);
	}
	if (priv->views) {
		g_list_foreach (priv->views, (GFunc) g_object_unref, NULL);
		g_list_free (priv->views);
	}
	/* FIXME: check if project should be unreffed */
	if (priv->cmd_manager) {
		g_object_unref (priv->cmd_manager);
	}

	if (priv->view_actions) {
		g_object_unref (priv->view_actions);
	}
	if (priv->actions) {
		g_object_unref (priv->actions);
	}
	if (priv->ui_manager) {
		g_object_unref (priv->ui_manager);
	}

	if (priv->application) {
		g_object_unref (priv->application);
	}

	g_free (window->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
recent_chooser_item_activated (GtkRecentChooser *chooser, gpointer user_data)
{
	gchar *uri;
	PlannerWindow *window;

	g_return_if_fail (PLANNER_IS_WINDOW (user_data));

	window = PLANNER_WINDOW (user_data);

	uri = gtk_recent_chooser_get_current_uri (chooser);
	if (uri != NULL) {
		planner_window_open_in_existing_or_new (window, uri, FALSE);
		g_free (uri);
	}
}

static void
window_add_widget (GtkUIManager  *merge,
		   GtkWidget     *widget,
		   PlannerWindow *window)
{
	PlannerWindowPriv *priv;

	priv = window->priv;

	gtk_box_pack_start (GTK_BOX (priv->ui_box), widget, FALSE, FALSE, 0);
}

static void
window_add_stock_icon (GtkIconFactory *icon_factory,
		       const gchar    *stock_id,
		       const gchar    *filename)
{
	gchar      *path;
	GdkPixbuf  *pixbuf;
	GtkIconSet *icon_set;

	path = mrp_paths_get_image_dir (filename);
	pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
	g_object_unref (pixbuf);
	gtk_icon_factory_add (icon_factory, stock_id, icon_set);
	g_free (path);
}

static void
window_add_stock_icons (void)
{
	static GtkIconFactory *icon_factory = NULL;

	if (icon_factory) {
		return;
	}

	icon_factory = gtk_icon_factory_new ();
	gtk_icon_factory_add_default (icon_factory);

	/* Task/Gantt icons. */
	window_add_stock_icon (icon_factory,
			       "planner-stock-insert-task",
			       "24_insert_task.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-remove-task",
			       "24_remove_task.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-unlink-task",
			       "24_unlink_task.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-link-task",
			       "24_link_task.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-indent-task",
			       "24_indent_task.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-unindent-task",
			       "24_unindent_task.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-move-task-up",
			       "24_task_up.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-move-task-down",
			       "24_task_down.png");

	/* Resource icons. */
	window_add_stock_icon (icon_factory,
			       "planner-stock-insert-resource",
			       "/24_insert_resource.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-remove-resource",
			       "24_remove_resource.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-edit-resource",
			       "24_edit_resource.png");

	window_add_stock_icon (icon_factory,
			       "planner-stock-edit-groups",
			       "24_groups.png");
}

static void
window_populate (PlannerWindow *window)
{
	PlannerWindowPriv    *priv;
	GtkWidget            *hbox;
	GList                *l;
	GtkWidget            *view_widget;
	GtkWidget            *recent_view;
	GtkRecentFilter      *filter;
	GtkWidget            *open_recent;
	PlannerView          *view;
	gint                  view_num;
	GtkRadioActionEntry  *r_entries;
	gchar                *xml_string_tmp, *xml_string;
	gchar                *str;
	gchar                *filename;
	const gchar          *xml_string_full =
		"<ui>"
		"<menubar name='MenuBar'>"
		"<menu action='View'>"
		"<placeholder name='Views placeholder'>"
		"%s"
		"</placeholder>"
		"</menu>"
		"</menubar>"
		"</ui>";

	priv = window->priv;

	window_add_stock_icons ();

	priv->ui_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), priv->ui_box);

	priv->actions = gtk_action_group_new ("Planner");

	gtk_action_group_set_translation_domain (priv->actions, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (priv->actions, entries, n_entries, window);

	priv->ui_manager = gtk_ui_manager_new ();

	g_signal_connect (priv->ui_manager,
			  "add_widget",
			  G_CALLBACK (window_add_widget),
			  window);
	g_signal_connect (priv->ui_manager,
			  "connect_proxy",
			  G_CALLBACK (window_connect_proxy_cb),
			  window);
	g_signal_connect (priv->ui_manager,
			  "disconnect_proxy",
			  G_CALLBACK (window_disconnect_proxy_cb),
			  window);

	gtk_ui_manager_insert_action_group (priv->ui_manager, priv->actions, 0);
	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (priv->ui_manager));

	filename = mrp_paths_get_ui_dir ("main-window.ui");
	gtk_ui_manager_add_ui_from_file (priv->ui_manager,
					 filename ,
					 NULL);
	g_free (filename);

	g_object_set (gtk_action_group_get_action (priv->actions, "EditUndo"),
		      "sensitive", FALSE,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "EditRedo"),
		      "sensitive", FALSE,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "FileSave"),
		      "sensitive", FALSE,
		      NULL);

	/* Make the Actions menu always visible even when empty. */
	g_object_set (gtk_action_group_get_action (priv->actions, "Actions"),
		      "hide-if-empty", FALSE,
		      NULL);

	/* Handle recent file stuff. */
	recent_view = gtk_recent_chooser_menu_new_for_manager (
		planner_application_get_recent_model (priv->application));

	filter = gtk_recent_filter_new ();
	gtk_recent_filter_add_mime_type (filter, "application/x-planner");
	gtk_recent_filter_add_mime_type (filter, "application/x-mrproject");
	gtk_recent_filter_add_group (filter, "planner");
	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (recent_view), filter);

	g_signal_connect (recent_view,
			  "item_activated",
			  G_CALLBACK (recent_chooser_item_activated),
			  window);

	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (recent_view), GTK_RECENT_SORT_MRU);
	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (recent_view), TRUE);
	gtk_recent_chooser_set_limit (GTK_RECENT_CHOOSER (recent_view), 5);
	gtk_recent_chooser_menu_set_show_numbers (GTK_RECENT_CHOOSER_MENU (recent_view), TRUE);

	open_recent = gtk_ui_manager_get_widget (priv->ui_manager, "/MenuBar/File/FileOpenRecent");
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (open_recent), recent_view);

	hbox = gtk_hbox_new (FALSE, 0);

	priv->sidebar = planner_sidebar_new ();
	gtk_box_pack_start (GTK_BOX (hbox), priv->sidebar, FALSE, TRUE, 0);
	g_signal_connect (priv->sidebar,
			  "icon-selected",
			  G_CALLBACK (window_view_selected),
			  window);

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), priv->notebook, TRUE, TRUE, 0);

	priv->statusbar = gtk_statusbar_new ();
	gtk_box_pack_end (GTK_BOX (priv->ui_box), priv->statusbar, FALSE, TRUE, 0);
	gtk_widget_show (priv->statusbar);

	gtk_box_pack_end (GTK_BOX (priv->ui_box), hbox, TRUE, TRUE, 0);

	/* Create views. */
	priv->view_actions = gtk_action_group_new ("View Actions");
	gtk_ui_manager_insert_action_group (priv->ui_manager, priv->view_actions, 0);

	priv->views = g_list_append (priv->views, planner_gantt_view_new ());
	priv->views = g_list_append (priv->views, planner_task_view_new ());
	priv->views = g_list_append (priv->views, planner_resource_view_new ());
	priv->views = g_list_append (priv->views, planner_usage_view_new ());

	view_num = 0;
	xml_string = g_strdup ("");
	r_entries  = g_new0 (GtkRadioActionEntry, g_list_length (priv->views));
	for (l = priv->views; l; l = l->next, view_num++ ) {
		view = l->data;

		planner_view_setup (view, window);

		view_widget = planner_view_get_widget (view);
		gtk_widget_show (view_widget);

		planner_sidebar_append (PLANNER_SIDEBAR (priv->sidebar),
					planner_view_get_icon (view),
					planner_view_get_label (view));

		r_entries[view_num].name  = planner_view_get_name (view);
		r_entries[view_num].label = planner_view_get_menu_label (view);
		r_entries[view_num].value = view_num;
		r_entries[view_num].tooltip = g_strdup_printf (_("Switch to the view \"%s\""),
							       planner_view_get_label (view));

		xml_string_tmp = xml_string;
		xml_string = g_strdup_printf ("%s<menuitem action='%s'/>",
					      xml_string, r_entries[view_num].name);
		g_free (xml_string_tmp);

		gtk_notebook_append_page (
			GTK_NOTEBOOK (priv->notebook),
			view_widget,
			gtk_label_new (planner_view_get_label (view)));
	}

	gtk_action_group_add_radio_actions (priv->view_actions,
					    r_entries,
					    g_list_length (priv->views), 0,
					    G_CALLBACK (window_view_cb),
					    window);

	view_num = 0;
	for (l = priv->views; l; l = l->next, view_num++ ) {
		/* Cast off const so we can free the string we allocated */
		gchar *tooltip = (gchar *)(r_entries[view_num].tooltip);
		g_free (tooltip);
	}
	g_free (r_entries);

	xml_string_tmp = g_strdup_printf (xml_string_full, xml_string);
	gtk_ui_manager_add_ui_from_string (priv->ui_manager, xml_string_tmp, -1, NULL);
	g_free (xml_string);
	g_free (xml_string_tmp);

	gtk_ui_manager_ensure_update (priv->ui_manager);

	/* Load plugins. */
	priv->plugins = planner_plugin_loader_load (window);

	str = planner_conf_get_string (CONF_ACTIVE_VIEW, NULL);
	if (str) {
		gboolean found;

		found = FALSE;
		view_num = 0;
		for (l = priv->views; l; l = l->next, view_num++ ) {
			view = l->data;

			if (strcmp (str, planner_view_get_name (view)) == 0) {
				found = TRUE;
				break;
			}
		}

		if (!found) {
			view_num = 0;
		}

		window_view_selected (PLANNER_SIDEBAR (priv->sidebar), view_num, window);
		g_free (str);
	} else {
		window_view_selected (PLANNER_SIDEBAR (priv->sidebar), 0, window);
	}
}

static void
window_view_selected (PlannerSidebar *sidebar,
		      gint            index,
		      PlannerWindow  *window)
{
	PlannerWindowPriv *priv;
	GList             *list;
	PlannerView       *view;
	GtkAction         *action;

	priv = window->priv;

	list = g_list_nth (priv->views, index);
	if (list != NULL) {
		view = list->data;
	} else {
		view = NULL;
	}

	if (view == priv->current_view) {
		return;
	}

	if (priv->current_view != NULL) {
		planner_view_deactivate (priv->current_view);
	}

	if (view != NULL) {
		planner_view_activate (view);
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), index);

	g_signal_handlers_block_by_func (sidebar, window_view_selected, window);
	planner_sidebar_set_active (PLANNER_SIDEBAR (sidebar), index);
	g_signal_handlers_unblock_by_func (sidebar, window_view_selected, window);

	action = gtk_action_group_get_action (priv->view_actions,
					      planner_view_get_name (view));
	if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) ) {
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}

	priv->current_view = view;

	planner_conf_set_string (CONF_ACTIVE_VIEW, planner_view_get_name (view), NULL);
}

static void
window_view_cb (GtkAction      *action,
		GtkRadioAction *current,
		gpointer        data)
{
	PlannerWindow     *window;
	GtkWidget         *sidebar;
	guint              index;

	window  = PLANNER_WINDOW (data);
	sidebar = window->priv->sidebar;

	index = gtk_radio_action_get_current_value(current);

	planner_sidebar_set_active (PLANNER_SIDEBAR (sidebar), index);
}

static void
window_new_cb (GtkAction *action,
	       gpointer   data)
{
	PlannerWindow     *window;
	PlannerWindowPriv *priv;
	GtkWidget         *new_window;

	window = PLANNER_WINDOW (data);
	priv   = window->priv;

	new_window = planner_application_new_window (priv->application);
	gtk_widget_show_all (new_window);
}

static gchar *
get_last_dir (PlannerWindow *window)
{
	gchar             *last_dir;

	last_dir = planner_conf_get_string (CONF_LAST_DIR, NULL);

	if (last_dir == NULL) {
		last_dir = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS));
	}

	return last_dir;
}

static void
window_open_cb (GtkAction *action,
		gpointer   data)
{
	PlannerWindow     *window;
	GtkWidget         *file_chooser;
	GtkFileFilter     *filter;
	gint               response;
	gchar             *last_dir;

	window = PLANNER_WINDOW (data);

	file_chooser = gtk_file_chooser_dialog_new (_("Open a File"),
						    GTK_WINDOW (window),
						    GTK_FILE_CHOOSER_ACTION_OPEN,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						    GTK_STOCK_OPEN, GTK_RESPONSE_OK,
						    NULL);

	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (file_chooser), TRUE);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Planner Files"));
	gtk_file_filter_add_pattern (filter, "*.planner");
	gtk_file_filter_add_pattern (filter, "*.mrproject");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All Files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser), filter);

	last_dir = get_last_dir (window);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (file_chooser), last_dir);
	g_free (last_dir);

	gtk_window_set_modal (GTK_WINDOW (file_chooser), TRUE);

	gtk_widget_show (file_chooser);

	response = gtk_dialog_run (GTK_DIALOG (file_chooser));
	gtk_widget_hide (file_chooser);

	if (response == GTK_RESPONSE_OK) {
		GSList *uris, *l;

		uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (file_chooser));

		if (uris) {
			gchar *filename;

			filename = g_filename_from_uri (uris->data, NULL, NULL);
			if (filename) {
				last_dir = g_path_get_dirname (filename);
				g_free (filename);
				planner_conf_set_string (CONF_LAST_DIR, last_dir, NULL);
				g_free (last_dir);
			}
		}

		for (l = uris; l; l = l->next) {
			planner_window_open_in_existing_or_new (window, l->data, FALSE);
		}

		g_slist_foreach (uris, (GFunc) g_free, NULL);
		g_slist_free (uris);
	}

	gtk_widget_destroy (file_chooser);
}

static void
window_save_as_cb (GtkAction *action,
		   gpointer   data)
{
	PlannerWindow *window;

	window = PLANNER_WINDOW (data);

        window_do_save_as (window);
}

static void
window_save_cb (GtkAction *action,
		gpointer   data)
{
	PlannerWindow     *window;

	window = PLANNER_WINDOW (data);

	window_do_save (window, FALSE);
}

static void
window_page_setup_cb (GtkAction *action,
		      gpointer   data)
{
	PlannerWindow     *window;
	GtkPageSetup      *old_page_setup, *new_page_setup;
	GtkPrintSettings  *settings;

	window = PLANNER_WINDOW (data);

	settings = planner_print_dialog_load_print_settings ();
	old_page_setup = planner_print_dialog_load_page_setup ();

	new_page_setup = gtk_print_run_page_setup_dialog (GTK_WINDOW (window), old_page_setup, settings);

	g_object_unref (old_page_setup);

	planner_print_dialog_save_page_setup (new_page_setup);
	planner_print_dialog_save_print_settings (settings);

	g_object_unref (new_page_setup);
	g_object_unref (settings);
}

static void
window_print_cb (GtkAction *action,
		 gpointer   data)
{
	PlannerWindow           *window;
	PlannerWindowPriv       *priv;
	GtkPrintOperation       *print;
	GtkPrintSettings        *settings;
	GtkPageSetup            *page_setup;
	PlannerPrintJob         *job;
	GError                  *error = NULL;
	GtkPrintOperationResult  res;
	GtkPrintOperationAction  print_action;

	window = PLANNER_WINDOW (data);
	priv = window->priv;

	/* Load printer settings */
	print = gtk_print_operation_new ();

	settings = planner_print_dialog_load_print_settings ();
	gtk_print_operation_set_print_settings (print, settings);
	g_object_unref (settings);
	settings = NULL;

	page_setup = planner_print_dialog_load_page_setup ();
	gtk_print_operation_set_default_page_setup (print, page_setup);
	g_object_unref (page_setup);
	page_setup = NULL;

	job = planner_print_job_new (print, priv->views);

	if (!strcmp (gtk_action_get_name (action), "FilePrint")) {
		print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	} else {
		print_action = GTK_PRINT_OPERATION_ACTION_PREVIEW;
	}

	res = gtk_print_operation_run (print, print_action, GTK_WINDOW (window), &error);

	if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 "%s", error->message);
		g_error_free (error);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_widget_show (dialog);
	}
	else if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		settings = gtk_print_operation_get_print_settings (print);
		planner_print_dialog_save_print_settings (settings);
	}
	g_object_unref (print);
	g_object_unref (job);
}

static void
window_close_cb (GtkAction *action,
		 gpointer   data)
{
	planner_window_close (PLANNER_WINDOW (data));
}

static void
window_exit_cb (GtkAction *action,
		gpointer   data)
{
	PlannerWindow     *window;
	PlannerWindowPriv *priv;

	window = PLANNER_WINDOW (data);
	priv   = window->priv;

	planner_application_exit (priv->application);
}

static void
window_redo_cb (GtkAction *action,
		gpointer           data)
{
	PlannerWindow     *window;
	PlannerWindowPriv *priv;

	window = PLANNER_WINDOW (data);

	priv = window->priv;

	planner_cmd_manager_redo (priv->cmd_manager);
}

static void
window_undo_cb (GtkAction *action,
		gpointer           data)
{
	PlannerWindow     *window;
	PlannerWindowPriv *priv;

	window = PLANNER_WINDOW (data);

	priv = window->priv;

	planner_cmd_manager_undo (priv->cmd_manager);
}

static void
window_project_props_cb (GtkAction *action,
			 gpointer           data)
{
	PlannerWindow     *window;
	PlannerWindowPriv *priv;

	window = PLANNER_WINDOW (data);
	priv = window->priv;

	if (priv->properties_dialog) {
		gtk_window_present (GTK_WINDOW (priv->properties_dialog));
	} else {
		priv->properties_dialog = planner_project_properties_new (window);

		g_object_add_weak_pointer (G_OBJECT (priv->properties_dialog),
					   (gpointer) &priv->properties_dialog);

		gtk_widget_show (priv->properties_dialog);
	}
}

static void
window_manage_calendars_cb (GtkAction *action,
			    gpointer           data)
{
	PlannerWindow *window;

	window = PLANNER_WINDOW (data);

	planner_window_show_calendar_dialog (window);
}

static void
window_edit_day_types_cb (GtkAction *action,
			  gpointer           data)
{
	PlannerWindow *window;

	window = PLANNER_WINDOW (data);

	planner_window_show_day_type_dialog (window);
}

static void
window_edit_phases_cb (GtkAction *action,
		       gpointer           data)
{
	PlannerWindow     *window;
	PlannerWindowPriv *priv;

	window = PLANNER_WINDOW (data);
	priv = window->priv;

	if (priv->phase_dialog) {
		gtk_window_present (GTK_WINDOW (priv->phase_dialog));
	} else {
		priv->phase_dialog = planner_phase_dialog_new (window);

		g_object_add_weak_pointer (G_OBJECT (priv->phase_dialog),
					   (gpointer) &priv->phase_dialog);

		gtk_widget_show (priv->phase_dialog);
	}
}

static void
window_help_cb (GtkAction *action,
		gpointer   data)
{
	planner_util_show_help (NULL);
}

static void
window_about_cb (GtkAction *action,
		 gpointer   data)
{
	PlannerWindow *window;
	gchar         *filename;
	GdkPixbuf     *pixbuf;
	const gchar   *authors[] = {
		"Kurt Maute <kurt@maute.us>",
		"Richard Hult <richard@imendio.com>",
		"Mikael Hallendal <micke@imendio.com>",
		"Alvaro del Castillo <acs@barrapunto.com>",
		NULL
	};
	const gchar   *documenters[] = {
		"Kurt Maute <kurt@maute.us>",
		"Alvaro del Castillo <acs@barrapunto.com>",
		"Pedro Soria-Rodriguez <sorrodp@alum.wpi.edu>",
		NULL
	};

	window = PLANNER_WINDOW (data);

	filename = mrp_paths_get_image_dir ("gnome-planner.png");
	pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	g_free (filename);

	gtk_show_about_dialog (GTK_WINDOW (window),
			       "version", VERSION,
			       "comments", _("A Project Management application for the GNOME desktop"),
			       "authors", authors,
			       "documenters", documenters,
			       "translator-credits", _("translator-credits"),
			       "website", "http://live.gnome.org/Planner",
			       "website-label", _("The Planner Homepage"),
			       "logo", pixbuf,
			       NULL);

	if (pixbuf != NULL) {
		g_object_unref (pixbuf);
	}
}

static gboolean
window_delete_event_cb (PlannerWindow *window,
			gpointer       user_data)
{
	planner_window_close (window);
	return TRUE;
}

static void
window_drag_data_received_cb (GtkWidget        *widget,
			      GdkDragContext   *context,
			      int               x,
			      int               y,
			      GtkSelectionData *data,
			      guint             info,
			      guint             time,
			      PlannerWindow    *window)
{
	gchar             **uris;
	gint                i;

	if (data->length < 0 || data->format != 8) {
		g_message ("Don't know how to handle format %d", data->format);
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	uris = g_uri_list_extract_uris (data->data);

	i = 0;
	while (uris[i]) {
		planner_window_open_in_existing_or_new (window, uris[i], FALSE);
		i++;
	}

	g_strfreev (uris);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
window_undo_state_changed_cb (PlannerCmdManager *manager,
			      gboolean           state,
			      const gchar       *str,
			      PlannerWindow     *window)
{
	PlannerWindowPriv *priv;
	GtkWidget         *widget;
	GtkWidget         *label;

	priv = window->priv;

	g_object_set (gtk_action_group_get_action (priv->actions, "EditUndo"),
		      "sensitive", state,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "EditUndo"),
		      "tooltip", str,
		      NULL);

	widget = gtk_ui_manager_get_widget (priv->ui_manager,
					    "/MenuBar/Edit/EditUndo");
	label = gtk_bin_get_child (GTK_BIN (widget));
	gtk_label_set_text (GTK_LABEL (label), str);
}

static void
window_redo_state_changed_cb (PlannerCmdManager *manager,
			      gboolean           state,
			      const gchar       *str,
			      PlannerWindow     *window)
{
	PlannerWindowPriv *priv;
	GtkWidget         *widget;
	GtkWidget         *label;

	priv = window->priv;

	g_object_set (gtk_action_group_get_action (priv->actions, "EditRedo"),
		      "sensitive", state,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "EditRedo"),
		      "tooltip", str,
		      NULL);

	widget = gtk_ui_manager_get_widget (priv->ui_manager,
					    "/MenuBar/Edit/EditRedo");
	label = gtk_bin_get_child (GTK_BIN (widget));
	gtk_label_set_text (GTK_LABEL (label), str);
}

static void
window_project_needs_saving_changed_cb (MrpProject   *project,
					gboolean      needs_saving,
					PlannerWindow *window)
{
	PlannerWindowPriv *priv;

	priv = window->priv;

	if (needs_saving) {
		g_timer_reset (priv->last_saved);
	}

	g_object_set (gtk_action_group_get_action (priv->actions, "FileSave"),
		      "sensitive", needs_saving,
		      NULL);
}

static void
window_project_notify_name_cb (MrpProject   *project,
			       GParamSpec   *pspec,
			       PlannerWindow *window)
{
	window_update_title (window);
}

static gboolean
window_confirm_exit_run (PlannerWindow *window)
{
	PlannerWindowPriv *priv;
	gchar            *time_str;
	GtkWidget        *dialog;
	GtkWidget        *quit_button, *cancel_button, *save_button;
	gint              ret;
	gint              minutes;
	gint              hours;
	gchar            *name;
	gchar            *tmp;
	const gchar      *uri;
	gboolean          is_sql = FALSE;

	priv = window->priv;

	minutes = (gint) (g_timer_elapsed (priv->last_saved, NULL) / 60);

	minutes = MAX (1, minutes);
	hours = floor (minutes / 60.0 + 0.5);
	minutes -= 60 * hours;

	if (hours == 0 && minutes == 1) {
		time_str = g_strdup (_("If you don't save, changes made "
				       "the last minute will be discarded."));
	}
	else if (hours == 0) {
		time_str =
			g_strdup_printf (ngettext(
					   "If you don't save, changes made "
					   "the last %d minute will be "
					   "discarded.",
					   "If you don't save, changes made "
					   "the last %d minutes will be "
					   "discarded.", minutes), minutes);
	}
	else if (hours == 1) {
		time_str = g_strdup (_("If you don't save, changes made "
				       "the last hour will be "
				       "discarded."));
	} else {
		time_str =
			g_strdup_printf (ngettext("If you don't save, changes made "
					   "the last %d hour will be "
					   "discarded.",
					   "If you don't save, changes made "
					   "the last %d hours will be "
					   "discarded.", hours), hours);
	}

	name = window_get_name (window);

	tmp = g_strdup_printf (_("Save changes to document '%s' before closing?"), name);

	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_MODAL |
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 "<b>%s</b>\n%s", tmp, time_str);
	g_free (name);
	g_free (time_str);
	g_free (tmp);

	g_object_set (GTK_MESSAGE_DIALOG (dialog)->label,
		      "use-markup", TRUE,
		      "wrap", FALSE,
		      "selectable", FALSE,
		      NULL);

	uri = mrp_project_get_uri (priv->project);
	if (uri && strncmp (uri, "sql://", 6) == 0) {
		/* Hack. */
		is_sql = TRUE;
	}

	quit_button = window_create_dialog_button (GTK_STOCK_QUIT,
						   _("C_lose without saving"));
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      quit_button,
				      GTK_RESPONSE_NO);

	cancel_button = window_create_dialog_button (GTK_STOCK_CANCEL,
						     _("_Cancel"));
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      cancel_button,
				      GTK_RESPONSE_CANCEL);

	if (!is_sql) {
		save_button = window_create_dialog_button (GTK_STOCK_SAVE,
							   _("_Save"));
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
					      save_button,
					      GTK_RESPONSE_YES);
		gtk_widget_grab_focus (save_button);
	}

	gtk_widget_show_all (dialog);
	ret = gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

	switch (ret) {
	case GTK_RESPONSE_NO: /* Don't save */
                return TRUE;
		break;
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CANCEL: /* Don't quit */
                return FALSE;
		break;
	case GTK_RESPONSE_YES: /* Save and quit */
		return window_do_save (window, FALSE);
		break;
	default:
		g_assert_not_reached ();
	};

        return FALSE;
}

static gboolean
window_do_save (PlannerWindow *window, gboolean force)
{
	PlannerWindowPriv *priv;
	const gchar      *uri = NULL;
	GError           *error = NULL;
	gint              response;

	priv = window->priv;

	uri = mrp_project_get_uri (priv->project);

	if (uri == NULL) {
		return window_do_save_as (window);
	} else {
		if (!mrp_project_save (priv->project, force, &error)) {
			GtkWidget *dialog;

			if (!force && error->code == MRP_ERROR_SAVE_FILE_CHANGED) {
				dialog = gtk_message_dialog_new (GTK_WINDOW (window),
								 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
								 GTK_MESSAGE_WARNING,
								 GTK_BUTTONS_YES_NO,
								 "%s",
								 error->message);

				response = gtk_dialog_run (GTK_DIALOG (dialog));
				gtk_widget_destroy (dialog);

				if (response == GTK_RESPONSE_YES) {
					return window_do_save (window, TRUE);
				}

				return FALSE;
			}

			dialog = gtk_message_dialog_new (GTK_WINDOW (window),
							 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							 "%s",
							 error->message);

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
window_do_save_as (PlannerWindow *window)
{
	PlannerWindowPriv *priv;
	GtkWidget        *file_chooser;
	gint              response;
	gchar            *filename = NULL;
	gchar            *last_dir;

	priv = window->priv;

	file_chooser = gtk_file_chooser_dialog_new (_("Save a File"),
						    GTK_WINDOW (window),
						    GTK_FILE_CHOOSER_ACTION_SAVE,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						    GTK_STOCK_SAVE, GTK_RESPONSE_OK,
						    NULL);
	gtk_window_set_modal (GTK_WINDOW (file_chooser), TRUE);

	gtk_dialog_set_default_response (GTK_DIALOG (file_chooser), GTK_RESPONSE_OK);

	last_dir = get_last_dir (window);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (file_chooser), last_dir);
	g_free (last_dir);

	response = gtk_dialog_run (GTK_DIALOG (file_chooser));
	if (response == GTK_RESPONSE_OK) {
		filename = gtk_file_chooser_get_filename (
			GTK_FILE_CHOOSER (file_chooser));
	}

	gtk_widget_destroy (file_chooser);

	if (filename != NULL) {
		gboolean  success;
		GError   *error = NULL;

		/* Save the file. */
		success = mrp_project_save_as (window->priv->project, filename,
					       FALSE, &error);

		if (!success && error->code == MRP_ERROR_SAVE_FILE_EXIST) {
			GtkWidget *dialog;
			gint       ret;

			dialog = gtk_message_dialog_new (GTK_WINDOW (window),
							 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_WARNING,
							 GTK_BUTTONS_YES_NO,
							 _("File \"%s\" exists, do you want to overwrite it?"),
							 error->message);

			gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);

			ret = gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			switch (ret) {
			case GTK_RESPONSE_YES:
				success = mrp_project_save_as (priv->project,
							       filename,
							       TRUE,
							       &error);
				break;
			case GTK_RESPONSE_NO:
			case GTK_RESPONSE_DELETE_EVENT:
				return window_do_save_as (window);
				break;
			default:
				g_assert_not_reached ();
			};
		}

		if (success) {
			/* Add the file to the recent list */
			window_recent_add_item (window, mrp_project_get_uri (priv->project));
		} else {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (GTK_WINDOW (window),
							 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_OK,
							 "%s",
							 error->message);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return FALSE;
		}

		last_dir = g_path_get_dirname (filename);
		planner_conf_set_string (CONF_LAST_DIR, last_dir, NULL);
		g_free (last_dir);

		g_free (filename);

		return TRUE;
	} else {
		return FALSE;
	}
}

static GtkWidget *
window_create_dialog_button (const gchar *icon_name, const gchar *text)
{
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *align;
	GtkWidget *hbox;

	button = gtk_button_new ();

	image = gtk_image_new_from_stock (icon_name, GTK_ICON_SIZE_BUTTON);
	label = gtk_label_new_with_mnemonic (text);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), GTK_WIDGET (button));

	hbox = gtk_hbox_new (FALSE, 2);

	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);

	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (button), align);
	gtk_container_add (GTK_CONTAINER (align), hbox);
	gtk_widget_show_all (align);

	return button;
}

GtkWidget *
planner_window_new (PlannerApplication *application)
{
	PlannerWindow     *window;
	PlannerWindowPriv *priv;

	window = g_object_new (PLANNER_TYPE_WINDOW, NULL);
	priv = window->priv;

	priv->application = g_object_ref (application);

	priv->project = mrp_project_new (MRP_APPLICATION (application));

	priv->last_saved = g_timer_new ();

	g_signal_connect (priv->project, "needs_saving_changed",
			  G_CALLBACK (window_project_needs_saving_changed_cb),
			  window);

	g_signal_connect (priv->project, "notify::name",
			  G_CALLBACK (window_project_notify_name_cb),
			  window);

	window_restore_state (window);

	g_signal_connect (window, "delete_event",
			  G_CALLBACK (window_delete_event_cb),
			  NULL);

	window_populate (window);
	window_update_title (window);

	return GTK_WIDGET (window);
}

gboolean
planner_window_open (PlannerWindow *window,
		     const gchar   *uri,
		     gboolean       internal)
{
	PlannerWindowPriv *priv;
	GError           *error = NULL;
	GtkWidget        *dialog;

	g_return_val_if_fail (PLANNER_IS_WINDOW (window), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	priv = window->priv;

	if (!mrp_project_load (priv->project, uri, &error)) {
		dialog = gtk_message_dialog_new (
			GTK_WINDOW (window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_ERROR,
			GTK_BUTTONS_OK,
			"%s",
			error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return FALSE;
	}

	planner_window_check_version (window);

	if (!internal) {
		/* Add the file to the recent list */
		window_recent_add_item (window, uri);
		window_update_title (window);
	}

	return TRUE;
}

gboolean
planner_window_open_in_existing_or_new (PlannerWindow *window,
					const gchar   *uri,
					gboolean       internal)
{
	PlannerWindowPriv *priv;
	GtkWidget         *new_window;
	gboolean           ret;

	priv = window->priv;
	if (mrp_project_is_empty (priv->project)) {
		ret = planner_window_open (window, uri, internal);
		return ret;
	} else {
		new_window = planner_application_new_window (priv->application);
		if (planner_window_open (PLANNER_WINDOW (new_window), uri, internal)) {
			gtk_widget_show_all (new_window);
			return TRUE;
		} else {
			g_signal_emit (new_window, signals[CLOSED], 0, NULL);
			gtk_widget_destroy (new_window);
			return FALSE;
		}
	}

	return FALSE;
}

GtkUIManager *
planner_window_get_ui_manager (PlannerWindow *window)
{
	g_return_val_if_fail (PLANNER_IS_WINDOW (window), NULL);

	return window->priv->ui_manager;
}

MrpProject *
planner_window_get_project (PlannerWindow *window)
{
	g_return_val_if_fail (PLANNER_IS_WINDOW (window), NULL);

	return window->priv->project;
}

PlannerApplication *
planner_window_get_application (PlannerWindow  *window)
{
	g_return_val_if_fail (PLANNER_IS_WINDOW (window), NULL);

	return window->priv->application;
}

void
planner_window_check_version (PlannerWindow *window)
{
	PlannerWindowPriv *priv;
	GtkWidget        *dialog;
	gint              version;

	g_return_if_fail (PLANNER_IS_WINDOW (window));

	priv = window->priv;

	version = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (priv->project),
						      "version"));

	if (version == 1) {
		dialog = gtk_message_dialog_new (
			GTK_WINDOW (window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_INFO,
			GTK_BUTTONS_OK,
			_("You have opened a file that was saved with an old "
			  "version of Planner.\n"
			  "\n"
			  "When loading old files, all tasks will use the "
			  "constraint 'Must Start On', since the old version "
			  "did not fully support automatic rescheduling. To "
			  "take advantage of this new feature, you should add "
			  "predecessor relations between tasks that are "
			  "dependent on each other.\n"
			  "\n"
			  "You can add a predecessor relation by clicking on "
			  "the predecessor and dragging to the successor.\n"
			  "\n"
			  "After doing this, you can remove all constraints by "
			  "selecting the menu item 'Remove all constraints' in "
			  "the 'Edit' menu."));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
}

/* Returns the project name, filename, or "Unnamed", in that order. */
static gchar *
window_get_name (PlannerWindow *window)
{
	PlannerWindowPriv *priv;
	gchar             *name, *tmp;
	const gchar       *uri;

	priv = window->priv;

	uri = mrp_project_get_uri (priv->project);

	if (uri && strncmp (uri, "sql://", 6) == 0) {
		/* Hack. */
		uri = _("Unnamed database project");
	}

	g_object_get (priv->project, "name", &name, NULL);
	if (name == NULL || name[0] == 0) {
		g_free (name);

		if (uri == NULL || uri[0] == 0) {
			name = g_strdup (_("Unnamed"));
		} else {
			tmp = g_path_get_basename (uri);
			name = g_markup_escape_text (tmp, -1);
			g_free (tmp);
		}
	}

	return name;
}

static void
window_update_title (PlannerWindow *window)
{
	gchar             *name;
	gchar             *title;

	name = window_get_name (window);

	title = g_strconcat (name, " - Planner", NULL);

	gtk_window_set_title (GTK_WINDOW (window), title);

	g_free (name);
	g_free (title);
}

void
planner_window_close (PlannerWindow *window)
{
	PlannerWindowPriv *priv;
        gboolean           close = TRUE;

	g_return_if_fail (PLANNER_IS_WINDOW (window));

	priv = window->priv;

	if (mrp_project_needs_saving (priv->project)) {
		close = window_confirm_exit_run (window);
	}

        if (close) {
		window_save_state (window);

                g_signal_emit (window, signals[CLOSED], 0, NULL);

                gtk_widget_destroy (GTK_WIDGET (window));
        }
}

void
planner_window_show_day_type_dialog (PlannerWindow *window)
{
	PlannerWindowPriv *priv;

	g_return_if_fail (PLANNER_IS_WINDOW (window));

	priv = window->priv;

	if (priv->day_type_dialog) {
		gtk_window_present (GTK_WINDOW (priv->day_type_dialog));
	} else {
		priv->day_type_dialog = planner_day_type_dialog_new (window);

		g_object_add_weak_pointer (G_OBJECT (priv->day_type_dialog),
					   (gpointer) &priv->day_type_dialog);

		gtk_widget_show (priv->day_type_dialog);
	}
}

void
planner_window_show_calendar_dialog (PlannerWindow *window)
{
	PlannerWindowPriv *priv;

	g_return_if_fail (PLANNER_IS_WINDOW (window));

	priv = window->priv;

	if (priv->calendar_dialog) {
		gtk_window_present (GTK_WINDOW (priv->calendar_dialog));
	} else {
		priv->calendar_dialog = planner_calendar_dialog_new (window);

		g_object_add_weak_pointer (G_OBJECT (priv->calendar_dialog),
					   (gpointer) &priv->calendar_dialog);

		gtk_widget_show (priv->calendar_dialog);
	}
}

static void
window_recent_add_item (PlannerWindow *window, const gchar *uri)
{
	gchar *file_uri;
	GtkRecentData *recent_data;
	static gchar *groups[2] = {
		"planner",
		NULL
	};

	g_return_if_fail (PLANNER_IS_WINDOW (window));
	if (uri == NULL)
		return;

	file_uri = g_filename_to_uri (uri, NULL, NULL);
	if (file_uri == NULL)
		return;

	recent_data = g_slice_new (GtkRecentData);
	recent_data->display_name = g_filename_display_basename (uri);
	recent_data->description = NULL;
	recent_data->mime_type = "application/x-planner";
	recent_data->app_name = (gchar *) g_get_application_name ();
	recent_data->app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
	recent_data->groups = groups;
	recent_data->is_private = FALSE;

	gtk_recent_manager_add_full (planner_application_get_recent_model (window->priv->application),
				     file_uri,
				     recent_data);

	g_free (recent_data->display_name);
	g_free (recent_data->app_exec);
	g_free (file_uri);

	g_slice_free (GtkRecentData, recent_data);
}

PlannerCmdManager *
planner_window_get_cmd_manager (PlannerWindow *window)
{
	g_return_val_if_fail (PLANNER_IS_WINDOW (window), NULL);

	return window->priv->cmd_manager;
}

static void
window_save_state (PlannerWindow *window)
{
	GdkWindowState     state;
	gboolean           maximized;

	state = gdk_window_get_state (GTK_WIDGET (window)->window);
	if (state & GDK_WINDOW_STATE_MAXIMIZED) {
		maximized = TRUE;
	} else {
		maximized = FALSE;
	}

	planner_conf_set_bool (CONF_WINDOW_MAXIMIZED, maximized, NULL);

	/* If maximized don't save the size and position */
	if (!maximized) {
		int width, height;
		int x, y;

		gtk_window_get_size (GTK_WINDOW (window), &width, &height);
		planner_conf_set_int (CONF_WINDOW_WIDTH, width, NULL);
		planner_conf_set_int (CONF_WINDOW_HEIGHT, height, NULL);

		gtk_window_get_position (GTK_WINDOW (window), &x, &y);
		planner_conf_set_int (CONF_WINDOW_POS_X, x, NULL);
		planner_conf_set_int (CONF_WINDOW_POS_Y, y, NULL);
	}
}

static void
window_restore_state (PlannerWindow *window)
{
	gboolean           exists;
	gboolean           maximized;
	int                width, height;
	int                x, y;

	exists = planner_conf_dir_exists (CONF_WINDOW_DIR, NULL);

	if (exists) {
		maximized = planner_conf_get_bool (CONF_WINDOW_MAXIMIZED,
						   NULL);

		if (maximized) {
			gtk_window_maximize (GTK_WINDOW (window));
		} else {
			width = planner_conf_get_int (CONF_WINDOW_WIDTH,
						      NULL);
			height = planner_conf_get_int (CONF_WINDOW_HEIGHT,
						       NULL);

			if (width == 0) {
				width = DEFAULT_WINDOW_WIDTH;
			}
			if (height == 0) {
				height = DEFAULT_WINDOW_HEIGHT;
			}

			gtk_window_set_default_size (GTK_WINDOW (window),
						     width, height);

			x = planner_conf_get_int (CONF_WINDOW_POS_X,
						  NULL);
			y = planner_conf_get_int (CONF_WINDOW_POS_Y,
						  NULL);

			gtk_window_move (GTK_WINDOW (window), x, y);
		}
	} else {
		gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
	}
}

void
planner_window_set_status (PlannerWindow *window, const gchar *message)
{
	PlannerWindowPriv *priv;

	priv = window->priv;

	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar), 0);
	gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar), 0,
			    message ? message : "");
}

static void
window_menu_item_select_cb (GtkMenuItem   *proxy,
			    PlannerWindow *window)
{
	PlannerWindowPriv *priv;
	GtkAction         *action;
	gchar             *message;

	priv = window->priv;

	action = g_object_get_data (G_OBJECT (proxy),  "gtk-action");
	g_return_if_fail (action != NULL);

	g_object_get (action, "tooltip", &message, NULL);
	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (priv->statusbar), 0, message);
		g_free (message);
	}
}

static void
window_menu_item_deselect_cb (GtkMenuItem   *proxy,
			      PlannerWindow *window)
{
	PlannerWindowPriv *priv;

	priv = window->priv;

	gtk_statusbar_pop (GTK_STATUSBAR (priv->statusbar), 0);
}

static void
window_disconnect_proxy_cb (GtkUIManager  *manager,
			    GtkAction     *action,
			    GtkWidget     *proxy,
			    PlannerWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_handlers_disconnect_by_func (
			proxy, G_CALLBACK (window_menu_item_select_cb), window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (window_menu_item_deselect_cb), window);
	}
}

static void
window_connect_proxy_cb (GtkUIManager  *manager,
			 GtkAction     *action,
			 GtkWidget     *proxy,
			 PlannerWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_connect (proxy, "select",
				  G_CALLBACK (window_menu_item_select_cb), window);
		g_signal_connect (proxy, "deselect",
				  G_CALLBACK (window_menu_item_deselect_cb), window);
	}
}
