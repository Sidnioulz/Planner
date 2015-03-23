/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio AB
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
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
#include <time.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libplanner/mrp-task.h>
#include "planner-task-view.h"
#include "libplanner/mrp-paths.h"
#include "planner-conf.h"
#include "planner-cell-renderer-date.h"
#include "planner-task-dialog.h"
#include "planner-property-dialog.h"
#include "planner-gantt-model.h"
#include "planner-task-tree.h"
#include "planner-table-print-sheet.h"
#include "planner-column-dialog.h"


struct _PlannerTaskViewPriv {
	GtkWidget              *tree;
	GtkWidget              *frame;
	PlannerTablePrintSheet *print_sheet;
	GtkUIManager           *ui_manager;
	GtkActionGroup         *actions;
	guint                   merged_id;
};

static void          task_view_finalize                     (GObject         *object);
static void          task_view_activate                     (PlannerView     *view);
static void          task_view_deactivate                   (PlannerView     *view);
static void          task_view_setup                        (PlannerView     *view,
							     PlannerWindow   *main_window);
static const gchar  *task_view_get_label                    (PlannerView     *view);
static const gchar  *task_view_get_menu_label               (PlannerView     *view);
static const gchar  *task_view_get_icon                     (PlannerView     *view);
static const gchar  *task_view_get_name                     (PlannerView     *view);
static GtkWidget    *task_view_get_widget                   (PlannerView     *view);
static void          task_view_print_init                   (PlannerView     *view,
							     PlannerPrintJob *job);
static void          task_view_print                        (PlannerView     *view,
							     gint             page_nr);
static gint          task_view_print_get_n_pages            (PlannerView     *view);
static void          task_view_print_cleanup                (PlannerView     *view);
static void          task_view_tree_view_columns_changed_cb (GtkTreeView     *tree_view,
							     PlannerView     *view);
static void          task_view_tree_view_destroy_cb         (GtkTreeView     *tree_view,
							     PlannerView     *view);
static void          task_view_project_loaded_cb            (MrpProject      *project,
							     PlannerView     *view);
static void          task_view_insert_task_cb               (GtkAction       *action,
							     gpointer         data);
static void          task_view_insert_tasks_cb              (GtkAction       *action,
							     gpointer         data);
static void          task_view_remove_task_cb               (GtkAction       *action,
							     gpointer         data);
static void          task_view_edit_task_cb                 (GtkAction       *action,
							     gpointer         data);
static void          task_view_select_all_cb                (GtkAction       *action,
							     gpointer         data);
static void          task_view_unlink_task_cb               (GtkAction       *action,
							     gpointer         data);
static void          task_view_link_tasks_cb                (GtkAction       *action,
							     gpointer         data);
static void          task_view_indent_task_cb               (GtkAction       *action,
							     gpointer         data);
static void          task_view_move_task_up_cb              (GtkAction       *action,
							     gpointer         data);
static void          task_view_move_task_down_cb            (GtkAction       *action,
							     gpointer         data);
static void          task_view_unindent_task_cb             (GtkAction       *action,
							     gpointer         data);
static void          task_view_reset_constraint_cb          (GtkAction       *action,
							     gpointer         data);
static void          task_view_edit_custom_props_cb         (GtkAction       *action,
							     gpointer         data);
static void          task_view_highlight_critical_cb        (GtkAction       *action,
							     gpointer         data);
static void          task_view_nonstandard_days_cb          (GtkAction       *action,
							     gpointer         data);
static void          task_view_edit_columns_cb              (GtkAction       *action,
							     gpointer         data);
static void          task_view_selection_changed_cb         (PlannerTaskTree *tree,
							     PlannerView     *view);
static void          task_view_relations_changed_cb         (PlannerTaskTree *tree,
							     MrpTask         *task,
							     MrpRelation     *relation,
							     PlannerView     *view);
static void          task_view_update_ui                    (PlannerView     *view);
static void          task_view_save_columns                 (PlannerView     *view);
static void          task_view_load_columns                 (PlannerView     *view);


static PlannerViewClass *parent_class = NULL;

