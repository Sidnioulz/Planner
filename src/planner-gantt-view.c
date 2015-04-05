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
#include <math.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libplanner/mrp-task.h>
#include "libplanner/mrp-paths.h"
#include "planner-gantt-view.h"
#include "planner-cell-renderer-date.h"
#include "planner-task-dialog.h"
#include "planner-resource-dialog.h"
#include "planner-gantt-model.h"
#include "planner-task-tree.h"
#include "planner-gantt-chart.h"
#include "planner-gantt-print.h"
#include "planner-column-dialog.h"
#include "planner-conf.h"

struct _PlannerGanttViewPriv {
	GtkWidget             *paned;
	GtkWidget             *tree;
	GtkWidget             *gantt;
	PlannerGanttPrintData *print_data;

	GtkUIManager          *ui_manager;
	GtkActionGroup        *actions;
	guint                  merged_id;
	gulong                 expose_id;
};

static void          gantt_view_finalize                  (GObject           *object);
static GtkWidget *   gantt_view_create_widget             (PlannerGanttView  *view);
static void          gantt_view_insert_task_cb            (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_insert_tasks_cb           (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_remove_task_cb            (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_edit_task_cb              (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_select_all_cb             (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_unlink_task_cb            (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_link_tasks_cb             (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_indent_task_cb            (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_unindent_task_cb          (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_move_task_up_cb           (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_move_task_down_cb         (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_reset_constraint_cb       (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_zoom_to_fit_cb            (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_zoom_in_cb                (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_zoom_out_cb               (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_highlight_critical_cb     (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_show_guidelines_cb	  (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_nonstandard_days_cb       (GtkAction         *action,
							   gpointer           data);
static void          gantt_view_edit_columns_cb           (GtkAction         *action,
							   gpointer           data);
static gboolean      gantt_view_expose_cb                 (GtkWidget         *widget,
							   GdkEventExpose    *event,
							   gpointer           user_data);
static void          gantt_view_update_row_height         (PlannerGanttView  *view);
static void          gantt_view_tree_style_set_cb         (GtkWidget         *tree,
							   GtkStyle          *prev_style,
							   PlannerGanttView  *view);
static void          gantt_view_selection_changed_cb      (PlannerTaskTree   *tree,
							   PlannerGanttView  *view);
static void          gantt_view_update_zoom_sensitivity   (PlannerGanttView  *view);
static void          gantt_view_gantt_status_updated_cb   (PlannerGanttChart *gantt,
							   const gchar       *message,
							   PlannerGanttView  *view);
static void          gantt_view_relations_changed_cb      (PlannerTaskTree   *tree,
							   MrpTask           *task,
							   MrpRelation       *relation,
							   PlannerGanttView  *view);
static void          gantt_view_gantt_resource_clicked_cb (PlannerGanttChart *chart,
							   MrpResource       *resource,
							   PlannerGanttView  *view);
static void          gantt_view_update_ui                 (PlannerGanttView  *view);
static void          gantt_view_save_columns              (PlannerGanttView  *view);
static void          gantt_view_load_columns              (PlannerGanttView  *view);
static void          gantt_view_activate                  (PlannerView       *view);
static void          gantt_view_deactivate                (PlannerView       *view);
static void          gantt_view_setup                     (PlannerView       *view,
							   PlannerWindow     *main_window);
static const gchar  *gantt_view_get_label                 (PlannerView       *view);
static const gchar  *gantt_view_get_menu_label            (PlannerView       *view);
static const gchar  *gantt_view_get_icon                  (PlannerView       *view);
static const gchar  *gantt_view_get_name                  (PlannerView       *view);
static GtkWidget    *gantt_view_get_widget                (PlannerView       *view);
static void          gantt_view_print_init                (PlannerView       *view,
							   PlannerPrintJob   *job);
static void          gantt_view_print                     (PlannerView       *view,
							   gint               page_nr);
static gint          gantt_view_print_get_n_pages         (PlannerView       *view);
static void          gantt_view_print_cleanup             (PlannerView       *view);


static PlannerViewClass *parent_class = NULL;

static const GtkActionEntry entries[] = {
	{ "InsertTask",      "planner-stock-insert-task",    N_("_Insert Task"),
	  "<control>n",        N_("Insert a new task"),
	  G_CALLBACK (gantt_view_insert_task_cb) },
	{ "InsertTasks",     "planner-stock-insert-task",    N_("In_sert Tasks..."),
	  NULL,                NULL,
	  G_CALLBACK (gantt_view_insert_tasks_cb) },
	{ "RemoveTask",      "planner-stock-remove-task",    N_("_Remove Task"),
	  "Delete",           N_("Remove the selected tasks"),
	  G_CALLBACK (gantt_view_remove_task_cb) },
	{ "EditTask",        NULL,                           N_("_Edit Task Properties..."),
	  "<control>e",       NULL,
	  G_CALLBACK (gantt_view_edit_task_cb) },
	{ "SelectAll",       NULL,                           N_("Select _All"),
	  NULL,               N_("Select all tasks"),
	  G_CALLBACK (gantt_view_select_all_cb) },
	{ "UnlinkTask",      "planner-stock-unlink-task",    N_("_Unlink Task"),
	  "<shift><control>l",        N_("Unlink the selected tasks"),
	  G_CALLBACK (gantt_view_unlink_task_cb) },
	{ "LinkTasks",       "planner-stock-link-task",      N_("_Link Tasks"),
	  "<control>l",        N_("Link the selected tasks"),
	  G_CALLBACK (gantt_view_link_tasks_cb) },
	{ "IndentTask",      "planner-stock-indent-task",    N_("I_ndent Task"),
	  "<control>i", N_("Indent the selected tasks"),
	  G_CALLBACK (gantt_view_indent_task_cb) },
	{ "UnindentTask",    "planner-stock-unindent-task",  N_("Unin_dent Task"),
	  "<shift><control>i", N_("Unindent the selected tasks"),
	  G_CALLBACK (gantt_view_unindent_task_cb) },
	{ "MoveTaskUp",      "planner-stock-move-task-up",   N_("Move Task _Up"),
	  "<control>Up",       N_("Move the selected tasks upwards"),
	  G_CALLBACK (gantt_view_move_task_up_cb) },
	{ "MoveTaskDown",    "planner-stock-move-task-down", N_("Move Task Do_wn"),
	  "<control>Down",     N_("Move the selected tasks downwards"),
	  G_CALLBACK (gantt_view_move_task_down_cb) },
	{ "ResetConstraint", NULL,                           N_("Reset _Constraint"),
	  NULL,                NULL,
	  G_CALLBACK (gantt_view_reset_constraint_cb) },
	{ "ZoomToFit",       GTK_STOCK_ZOOM_FIT,             N_("Zoom To _Fit"),
	  NULL,                N_("Zoom to fit the entire project"),
	  G_CALLBACK (gantt_view_zoom_to_fit_cb) },
	{ "ZoomIn",          GTK_STOCK_ZOOM_IN,              N_("_Zoom In"),
	  "<control>plus",     N_("Zoom in"),
	  G_CALLBACK (gantt_view_zoom_in_cb) },
	{ "ZoomOut",         GTK_STOCK_ZOOM_OUT,             N_("Zoom _Out"),
	  "<control>minus",    N_("Zoom out"),
	  G_CALLBACK (gantt_view_zoom_out_cb) },
	{ "EditColumns",     NULL,                           N_("Edit _Visible Columns"),
	  NULL,                N_("Edit visible columns"),
	  G_CALLBACK (gantt_view_edit_columns_cb) }
};

static const GtkToggleActionEntry toggle_entries[] = {
	{ "HighlightCriticalTasks", NULL, N_("_Highlight Critical Tasks"),
	  NULL, NULL,
	  G_CALLBACK (gantt_view_highlight_critical_cb), FALSE },
	{ "ShowGuideLines", NULL, N_("_Show Guide Lines"),
	  NULL, NULL,
	  G_CALLBACK (gantt_view_show_guidelines_cb), FALSE },
	{ "NonstandardDays", NULL, N_("_Nonstandard Days"),
	  NULL, NULL,
	  G_CALLBACK (gantt_view_nonstandard_days_cb), FALSE }
};



G_DEFINE_TYPE (PlannerGanttView, planner_gantt_view, PLANNER_TYPE_VIEW);


static gboolean
gantt_view_chart_scroll_event (GtkWidget * gki, GdkEventScroll * event, PlannerGanttView *view)
{
	gboolean dontpropagate = FALSE;
	gboolean can_in, can_out;
	PlannerGanttViewPriv *priv;

	if (event->state & GDK_CONTROL_MASK) {
		priv = view->priv;
		planner_gantt_chart_can_zoom (PLANNER_GANTT_CHART (priv->gantt), &can_in, &can_out);
		switch (event->direction) {
			case GDK_SCROLL_UP: {
				dontpropagate = TRUE;
				if (can_in)
					gantt_view_zoom_in_cb  (NULL, view);
				break;
			}
			case GDK_SCROLL_DOWN:
				dontpropagate = TRUE;
				if (can_out)
					gantt_view_zoom_out_cb  (NULL, view);
			        break;
			default:
				break;
		}
	}

	return dontpropagate;
}

static void
planner_gantt_view_class_init (PlannerGanttViewClass *klass)
{
	GObjectClass     *o_class;
	PlannerViewClass *view_class;

	parent_class = g_type_class_peek_parent (klass);

	o_class = (GObjectClass *) klass;
	view_class = PLANNER_VIEW_CLASS (klass);

	o_class->finalize = gantt_view_finalize;

	view_class->setup = gantt_view_setup;
	view_class->get_label = gantt_view_get_label;
	view_class->get_menu_label = gantt_view_get_menu_label;
	view_class->get_icon = gantt_view_get_icon;
	view_class->get_name = gantt_view_get_name;
	view_class->get_widget = gantt_view_get_widget;
	view_class->activate = gantt_view_activate;
	view_class->deactivate = gantt_view_deactivate;
	view_class->print_init = gantt_view_print_init;
	view_class->print_get_n_pages = gantt_view_print_get_n_pages;
	view_class->print = gantt_view_print;
	view_class->print_cleanup = gantt_view_print_cleanup;
}

static void
planner_gantt_view_init (PlannerGanttView *view)
{
	view->priv = g_new0 (PlannerGanttViewPriv, 1);
}

static void
gantt_view_finalize (GObject *object)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (object);

	if (PLANNER_VIEW (view)->activated) {
		gantt_view_deactivate (PLANNER_VIEW (view));
	}

	g_free (view->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize) {
		(*G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
gantt_view_activate (PlannerView *view)
{
	PlannerGanttViewPriv *priv;
	gboolean              show_critical, show_nostd_days, show_guidelines;
	gchar                *filename;

	priv = PLANNER_GANTT_VIEW (view)->priv;

	priv->actions = gtk_action_group_new ("GanttView");
	gtk_action_group_set_translation_domain (priv->actions, GETTEXT_PACKAGE);

	gtk_action_group_add_actions (priv->actions,
				      entries,
				      G_N_ELEMENTS (entries),
				      view);
	gtk_action_group_add_toggle_actions (priv->actions,
					     toggle_entries,
					     G_N_ELEMENTS (toggle_entries),
					     view);

	gtk_ui_manager_insert_action_group (priv->ui_manager, priv->actions, 0);
	filename = mrp_paths_get_ui_dir ("gantt-view.ui");
	priv->merged_id = gtk_ui_manager_add_ui_from_file (priv->ui_manager,
							   filename,
							   NULL);
	g_free (filename);
	gtk_ui_manager_ensure_update (priv->ui_manager);

	/* Set the initial UI state. */
	show_critical = planner_gantt_chart_get_highlight_critical_tasks (
		PLANNER_GANTT_CHART (priv->gantt));

	show_nostd_days = planner_gantt_chart_get_nonstandard_days (
		PLANNER_GANTT_CHART (priv->gantt));

	show_guidelines = planner_gantt_chart_get_show_guidelines (
			PLANNER_GANTT_CHART (priv->gantt));

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

	gtk_toggle_action_set_active (
		GTK_TOGGLE_ACTION (gtk_action_group_get_action (priv->actions, "ShowGuideLines")),
		show_guidelines);

	gantt_view_selection_changed_cb (PLANNER_TASK_TREE (priv->tree),
					 PLANNER_GANTT_VIEW (view));
	gantt_view_update_zoom_sensitivity (PLANNER_GANTT_VIEW (view));

	gtk_widget_grab_focus (priv->tree);
}

static void
gantt_view_deactivate (PlannerView *view)
{
	PlannerGanttViewPriv *priv;

	priv = PLANNER_GANTT_VIEW (view)->priv;

	gtk_ui_manager_remove_ui (priv->ui_manager, priv->merged_id);
	gtk_ui_manager_remove_action_group (priv->ui_manager, priv->actions);
	g_object_unref (priv->actions);
	priv->actions = NULL;
}

static void
gantt_view_setup (PlannerView *view, PlannerWindow *main_window)
{
	PlannerGanttViewPriv *priv;

	priv = PLANNER_GANTT_VIEW (view)->priv;

	priv->ui_manager = planner_window_get_ui_manager (main_window);
}

static const gchar *
gantt_view_get_label (PlannerView *view)
{
	/* i18n: Label used for the sidebar. Please try to make it short and use
	 * a linebreak if necessary/possible.
	 */
	return _("Gantt");
}

static const gchar *
gantt_view_get_menu_label (PlannerView *view)
{
	return _("_Gantt Chart");
}

static const gchar *
gantt_view_get_icon (PlannerView *view)
{
	static gchar *filename = NULL;

	if (!filename) {
		filename = mrp_paths_get_image_dir ("gantt.png");
	}

	return filename;
}

static const gchar *
gantt_view_get_name (PlannerView *view)
{
	return "gantt_view";
}

static GtkWidget *
gantt_view_get_widget (PlannerView *view)
{
	PlannerGanttViewPriv *priv;

	priv = PLANNER_GANTT_VIEW (view)->priv;

	if (priv->paned == NULL) {
		priv->paned = gantt_view_create_widget (PLANNER_GANTT_VIEW (view));
		gtk_widget_show_all (priv->paned);
	}

	return priv->paned;
}

static void
gantt_view_print_init (PlannerView     *view,
		       PlannerPrintJob *job)
{
	PlannerGanttViewPriv *priv;
	gdouble          zoom;
	gboolean         show_critical;

	priv = PLANNER_GANTT_VIEW (view)->priv;

	zoom = planner_gantt_chart_get_zoom (PLANNER_GANTT_CHART (priv->gantt));

	show_critical = planner_gantt_chart_get_highlight_critical_tasks (
		PLANNER_GANTT_CHART (priv->gantt));

	priv->print_data = planner_gantt_print_data_new (view, job,
							 GTK_TREE_VIEW (priv->tree),
							 zoom, show_critical);
}

static void
gantt_view_print (PlannerView *view, gint page_nr)

{
	PlannerGanttViewPriv *priv;

	priv = PLANNER_GANTT_VIEW (view)->priv;

	planner_gantt_print_do (priv->print_data, page_nr);
}

static gint
gantt_view_print_get_n_pages (PlannerView *view)
{
	PlannerGanttViewPriv *priv;

	priv = PLANNER_GANTT_VIEW (view)->priv;

	return planner_gantt_print_get_n_pages (priv->print_data);
}

static void
gantt_view_print_cleanup (PlannerView *view)

{
	PlannerGanttViewPriv *priv;

	priv = PLANNER_GANTT_VIEW (view)->priv;

	planner_gantt_print_data_free (priv->print_data);
	priv->print_data = NULL;
}

static void
gantt_view_row_expanded (GtkTreeView *tree_view,
			 GtkTreeIter *iter,
			 GtkTreePath *path,
			 gpointer     data)
{
	PlannerGanttChart *gantt = data;

	planner_gantt_chart_expand_row (gantt, path);
}

static void
gantt_view_row_collapsed (GtkTreeView *tree_view,
			  GtkTreeIter *iter,
			  GtkTreePath *path,
			  gpointer     data)
{
	PlannerGanttChart *gantt = data;

	planner_gantt_chart_collapse_row (gantt, path);
}

static void
gantt_view_task_inserted_cb (MrpProject   *project,
			     MrpTask      *task,
			     PlannerGanttView *view)
{
	if(view->priv->expose_id == 0) {
		view->priv->expose_id = g_signal_connect_after (view->priv->tree,
								"expose_event",
								G_CALLBACK (gantt_view_expose_cb),
								view);
	}
}

static void
gantt_view_project_loaded_cb (MrpProject  *project,
			      PlannerGanttView *view)
{
	GtkTreeModel *model;

	model = GTK_TREE_MODEL (planner_gantt_model_new (project));

	planner_task_tree_set_model (PLANNER_TASK_TREE (view->priv->tree),
				     PLANNER_GANTT_MODEL (model));

	planner_gantt_chart_set_model (PLANNER_GANTT_CHART (view->priv->gantt),
				       model);

	view->priv->expose_id = g_signal_connect_after (view->priv->tree,
							"expose_event",
							G_CALLBACK (gantt_view_expose_cb),
							view);

	g_object_unref (model);
}

static void
gantt_view_tree_view_size_request_cb (GtkWidget      *widget,
				      GtkRequisition *req,
				      gpointer        data)
{
	req->height = -1;
}

static gboolean
gantt_view_tree_view_scroll_event_cb (GtkWidget      *widget,
				      GdkEventScroll *event,
				      gpointer        data)
{
	GtkAdjustment *adj;
	GtkTreeView   *tv = GTK_TREE_VIEW (widget);
	gdouble        new_value;

	if (event->direction != GDK_SCROLL_UP &&
	    event->direction != GDK_SCROLL_DOWN) {
		return FALSE;
	}

	adj = gtk_tree_view_get_vadjustment (tv);

	if (event->direction == GDK_SCROLL_UP) {
		new_value = adj->value - adj->page_increment / 2;
	} else {
		new_value = adj->value + adj->page_increment / 2;
	}

	new_value = CLAMP (new_value, adj->lower, adj->upper - adj->page_size);
	gtk_adjustment_set_value (adj, new_value);

	return TRUE;
}

static void
gantt_view_tree_view_columns_changed_cb (GtkTreeView *tree_view,
					 PlannerGanttView *view)
{
	gantt_view_save_columns (view);
}

static void
gantt_view_tree_view_destroy_cb (GtkTreeView *tree_view,
				 PlannerGanttView *view)
{
	/* Block, we don't want to save the column configuration when they are
	 * removed by the destruction.
	 */
	g_signal_handlers_block_by_func (tree_view,
					 gantt_view_tree_view_columns_changed_cb,
					 view);
}

static GtkWidget *
gantt_view_create_widget (PlannerGanttView *view)
{
	PlannerGanttViewPriv  *priv;
	GtkWidget        *tree;
	GtkWidget        *sw;
	GtkWidget        *frame;
	GtkWidget        *vbox;
	GtkWidget        *hpaned;
	GtkAdjustment    *hadj, *vadj;
	GtkTreeModel     *model;
	MrpProject       *project;
	GtkTreeSelection *selection;

	project = planner_window_get_project (PLANNER_VIEW (view)->main_window);

	priv = view->priv;

	g_signal_connect (project,
			  "loaded",
			  G_CALLBACK (gantt_view_project_loaded_cb),
			  view);

	g_signal_connect (project, "task_inserted",
			  G_CALLBACK (gantt_view_task_inserted_cb),
			  view);

	model = GTK_TREE_MODEL (planner_gantt_model_new (project));

	tree = planner_task_tree_new (PLANNER_VIEW (view)->main_window,
				      PLANNER_GANTT_MODEL (model),
				      FALSE,
				      TRUE,
				      /* i18n: WBS is short for work breakdown structure, and is a
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

	priv->tree = tree;

	gantt_view_load_columns (view);

	priv->gantt = planner_gantt_chart_new_with_model (model);
	planner_gantt_chart_set_view (PLANNER_GANTT_CHART (priv->gantt),
				      PLANNER_TASK_TREE (tree));

	gtk_widget_set_events (GTK_WIDGET (priv->gantt), GDK_SCROLL_MASK);

	g_signal_connect (priv->gantt, "scroll-event",
                    G_CALLBACK (gantt_view_chart_scroll_event), view);

	g_object_unref (model);

	g_signal_connect (tree,
			  "columns-changed",
			  G_CALLBACK (gantt_view_tree_view_columns_changed_cb),
			  view);

	g_signal_connect (tree,
			  "destroy",
			  G_CALLBACK (gantt_view_tree_view_destroy_cb),
			  view);

	g_signal_connect (priv->gantt,
			  "status_updated",
			  G_CALLBACK (gantt_view_gantt_status_updated_cb),
			  view);

	g_signal_connect (priv->gantt,
			  "resource_clicked",
			  G_CALLBACK (gantt_view_gantt_resource_clicked_cb),
			  view);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (tree,
			  "style_set",
			  G_CALLBACK (gantt_view_tree_style_set_cb),
			  view);

	g_signal_connect (tree,
			  "selection_changed",
			  G_CALLBACK (gantt_view_selection_changed_cb),
			  view);

	g_signal_connect (tree,
			  "relation_added",
			  G_CALLBACK (gantt_view_relations_changed_cb),
			  view);

	g_signal_connect (tree,
			  "relation_removed",
			  G_CALLBACK (gantt_view_relations_changed_cb),
			  view);

	vbox = gtk_vbox_new (FALSE, 3);

	gtk_box_pack_start (GTK_BOX (vbox), tree, TRUE, TRUE, 0);

	hadj = gtk_tree_view_get_hadjustment (GTK_TREE_VIEW (tree));
	gtk_box_pack_start (GTK_BOX (vbox), gtk_hscrollbar_new (hadj), FALSE, TRUE, 0);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	hpaned = gtk_hpaned_new ();
	gtk_paned_add1 (GTK_PANED (hpaned), frame);

	g_signal_connect_after (tree,
				"size_request",
				G_CALLBACK (gantt_view_tree_view_size_request_cb),
				NULL);

	g_signal_connect_after (tree,
				"scroll_event",
				G_CALLBACK (gantt_view_tree_view_scroll_event_cb),
				view);

	hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 0, 90, 250, 2000));
	vadj = gtk_tree_view_get_vadjustment (GTK_TREE_VIEW (tree));

	sw = gtk_scrolled_window_new (hadj, vadj);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_ALWAYS,
					GTK_POLICY_AUTOMATIC);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (frame), sw);

	gtk_container_add (GTK_CONTAINER (sw),
			   GTK_WIDGET (priv->gantt));

	gtk_paned_add2 (GTK_PANED (hpaned), frame);

	gtk_paned_set_position (GTK_PANED (hpaned), 250);

	g_signal_connect (tree,
			  "row_expanded",
			  G_CALLBACK (gantt_view_row_expanded),
			  priv->gantt);

	g_signal_connect (tree,
			  "row_collapsed",
			  G_CALLBACK (gantt_view_row_collapsed),
			  priv->gantt);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree));

	return hpaned;
}

/* Command callbacks. */

static void
gantt_view_insert_task_cb (GtkAction *action,
			   gpointer   data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_insert_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_insert_tasks_cb (GtkAction *action,
			    gpointer   data)
{
	PlannerGanttView *view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_insert_tasks (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_remove_task_cb (GtkAction *action,
			   gpointer   data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_remove_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_select_all_cb (GtkAction *action,
			  gpointer   data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_select_all (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_edit_task_cb (GtkAction *action,
			 gpointer   data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_edit_task (PLANNER_TASK_TREE (view->priv->tree),
				     PLANNER_TASK_DIALOG_PAGE_GENERAL);
}

static void
gantt_view_unlink_task_cb (GtkAction *action,
			   gpointer   data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_unlink_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_link_tasks_cb (GtkAction *action,
			  gpointer   data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_link_tasks (PLANNER_TASK_TREE (view->priv->tree),
				      MRP_RELATION_FS);
}

static void
gantt_view_indent_task_cb (GtkAction *action,
			   gpointer   data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_indent_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_unindent_task_cb (GtkAction *action,
			     gpointer   data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_unindent_task (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_move_task_up_cb (GtkAction  *action,
			    gpointer    data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_move_task_up (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_move_task_down_cb (GtkAction *action,
			      gpointer	 data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_move_task_down (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_reset_constraint_cb (GtkAction *action,
				gpointer   data)
{
	PlannerGanttView *view;

	view = PLANNER_GANTT_VIEW (data);

	planner_task_tree_reset_constraint (PLANNER_TASK_TREE (view->priv->tree));
}

static void
gantt_view_zoom_to_fit_cb (GtkAction *action,
			   gpointer   data)
{
	PlannerGanttView     *view;
	PlannerGanttViewPriv *priv;

	view = PLANNER_GANTT_VIEW (data);
	priv = view->priv;

	planner_gantt_chart_zoom_to_fit (PLANNER_GANTT_CHART (priv->gantt));

	gantt_view_update_zoom_sensitivity (view);
}

static void
gantt_view_zoom_in_cb (GtkAction *action,
		       gpointer   data)
{
	PlannerGanttView     *view;
	PlannerGanttViewPriv *priv;

	view = PLANNER_GANTT_VIEW (data);
	priv = view->priv;

	planner_gantt_chart_zoom_in (PLANNER_GANTT_CHART (priv->gantt));

	gantt_view_update_zoom_sensitivity (view);
}

static void
gantt_view_zoom_out_cb (GtkAction *action,
			gpointer   data)
{
	PlannerGanttView     *view;
	PlannerGanttViewPriv *priv;

	view = PLANNER_GANTT_VIEW (data);
	priv = view->priv;

	planner_gantt_chart_zoom_out (PLANNER_GANTT_CHART (priv->gantt));

	gantt_view_update_zoom_sensitivity (view);
}

static void
gantt_view_highlight_critical_cb (GtkAction *action,
				  gpointer   data)
{
	PlannerGanttView     *view;
	PlannerGanttViewPriv *priv;
	gboolean         state;

	view = PLANNER_GANTT_VIEW (data);
	priv = view->priv;

	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	planner_gantt_chart_set_highlight_critical_tasks (
		PLANNER_GANTT_CHART (priv->gantt),
		state);

	planner_task_tree_set_highlight_critical (
                PLANNER_TASK_TREE (priv->tree),
		state);
}

static void
gantt_view_nonstandard_days_cb (GtkAction *action,
				  gpointer   data)
{
	PlannerGanttView     *view;
	PlannerGanttViewPriv *priv;
	gboolean         state;

	view = PLANNER_GANTT_VIEW (data);
	priv = view->priv;

	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	planner_gantt_chart_set_nonstandard_days (
		PLANNER_GANTT_CHART (priv->gantt),
		state);
	planner_task_tree_set_nonstandard_days (
		PLANNER_TASK_TREE (priv->tree),
		state);
	gantt_view_update_row_height (view);

}

static void
gantt_view_edit_columns_cb (GtkAction *action,
			    gpointer   data)
{
	PlannerGanttView     *view;
	PlannerGanttViewPriv *priv;

	view = PLANNER_GANTT_VIEW (data);
	priv = view->priv;

	planner_column_dialog_show (PLANNER_VIEW (view)->main_window,
				    _("Edit Gantt Columns"),
				    GTK_TREE_VIEW (priv->tree));
}

static void
gantt_view_update_row_height (PlannerGanttView *view)
{
	GtkTreeView       *tv = GTK_TREE_VIEW (view->priv->tree);
	GtkWidget         *gantt = view->priv->gantt;
	gint               row_height;
	gint               header_height;
	gint               height;
	GList             *cols, *l;
	GtkTreeViewColumn *col;
	GtkRequisition     req;
	GtkTreePath       *path;
	GdkRectangle       rect;

	/* Get the row and header heights. */
	cols = gtk_tree_view_get_columns (tv);
	row_height = 0;
	header_height = 0;

	path = gtk_tree_path_new_first ();

	for (l = cols; l; l = l->next) {
		col = l->data;

		gtk_widget_size_request (col->button, &req);
		header_height = MAX (header_height, req.height);

		gtk_tree_view_column_cell_get_size (col,
						    NULL,
						    NULL,
						    NULL,
						    NULL,
						    &height);
		row_height = MAX (row_height, height);

		gtk_tree_view_get_background_area (tv,
						   path,
						   col,
						   &rect);

		row_height = MAX (row_height, rect.height);
	}
	if (path)
		gtk_tree_path_free (path);

	g_list_free (cols);

	/* Sync with the gantt widget. */
	g_object_set (gantt,
		      "header_height", header_height,
		      "row_height", row_height,
		      NULL);
}

static gboolean
gantt_view_expose_cb (GtkWidget      *widget,
		      GdkEventExpose *event,
		      gpointer        data)
{
	PlannerGanttView     *view = PLANNER_GANTT_VIEW (data);

	gantt_view_update_row_height (view);

	g_signal_handler_disconnect (view->priv->tree,
				     view->priv->expose_id);

	return FALSE;
}

static gboolean
idle_update_heights (PlannerGanttView *view)
{
	gantt_view_update_row_height (view);
	return FALSE;
}

static void
gantt_view_tree_style_set_cb (GtkWidget   *tree,
			      GtkStyle    *prev_style,
			      PlannerGanttView *view)
{
	if (prev_style) {
		g_idle_add ((GSourceFunc) idle_update_heights, view);
	} else {
		idle_update_heights (view);
	}
}

static void
gantt_view_selection_changed_cb (PlannerTaskTree *tree, PlannerGanttView *view)
{
	g_return_if_fail (PLANNER_IS_VIEW (view));

	gantt_view_update_ui (view);
}

static void
gantt_view_gantt_status_updated_cb (PlannerGanttChart *gantt,
				    const gchar       *message,
				    PlannerGanttView  *view)
{
	planner_window_set_status (PLANNER_VIEW (view)->main_window, message);
}

static void
gantt_view_gantt_resource_clicked_cb (PlannerGanttChart *chart,
				      MrpResource       *resource,
				      PlannerGanttView       *view)
{
	GtkWidget *dialog;

	dialog = planner_resource_dialog_new (PLANNER_VIEW (view)->main_window, resource);
	gtk_widget_show (dialog);
}

static void
gantt_view_relations_changed_cb (PlannerTaskTree *tree,
				 MrpTask         *task,
				 MrpRelation     *relation,
				 PlannerGanttView     *view)
{
	gantt_view_update_ui (view);
}

static void
gantt_view_update_ui (PlannerGanttView *view)
{
	PlannerGanttViewPriv *priv;
	GList                *list, *l;
	gboolean              value;
	gboolean              rel_value = FALSE;
	gboolean              link_value = FALSE;
	gint                  count_value = 0;

	if (!PLANNER_VIEW (view)->activated) {
		return;
	}

	priv = view->priv;

	list = planner_task_tree_get_selected_tasks (PLANNER_TASK_TREE (priv->tree));

	for (l = list; l; l = l->next) {
		if (mrp_task_has_relation (MRP_TASK (l->data))) {
			rel_value = TRUE;
			break;
		}
	}

	for (l = list; l; l = l->next) {
		count_value++;
	}

	value = (list != NULL);
	link_value = (count_value >= 2);

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
gantt_view_show_guidelines_cb (GtkAction *action,
				  gpointer   data)
{
	PlannerGanttView     *view;
	PlannerGanttViewPriv *priv;
	gboolean              state;

	view = PLANNER_GANTT_VIEW (data);
	priv = view->priv;

	state = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION(action));

	planner_gantt_chart_set_show_guidelines (
		PLANNER_GANTT_CHART (priv->gantt),
		state);
}

static void
gantt_view_update_zoom_sensitivity (PlannerGanttView *view)
{
	gboolean in, out;

	planner_gantt_chart_can_zoom (PLANNER_GANTT_CHART (view->priv->gantt),
				      &in,
				      &out);

	g_object_set (gtk_action_group_get_action (GTK_ACTION_GROUP(view->priv->actions),
						   "ZoomIn"),
		      "sensitive", in,
		      NULL);

	g_object_set (gtk_action_group_get_action (GTK_ACTION_GROUP(view->priv->actions),
						   "ZoomOut"),
		      "sensitive", out,
		      NULL);
}

static void
gantt_view_save_columns (PlannerGanttView *view)
{
	PlannerGanttViewPriv *priv;

	priv = view->priv;

	planner_view_column_save_helper (PLANNER_VIEW (view), GTK_TREE_VIEW (priv->tree));
}

static void
gantt_view_load_columns (PlannerGanttView *view)
{
	PlannerGanttViewPriv *priv;
	GList                *columns, *l;
	GtkTreeViewColumn    *column;
	const gchar          *id;
	gint                  i;

	priv = view->priv;

	/* Load the columns. */
	planner_view_column_load_helper (PLANNER_VIEW (view), GTK_TREE_VIEW (priv->tree));

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
			if (!id) {
				continue;
			}

			if (strcmp (id, "wbs") == 0 ||
			    strcmp (id, "name") == 0 ||
			    strcmp (id, "work") == 0) {
				gtk_tree_view_column_set_visible (column, TRUE);
			} else {
				gtk_tree_view_column_set_visible (column, FALSE);
			}
		}
	}

	g_list_free (columns);
}

PlannerView *
planner_gantt_view_new (void)
{
	PlannerView *view;

	view = g_object_new (PLANNER_TYPE_GANTT_VIEW, NULL);

	return view;
}