static const GtkActionEntry entries[] = {
	{ "InsertTask",      "planner-stock-insert-task",      N_("_Insert Task"),
	  "<Control>i",        N_("Insert a new task"),
          G_CALLBACK (task_view_insert_task_cb) },
	{ "InsertTasks",     "planner-stock-insert-task",      N_("In_sert Tasks..."),
         NULL,                NULL,
         G_CALLBACK (task_view_insert_tasks_cb) },
	{ "RemoveTask",      "planner-stock-remove-task",      N_("_Remove Task"),
	  "Delete",           N_("Remove the selected tasks"),
	  G_CALLBACK (task_view_remove_task_cb) },
	{ "EditTask",        NULL,                             N_("_Edit Task"),
	  "<Control>e",       NULL,
	  G_CALLBACK (task_view_edit_task_cb) },
	{ "SelectAll",       NULL,                             N_("Select _All"),
	  NULL,               N_("Select all tasks"),
	  G_CALLBACK (task_view_select_all_cb) },
	{ "UnlinkTask",      "planner-stock-unlink-task",      N_("_Unlink Task"),
	  "<Control>u",        N_("Unlink the selected tasks"),
	  G_CALLBACK (task_view_unlink_task_cb) },
	{ "LinkTasks",       "planner-stock-link-task",        N_("_Link Tasks"),
	  "<Control>l",         N_("Link the selected tasks"),
	  G_CALLBACK (task_view_link_tasks_cb) },
	{ "IndentTask",      "planner-stock-indent-task",      N_("I_ndent Task"),
	  "<Shift><Control>i", N_("Indent the selected tasks"),
	  G_CALLBACK (task_view_indent_task_cb) },
	{ "UnindentTask",    "planner-stock-unindent-task",    N_("Unin_dent Task"),
	  "<Shift><Control>u", N_("Unindent the selected tasks"),
	  G_CALLBACK (task_view_unindent_task_cb) },
	{ "MoveTaskUp",      "planner-stock-move-task-up",     N_("Move Task _Up"),
	  "<Control>Up",       N_("Move the selected tasks upwards"),
	  G_CALLBACK (task_view_move_task_up_cb) },
	{ "MoveTaskDown",    "planner-stock-move-task-down",   N_("Move Task Do_wn"),
	  "<Control>Down",     N_("Move the selected tasks downwards"),
	  G_CALLBACK (task_view_move_task_down_cb) },
	{ "ResetConstraint", "planner-stock-reset-constraint", N_("Reset _Constraint"),
          NULL,                NULL,
	  G_CALLBACK (task_view_reset_constraint_cb) },
	{ "EditCustomProps", GTK_STOCK_PROPERTIES,             N_("_Edit Custom Properties..."),
	  NULL,                NULL,
	  G_CALLBACK (task_view_edit_custom_props_cb) },
	{ "EditColumns",       NULL,                           N_("Edit _Visible Columns"),
	  NULL,                N_("Edit visible columns"),
	  G_CALLBACK (task_view_edit_columns_cb) }
};

static const GtkToggleActionEntry toggle_entries[] = {
	{ "HighlightCriticalTasks", NULL, N_("_Highlight Critical Tasks"), "<Control>h", NULL,
	  G_CALLBACK (task_view_highlight_critical_cb), FALSE },
	{ "NonstandardDays", NULL, N_("_Nonstandard Days"), NULL, NULL,
	  G_CALLBACK (task_view_nonstandard_days_cb), FALSE }
};

#define CRITICAL_PATH_KEY  "/views/task_view/highlight_critical_path"
#define NOSTDDAYS_PATH_KEY "/views/task_view/display_nonstandard_days"

G_DEFINE_TYPE (PlannerTaskView, planner_task_view, PLANNER_TYPE_VIEW);


static void
planner_task_view_class_init (PlannerTaskViewClass *klass)
{
	GObjectClass     *o_class;
	PlannerViewClass *view_class;

	parent_class = g_type_class_peek_parent (klass);

	o_class = (GObjectClass *) klass;
	view_class = PLANNER_VIEW_CLASS (klass);

	o_class->finalize = task_view_finalize;

	view_class->setup = task_view_setup;
	view_class->get_label = task_view_get_label;
	view_class->get_menu_label = task_view_get_menu_label;
	view_class->get_icon = task_view_get_icon;
	view_class->get_name = task_view_get_name;
	view_class->get_widget = task_view_get_widget;
	view_class->activate = task_view_activate;
	view_class->deactivate = task_view_deactivate;
	view_class->print_init = task_view_print_init;
	view_class->print_get_n_pages = task_view_print_get_n_pages;
	view_class->print = task_view_print;
	view_class->print_cleanup = task_view_print_cleanup;
}

static void
planner_task_view_init (PlannerTaskView *view)
{
	view->priv = g_new0 (PlannerTaskViewPriv, 1);
}

static void
task_view_finalize (GObject *object)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (object);

	if (PLANNER_VIEW (view)->activated) {
		task_view_deactivate (PLANNER_VIEW (view));
	}

	g_free (view->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize) {
		(*G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
task_view_activate (PlannerView *view)
{
	PlannerTaskViewPriv *priv;
	gboolean             show_critical;
	gboolean             show_nostd_days;
	gchar               *filename;

	priv = PLANNER_TASK_VIEW (view)->priv;

	priv->actions = gtk_action_group_new ("TaskView");
	gtk_action_group_set_translation_domain (priv->actions, GETTEXT_PACKAGE);

	gtk_action_group_add_actions (priv->actions, entries,
				      G_N_ELEMENTS (entries),
				      view);
	gtk_action_group_add_toggle_actions (priv->actions, toggle_entries,
					     G_N_ELEMENTS (toggle_entries),
					     view);

	gtk_ui_manager_insert_action_group (priv->ui_manager, priv->actions, 0);
	filename = mrp_paths_get_ui_dir ("task-view.ui");
	priv->merged_id = gtk_ui_manager_add_ui_from_file (priv->ui_manager,
							   filename,
							   NULL);
	g_free (filename);
	gtk_ui_manager_ensure_update (priv->ui_manager);

	/* Set the initial UI state. */
	show_critical =   planner_conf_get_bool (CRITICAL_PATH_KEY, NULL);
	show_nostd_days = planner_conf_get_bool (NOSTDDAYS_PATH_KEY, NULL);
	planner_task_tree_set_highlight_critical (PLANNER_TASK_TREE (priv->tree),
						  show_critical);
	planner_task_tree_set_nonstandard_days (PLANNER_TASK_TREE (priv->tree),
						show_nostd_days);

	gtk_toggle_action_set_active (
		GTK_TOGGLE_ACTION (gtk_action_group_get_action (priv->actions, "HighlightCriticalTasks")),
		show_critical);

	gtk_toggle_action_set_active (
		GTK_TOGGLE_ACTION (gtk_action_group_get_action (priv->actions, "NonstandardDays")),
		show_nostd_days);

	task_view_selection_changed_cb (PLANNER_TASK_TREE (priv->tree), view);

	gtk_widget_grab_focus (priv->tree);
}

static void
task_view_deactivate (PlannerView *view)
{
	PlannerTaskViewPriv *priv;

	priv = PLANNER_TASK_VIEW (view)->priv;
	gtk_ui_manager_remove_ui (priv->ui_manager, priv->merged_id);
	gtk_ui_manager_remove_action_group (priv->ui_manager, priv->actions);
	g_object_unref (priv->actions);
	priv->actions = NULL;
}

static void
task_view_setup (PlannerView *view, PlannerWindow *main_window)
{
	PlannerTaskViewPriv *priv;

	priv = PLANNER_TASK_VIEW (view)->priv;

	priv->ui_manager = planner_window_get_ui_manager (main_window);
}

static const gchar *
task_view_get_label (PlannerView *view)
{
	return _("Tasks");
}

static const gchar *
task_view_get_menu_label (PlannerView *view)
{
	return _("_Tasks");
}

static const gchar *
task_view_get_icon (PlannerView *view)
{
	static gchar *filename = NULL;

	if (!filename) {
		filename = mrp_paths_get_image_dir ("tasks.png");
	}

	return filename;
}

static const gchar *
task_view_get_name (PlannerView *view)
{
	return "task_view";
}

static GtkWidget *
task_view_get_widget (PlannerView *view)
{
	PlannerTaskViewPriv *priv;
	MrpProject          *project;
	GtkWidget           *sw;
	PlannerGanttModel   *model;

	priv = PLANNER_TASK_VIEW (view)->priv;

	if (priv->tree == NULL) {
		project = planner_window_get_project (view->main_window);

		g_signal_connect (project,
				  "loaded",
				  G_CALLBACK (task_view_project_loaded_cb),
				  view);

		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);

		priv->frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (priv->frame), GTK_SHADOW_IN);

		gtk_container_add (GTK_CONTAINER (priv->frame), sw);

		model = planner_gantt_model_new (project);

		priv->tree = planner_task_tree_new (view->main_window,
						    model,
						    TRUE,
						    FALSE,
						    /* i18n: WBS is sort for work breakdown structure, and is a
						     * project management term. You might want to leave it
						     * untranslated unless there is a localized term for it.
						     */
						    COL_WBS, _("WBS"),
						    COL_NAME, _("Name"),
						    COL_START, _("Start"),
						    COL_FINISH, _("Finish"),
						    COL_WORK, _("Work"),
						    COL_DURATION, _("Duration"),
						    COL_SLACK, _("Slack"),
						    COL_COST, _("Cost"),
						    COL_ASSIGNED_TO, _("Assigned to"),
						    /* i18n: The string "% Complete" will be used in the header
						     * of a column containing values from 0 upto 100, indicating
						     * what part of a task has been completed.
						     * xgettext:no-c-format
						     */
						    COL_COMPLETE, _("% Complete"),
						    -1);

		g_object_unref (model);

		task_view_load_columns (view);

		gtk_container_add (GTK_CONTAINER (sw), priv->tree);

		g_signal_connect (priv->tree,
				  "columns-changed",
				  G_CALLBACK (task_view_tree_view_columns_changed_cb),
				  view);

		g_signal_connect (priv->tree,
				  "destroy",
				  G_CALLBACK (task_view_tree_view_destroy_cb),
				  view);

		g_signal_connect (priv->tree,
				  "selection-changed",
				  G_CALLBACK (task_view_selection_changed_cb),
				  view);

		g_signal_connect (priv->tree,
				  "relation-added",
				  G_CALLBACK (task_view_relations_changed_cb),
				  view);

		g_signal_connect (priv->tree,
				  "relation-removed",
				  G_CALLBACK (task_view_relations_changed_cb),
				  view);

		gtk_widget_show (priv->tree);
		gtk_widget_show (sw);
		gtk_widget_show (priv->frame);
	}

	return priv->frame;
}

static void
task_view_tree_view_columns_changed_cb (GtkTreeView *tree_view,
					PlannerView *view)
{
	task_view_save_columns (view);
}

static void
task_view_tree_view_destroy_cb (GtkTreeView *tree_view,
				PlannerView *view)
{
	/* Block, we don't want to save the column configuration when they are
	 * removed by the destruction.
	 */
	g_signal_handlers_block_by_func (tree_view,
					 task_view_tree_view_columns_changed_cb,
					 view);
}

static void
task_view_project_loaded_cb (MrpProject  *project,
			     PlannerView *view)
{
	PlannerTaskViewPriv *priv;
	GtkTreeModel        *model;

	priv = PLANNER_TASK_VIEW (view)->priv;

	model = GTK_TREE_MODEL (planner_gantt_model_new (project));

	planner_task_tree_set_model (PLANNER_TASK_TREE (priv->tree),
				     PLANNER_GANTT_MODEL (model));

	g_object_unref (model);
}

/* Command callbacks. */

static void
task_view_insert_task_cb (GtkAction *action,
			  gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_insert_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_insert_tasks_cb (GtkAction *action,
			   gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_insert_tasks (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_remove_task_cb (GtkAction *action,
			  gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_remove_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_edit_task_cb (GtkAction *action,
			gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_edit_task (PLANNER_TASK_TREE (view->priv->tree),
				     PLANNER_TASK_DIALOG_PAGE_GENERAL);
}

static void
task_view_select_all_cb (GtkAction *action,
			 gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_select_all (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_unlink_task_cb (GtkAction *action,
			  gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_unlink_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_link_tasks_cb (GtkAction *action,
			 gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_link_tasks (PLANNER_TASK_TREE (view->priv->tree),
				      MRP_RELATION_FS);
}

static void
task_view_indent_task_cb (GtkAction *action,
			  gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_indent_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_move_task_up_cb (GtkAction *action,
			   gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_move_task_up (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_move_task_down_cb (GtkAction *action,
			     gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_move_task_down (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_unindent_task_cb (GtkAction *action,
			    gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_unindent_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_reset_constraint_cb (GtkAction *action,
			       gpointer   data)
{
	PlannerTaskView *view;

	view = PLANNER_TASK_VIEW (data);

	planner_task_tree_reset_constraint (PLANNER_TASK_TREE (view->priv->tree));
}

static void
task_view_edit_custom_props_cb (GtkAction *action,
				gpointer   data)
{
	PlannerTaskView *view;
	GtkWidget       *dialog;
	MrpProject      *project;

	view = PLANNER_TASK_VIEW (data);

	project = planner_window_get_project (PLANNER_VIEW (view)->main_window);

	dialog = planner_property_dialog_new (PLANNER_VIEW (view)->main_window,
					      project,
					      MRP_TYPE_TASK,
					      _("Edit custom task properties"));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 300);
	gtk_widget_show (dialog);
}

static void
task_view_highlight_critical_cb (GtkAction *action,
				 gpointer   data)
{
	PlannerTaskViewPriv *priv;
	gboolean             state;

	priv = PLANNER_TASK_VIEW (data)->priv;

	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	planner_task_tree_set_highlight_critical (
		PLANNER_TASK_TREE (priv->tree),
		state);

	planner_conf_set_bool (CRITICAL_PATH_KEY, state, NULL);
}

static void
task_view_nonstandard_days_cb (GtkAction *action,
				 gpointer   data)
{
	PlannerTaskViewPriv *priv;
	gboolean             state;

	priv = PLANNER_TASK_VIEW (data)->priv;

	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	planner_task_tree_set_nonstandard_days (
		PLANNER_TASK_TREE (priv->tree),
		state);


	planner_conf_set_bool (NOSTDDAYS_PATH_KEY, state, NULL);
}

static void
task_view_edit_columns_cb (GtkAction *action,
			   gpointer   data)
{
	PlannerTaskView     *view;
	PlannerTaskViewPriv *priv;

	view = PLANNER_TASK_VIEW (data);
	priv = view->priv;

	planner_column_dialog_show (PLANNER_VIEW (view)->main_window,
				    _("Edit Task Columns"),
				    GTK_TREE_VIEW (priv->tree));
}

static void
task_view_selection_changed_cb (PlannerTaskTree *tree, PlannerView *view)
{
	task_view_update_ui (view);
}

static void
task_view_relations_changed_cb (PlannerTaskTree  *tree,
				MrpTask     *task,
				MrpRelation *relation,
				PlannerView      *view)
{
	task_view_update_ui (view);
}


static void
task_view_print_init (PlannerView     *view,
		      PlannerPrintJob *job)
{
	PlannerTaskViewPriv *priv;

	priv = PLANNER_TASK_VIEW (view)->priv;

	priv->print_sheet = planner_table_print_sheet_new (PLANNER_VIEW (view), job,
							   GTK_TREE_VIEW (priv->tree));
}

static void
task_view_print (PlannerView *view,
		 gint         page_nr)
{
	PlannerTaskViewPriv *priv;

	priv = PLANNER_TASK_VIEW (view)->priv;

	planner_table_print_sheet_output (priv->print_sheet, page_nr);
}

static gint
task_view_print_get_n_pages (PlannerView *view)
{
	PlannerTaskViewPriv *priv;

	priv = PLANNER_TASK_VIEW (view)->priv;

	return planner_table_print_sheet_get_n_pages (priv->print_sheet);
}

static void
task_view_print_cleanup (PlannerView *view)
{
	PlannerTaskViewPriv *priv;

	priv = PLANNER_TASK_VIEW (view)->priv;

	planner_table_print_sheet_free (priv->print_sheet);
	priv->print_sheet = NULL;
}

static void
task_view_update_ui (PlannerView *view)
{
	PlannerTaskViewPriv *priv;
	GList           *list, *l;
	gboolean         value;
	gboolean         rel_value  = FALSE;
	gboolean         link_value = FALSE;
	gint	         count = 0;

	if (!view->activated) {
		return;
	}

	priv = PLANNER_TASK_VIEW (view)->priv;

	list = planner_task_tree_get_selected_tasks (PLANNER_TASK_TREE (priv->tree));

	for (l = list; l; l = l->next) {
		if (mrp_task_has_relation (MRP_TASK (l->data))) {
			rel_value = TRUE;
			break;
		}
	}

	for (l = list; l; l = l->next) {
		count++;
	}

	value = (list != NULL);
	link_value = (count >= 2);

	g_object_set (gtk_action_group_get_action (priv->actions, "EditTask"),
		      "sensitive", value,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "RemoveTask"),
		      "sensitive", value,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "UnlinkTask"),
		      "sensitive", rel_value,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "LinkTasks"),
		      "sensitive", link_value,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "IndentTask"),
		      "sensitive", value,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "UnindentTask"),
		      "sensitive", value,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "MoveTaskUp"),
		      "sensitive", value,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "MoveTaskDown"),
		      "sensitive", value,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "ResetConstraint"),
		      "sensitive", value,
		      NULL);

	g_list_free (list);
}

static void
task_view_save_columns (PlannerView *view)
{
	PlannerTaskViewPriv *priv;

	priv = PLANNER_TASK_VIEW (view)->priv;

	planner_view_column_save_helper (view, GTK_TREE_VIEW (priv->tree));
}

static void
task_view_load_columns (PlannerView *view)
{
	PlannerTaskViewPriv *priv;
	GList               *columns, *l;
	GtkTreeViewColumn   *column;
	const gchar         *id;
	gint                 i;

	priv = PLANNER_TASK_VIEW (view)->priv;

	/* Load the columns. */
	planner_view_column_load_helper (view, GTK_TREE_VIEW (priv->tree));

	/* Make things a bit more robust by setting defaults if we don't get any
	 * visible columns. Should be done through a schema instead (but we'll
	 * keep this since a lot of people get bad installations when installing
	 * themselves).
	 */
	columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (priv->tree));
	i = 0;
	for (l = columns; l; l = l->next) {
		if (gtk_tree_view_column_get_visible (l->data)) {
			i++;
		}
	}

	if (i == 0) {
		for (l = columns; l; l = l->next) {
			column = l->data;

			if (g_object_get_data (G_OBJECT (column), "custom")) {
				continue;
			}

			id = g_object_get_data (G_OBJECT (column), "id");

			g_print ("%s\n", id);

			if (!id) {
				continue;
			}


			if (strcmp (id, "wbs") == 0 ||
			    strcmp (id, "name") == 0 ||
			    strcmp (id, "start") == 0 ||
			    strcmp (id, "finish") == 0 ||
			    strcmp (id, "work") == 0 ||
			    strcmp (id, "duration") == 0 ||
			    strcmp (id, "duration") == 0 ||
			    strcmp (id, "slack") == 0 ||
			    strcmp (id, "cost") == 0 ||
			    strcmp (id, "complete") == 0 ||
			    strcmp (id, "assigned-to")) {
				gtk_tree_view_column_set_visible (column, TRUE);
			} else {
				gtk_tree_view_column_set_visible (column, FALSE);
			}
		}
	}

	g_list_free (columns);
}

PlannerView *
planner_task_view_new (void)
{
	PlannerView *view;

	view = g_object_new (PLANNER_TYPE_TASK_VIEW, NULL);

	return view;
}
