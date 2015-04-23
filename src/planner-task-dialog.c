/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2005 Imendio AB
 * Copyright (C) 2002-2003 CodeFactory AB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2002-2004 Alvaro del Castillo <acs@barrapunto.com>
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
#include <time.h>
#include <string.h>
#include <stdlib.h> /* for atoi */
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <libplanner/mrp-object.h>
#include <libplanner/mrp-project.h>
#include <libplanner/mrp-private.h>
#include "libplanner/mrp-paths.h"
#include "planner-cell-renderer-list.h"
#include "planner-assignment-model.h"
#include "planner-predecessor-model.h"
#include "planner-task-cmd.h"
#include "planner-format.h"
#include "planner-popup-button.h"
#include "planner-task-date-widget.h"
#include "planner-task-dialog.h"


typedef struct {
	PlannerWindow *main_window;
	MrpTask       *task;
	GtkWidget     *dialog;
	GtkWidget     *predecessor_list;
	GtkWidget     *resource_list;
	GtkWidget     *name_entry;
	GtkWidget     *milestone_checkbutton;
	GtkWidget     *fixed_checkbutton;
	GtkWidget     *work_entry;
	GtkWidget     *duration_entry;
	GtkWidget     *schedule_label;
	GtkWidget     *schedule_button;
	GtkWidget     *complete_spinbutton;
	GtkWidget     *priority_spinbutton;
	GtkWidget     *note_textview;
	GtkTextBuffer *note_buffer;
} DialogData;


static void            task_dialog_close_clicked_cb               (GtkWidget               *w,
								   DialogData              *data);
static void            task_dialog_task_removed_cb                (MrpObject               *object,
								   GtkWidget               *dialog);
static void            task_dialog_task_name_changed_cb           (MrpTask                 *task,
								   GParamSpec              *pspec,
								   GtkWidget               *dialog);
static void            task_dialog_name_changed_cb                (GtkWidget               *w,
								   DialogData              *data);
static gboolean        task_dialog_name_focus_in_cb               (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static gboolean        task_dialog_name_focus_out_cb              (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static void            task_dialog_task_type_changed_cb           (MrpTask                 *task,
								   GParamSpec              *pspec,
								   GtkWidget               *dialog);
static void            task_dialog_type_toggled_cb                (GtkWidget               *w,
								   DialogData              *data);
static void            task_dialog_task_sched_changed_cb          (MrpTask                 *task,
								   GParamSpec              *pspec,
								   GtkWidget               *dialog);
static void            task_dialog_fixed_toggled_cb               (GtkWidget               *w,
								   DialogData              *data);
static void            task_dialog_task_work_changed_cb           (MrpTask                 *task,
								   GParamSpec              *pspec,
								   GtkWidget               *dialog);
static gboolean        task_dialog_work_focus_in_cb               (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static gboolean        task_dialog_work_focus_out_cb              (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static void            task_dialog_task_duration_changed_cb       (MrpTask                 *task,
								   GParamSpec              *pspec,
								   GtkWidget               *dialog);
static gboolean        task_dialog_duration_focus_in_cb           (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static gboolean        task_dialog_duration_focus_out_cb          (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static void            task_dialog_update_schedule_label          (DialogData              *data);
static void            task_dialog_task_constraint_changed_cb     (MrpTask                 *task,
								   GParamSpec              *pspec,
								   GtkWidget               *dialog);

static void            task_dialog_task_complete_changed_cb       (MrpTask                 *task,
								   GParamSpec              *pspec,
								   GtkWidget               *dialog);
static void            task_dialog_complete_changed_cb            (GtkWidget               *w,
								   DialogData              *data);
static gboolean        task_dialog_complete_focus_in_cb           (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static gboolean        task_dialog_complete_focus_out_cb          (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static void            task_dialog_task_priority_changed_cb       (MrpTask                 *task,
								   GParamSpec              *pspec,
								   GtkWidget               *dialog);
static void            task_dialog_priority_changed_cb            (GtkWidget               *w,
								   DialogData              *data);
static gboolean        task_dialog_priority_focus_in_cb           (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static gboolean        task_dialog_priority_focus_out_cb          (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static void            task_dialog_task_note_changed_cb           (MrpTask                 *task,
								   GParamSpec              *pspec,
								   GtkWidget               *dialog);
static void            task_dialog_note_changed_cb                (GtkWidget               *w,
								   DialogData              *data);
static gboolean        task_dialog_note_focus_in_cb               (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static gboolean        task_dialog_note_focus_out_cb              (GtkWidget               *w,
								   GdkEventFocus           *event,
								   DialogData              *data);
static void            task_dialog_note_stamp_cb                  (GtkWidget               *w,
								   DialogData              *data);
static void            task_dialog_task_child_added_or_removed_cb (MrpTask                 *task,
								   GtkWidget               *dialog);
static void            task_dialog_setup_widgets                  (DialogData              *data,
								   GladeXML                *glade);
static void            task_dialog_assignment_toggled_cb          (GtkCellRendererText     *cell,
								   gchar                   *path_str,
								   DialogData              *data);
static void            task_dialog_setup_resource_list            (DialogData              *data);
static void            task_dialog_connect_to_task                (DialogData              *data);
static void            task_dialog_resource_units_cell_edited     (GtkCellRendererText     *cell,
								   gchar                   *path_str,
								   gchar                   *new_text,
								   DialogData              *data);
static void            task_dialog_pred_cell_edited               (GtkCellRendererText     *cell,
								   gchar                   *path_str,
								   gchar                   *new_text,
								   DialogData              *data);
static MrpRelationType cell_index_to_relation_type                (gint                     i);
static void            task_dialog_cell_type_show_popup           (PlannerCellRendererList *cell,
								   const gchar             *path_string,
								   gint                     x1,
								   gint                     y1,
								   gint                     x2,
								   gint                     y2,
								   DialogData              *data);
static void            task_dialog_cell_type_hide_popup           (PlannerCellRendererList *cell,
								   GtkWidget               *view);
static void            task_dialog_add_predecessor_cb             (GtkWidget               *widget,
								   DialogData              *data);
static void            task_dialog_remove_predecessor_cb          (GtkWidget               *widget,
								   DialogData              *data);
static void            task_dialog_new_pred_ok_clicked_cb         (GtkWidget               *w,
								   GtkWidget               *dialog);
static void            task_dialog_new_pred_cancel_clicked_cb     (GtkWidget               *w,
								   GtkWidget               *dialog);
static void            task_dialog_update_sensitivity             (DialogData              *data);
static void            task_dialog_update_title                   (DialogData              *data);
extern gchar *         get_wbs_from_task                          (MrpTask *task);



/* Keep the dialogs here so that we can just raise the dialog if it's
 * opened twice for the same task.
 */
static GHashTable *dialogs = NULL;

#define DIALOG_GET_DATA(d) g_object_get_data ((GObject*)d, "data")

typedef struct {
	PlannerCmd  base;

	MrpTask    *task;
	gchar      *property;
	GValue     *value;
	GValue     *old_value;
} TaskCmdEditProperty;

typedef struct {
	PlannerCmd   base;

	MrpTask     *task;
	MrpTaskType  type;
	MrpTaskType  old_type;
} TaskCmdEditType;

typedef struct {
	PlannerCmd   base;

	MrpTask     *task;
	MrpTaskSched sched;
	MrpTaskSched old_sched;
} TaskCmdEditSchedule;

typedef struct {
	PlannerCmd     base;

	MrpTask       *task;
	MrpConstraint  constraint;
	MrpConstraint  old_constraint;
} TaskCmdEditConstraint;

typedef struct {
	PlannerCmd base;

	MrpTask       *task;
	MrpResource   *resource;
	MrpAssignment *assignment;
	guint          units;
	guint          old_units;
} TaskCmdEditAssignment;

typedef struct {
	PlannerCmd base;

	MrpTask         *task;
	MrpTask         *predecessor;
	MrpTask         *old_predecessor;
	guint            lag;
	guint            old_lag;
	MrpRelationType  rel_type;
	MrpRelationType  old_rel_type;
	GError          *error;
} TaskCmdEditPredecessor;

typedef struct {
	PlannerCmd base;

	MrpRelation  *relation;
	guint         lag;
	guint         old_lag;
} TaskCmdEditLag;

typedef struct {
	PlannerCmd   base;

	MrpTask  *task;
	gchar    *property;
	gchar    *note;
	gchar    *old_note;
} TaskCmdEditNote;

static void
task_dialog_setup_option_menu (GtkWidget     *option_menu,
			       GCallback      func,
			       gpointer       user_data,
			       gconstpointer  str1, ...)
{
	GtkWidget     *menu;
	GtkWidget     *menu_item;
	gint           i;
	va_list        args;
	gconstpointer  str;
	gint           type;

       	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
	if (menu) {
		gtk_widget_destroy (menu);
	}

	menu = gtk_menu_new ();

	va_start (args, str1);
	for (str = str1, i = 0; str != NULL; str = va_arg (args, gpointer), i++) {
		menu_item = gtk_menu_item_new_with_label (str);
		gtk_widget_show (menu_item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

		type = va_arg (args, gint);

		g_object_set_data (G_OBJECT (menu_item),
				   "data",
				   GINT_TO_POINTER (type));
		if (func) {
			g_signal_connect (menu_item,
					  "activate",
					  func,
					  user_data);
		}
	}
	va_end (args);

	gtk_widget_show (menu);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
}

static gint
task_dialog_option_menu_get_selected (GtkWidget *option_menu)
{
	GtkWidget *menu;
	GtkWidget *item;
	gint       ret;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));

	item = gtk_menu_get_active (GTK_MENU (menu));

	ret = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "data"));

	return ret;
}

#if 0
static void
task_dialog_option_menu_set_selected (GtkWidget *option_menu, gint data)
{
	GtkWidget *menu;
	GtkWidget *item;
	GList     *children, *l;
	gint       i;

       	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));

	children = GTK_MENU_SHELL (menu)->children;
	for (i = 0, l = children; l; i++, l = l->next) {
		item = l->data;

		if (GINT_TO_POINTER (data) == g_object_get_data (G_OBJECT (item), "data")) {
			gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), i);
			break;
		}
	}
}
#endif

static void
task_dialog_setup_task_combo (GtkComboBoxEntry *combo,
			      GList    *tasks)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GList        *l;
	const gchar  *name;

	if (tasks == NULL) {
		return;
	}

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	for (l = tasks; l; l = l->next) {
		GString *task_label;
		gchar *wbs;

		gtk_list_store_append (store, &iter);
		task_label = g_string_new( NULL);

		wbs = get_wbs_from_task (l->data);
		if (wbs != NULL && wbs[0] != 0)
			g_string_append_printf (task_label, "%s ", wbs);
		g_free(wbs);

		name = mrp_task_get_name (l->data);
		if (name != NULL && name[0] != 0)
			g_string_append (task_label, name);

		if (task_label->len == 0) {
			gtk_list_store_set (store, &iter,
			                    0, _("(No name)"),
			                    1, l->data,
			                    -1);
		} else {
			gtk_list_store_set (store, &iter,
			                    0, task_label->str,
			                    1, l->data,
			                    -1);
		}
		g_string_free(task_label, TRUE);
	}

	gtk_combo_box_set_model (GTK_COMBO_BOX (combo),
	                         GTK_TREE_MODEL (store));

	gtk_combo_box_entry_set_text_column (combo, 0);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
}

static gboolean
task_cmd_edit_property_do (PlannerCmd *cmd_base)
{
	TaskCmdEditProperty *cmd;

	cmd = (TaskCmdEditProperty*) cmd_base;

	g_object_set_property (G_OBJECT (cmd->task),
			       cmd->property,
			       cmd->value);

	return TRUE;
}

static void
task_cmd_edit_property_undo (PlannerCmd *cmd_base)
{
	TaskCmdEditProperty *cmd;

	cmd = (TaskCmdEditProperty*) cmd_base;

	g_object_set_property (G_OBJECT (cmd->task),
			       cmd->property,
			       cmd->old_value);
}

static void
task_cmd_edit_property_free (PlannerCmd *cmd_base)
{
	TaskCmdEditProperty *cmd;

	cmd = (TaskCmdEditProperty*) cmd_base;

	g_value_unset (cmd->value);
	g_value_unset (cmd->old_value);

	g_free (cmd->property);
	g_object_unref (cmd->task);
}

static PlannerCmd *
task_cmd_edit_property_focus (PlannerWindow *main_window,
			      MrpTask       *task,
			      const gchar   *property,
			      const GValue  *focus_in_value)
{
	PlannerCmd          *cmd_base;
	TaskCmdEditProperty *cmd;

	cmd_base = planner_cmd_new (TaskCmdEditProperty,
				    _("Edit task property"),
				    task_cmd_edit_property_do,
				    task_cmd_edit_property_undo,
				    task_cmd_edit_property_free);

	cmd = (TaskCmdEditProperty *) cmd_base;

	cmd->property = g_strdup (property);
	cmd->task = g_object_ref (task);

	cmd->value = g_new0 (GValue, 1);
	g_value_init (cmd->value, G_VALUE_TYPE (focus_in_value));
	g_object_get_property (G_OBJECT (cmd->task),
			       cmd->property,
			       cmd->value);

	cmd->old_value = g_new0 (GValue, 1);
	g_value_init (cmd->old_value, G_VALUE_TYPE (focus_in_value));
	g_value_copy (focus_in_value, cmd->old_value);

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (main_window),
					   cmd_base);

	return cmd_base;
}

static gboolean
task_cmd_edit_type_do (PlannerCmd *cmd_base)
{
	TaskCmdEditType *cmd;

	cmd = (TaskCmdEditType *) cmd_base;

	mrp_object_set (cmd->task, "type", cmd->type, NULL);

	return TRUE;
}

static void
task_cmd_edit_type_undo (PlannerCmd *cmd_base)
{
	TaskCmdEditType *cmd;

	cmd = (TaskCmdEditType *) cmd_base;

	mrp_object_set (cmd->task, "type", cmd->old_type, NULL);
}

static void
task_cmd_edit_type_free (PlannerCmd *cmd_base)
{
	TaskCmdEditType *cmd;

	cmd = (TaskCmdEditType *) cmd_base;

	g_object_unref (cmd->task);
}

static PlannerCmd *
task_cmd_edit_type (PlannerWindow *main_window,
		    MrpTask       *task,
		    MrpTaskType    type)
{
	PlannerCmd          *cmd_base;
	TaskCmdEditType     *cmd;
	MrpTaskType          old_type;

	mrp_object_get (task, "type", &old_type, NULL);

	if (old_type == type) {
		return NULL;
	}

	cmd_base = planner_cmd_new (TaskCmdEditType,
				    _("Edit task type"),
				    task_cmd_edit_type_do,
				    task_cmd_edit_type_undo,
				    task_cmd_edit_type_free);


	cmd = (TaskCmdEditType *) cmd_base;

	cmd->task = g_object_ref (task);

	cmd->old_type = old_type;
	cmd->type = type;

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (main_window),
					   cmd_base);

	return cmd_base;
}

static gboolean
task_cmd_edit_sched_do (PlannerCmd *cmd_base)
{
	TaskCmdEditSchedule *cmd;

	cmd = (TaskCmdEditSchedule *) cmd_base;

	mrp_object_set (cmd->task, "sched", cmd->sched, NULL);

	return TRUE;
}

static void
task_cmd_edit_sched_undo (PlannerCmd *cmd_base)
{
	TaskCmdEditSchedule *cmd;

	cmd = (TaskCmdEditSchedule *) cmd_base;

	mrp_object_set (cmd->task, "sched", cmd->old_sched, NULL);
}

static void
task_cmd_edit_sched_free (PlannerCmd *cmd_base)
{
	TaskCmdEditSchedule *cmd;

	cmd = (TaskCmdEditSchedule *) cmd_base;

	g_object_unref (cmd->task);
}

static PlannerCmd *
task_cmd_edit_sched (PlannerWindow *main_window,
		     MrpTask       *task,
		     MrpTaskSched   sched)
{
	PlannerCmd          *cmd_base;
	TaskCmdEditSchedule *cmd;
	MrpTaskSched         old_sched;

	mrp_object_get (task, "sched", &old_sched, NULL);

	if (old_sched == sched) {
		return NULL;
	}

	cmd_base = planner_cmd_new (TaskCmdEditSchedule,
				    _("Toggle fixed duration"),
				    task_cmd_edit_sched_do,
				    task_cmd_edit_sched_undo,
				    task_cmd_edit_sched_free);

	cmd = (TaskCmdEditSchedule *) cmd_base;

	cmd->task = g_object_ref (task);

	cmd->old_sched = old_sched;
	cmd->sched = sched;

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (main_window),
					   cmd_base);

	return cmd_base;
}

static gboolean
task_cmd_edit_constraint_do (PlannerCmd *cmd_base)
{
	TaskCmdEditConstraint *cmd;

	cmd = (TaskCmdEditConstraint *) cmd_base;

	mrp_object_set (cmd->task, "constraint", &cmd->constraint, NULL);

	return TRUE;
}

static void
task_cmd_edit_constraint_undo (PlannerCmd *cmd_base)
{
	TaskCmdEditConstraint *cmd;

	cmd = (TaskCmdEditConstraint *) cmd_base;

	mrp_object_set (cmd->task, "constraint", &cmd->old_constraint, NULL);
}

static void
task_cmd_edit_constraint_free (PlannerCmd *cmd_base)
{
	TaskCmdEditConstraint *cmd;

	cmd = (TaskCmdEditConstraint *) cmd_base;

	g_object_unref (cmd->task);
}

static PlannerCmd *
task_cmd_edit_constraint (PlannerWindow *main_window,
			  MrpTask       *task,
			  MrpConstraint *constraint)
{
	PlannerCmd            *cmd_base;
	TaskCmdEditConstraint *cmd;
	MrpConstraint         *old_constraint;

	g_object_get (task, "constraint", &old_constraint, NULL);

	if (old_constraint->type == constraint->type &&
	    old_constraint->time == constraint->time) {
		return NULL;
	}

	cmd_base = planner_cmd_new (TaskCmdEditConstraint,
				    _("Edit task scheduling"),
				    task_cmd_edit_constraint_do,
				    task_cmd_edit_constraint_undo,
				    task_cmd_edit_constraint_free);

	cmd = (TaskCmdEditConstraint *) cmd_base;

	cmd->task = g_object_ref (task);

	cmd->old_constraint = *old_constraint;
	cmd->constraint = *constraint;

	g_free (old_constraint);

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (main_window),
					   cmd_base);

	return cmd_base;
}

static gboolean
task_cmd_assign_add_do (PlannerCmd *cmd_base)
{
	TaskCmdEditAssignment *cmd;

	cmd = (TaskCmdEditAssignment *) cmd_base;

	/* FIXME: better than returning void it could return the assignment */
	mrp_resource_assign (cmd->resource, cmd->task, cmd->units);

	return TRUE;
}

static void
task_cmd_assign_add_undo (PlannerCmd *cmd_base)
{
	TaskCmdEditAssignment *cmd;
	MrpAssignment         *assignment;

	cmd = (TaskCmdEditAssignment *) cmd_base;

	assignment = mrp_task_get_assignment (cmd->task, cmd->resource);

	mrp_object_removed (MRP_OBJECT (assignment));
}

static void
task_cmd_assign_add_free (PlannerCmd *cmd_base)
{
	TaskCmdEditAssignment *cmd;

	cmd = (TaskCmdEditAssignment *) cmd_base;

	g_object_unref (cmd->task);
	g_object_unref (cmd->resource);
}

static PlannerCmd *
task_cmd_assign_add (PlannerWindow *main_window,
		     MrpTask       *task,
		     MrpResource   *resource,
		     guint          units)
{
	PlannerCmd             *cmd_base;
	TaskCmdEditAssignment  *cmd;
	MrpAssignment          *assignment;
	guint                   old_units;

	assignment = mrp_task_get_assignment (task, resource);
	if (assignment) {
		old_units = mrp_assignment_get_units (assignment);

		if (old_units == units) {
			return NULL;
		}
	} else {
		old_units = 0;
	}

	cmd_base = planner_cmd_new (TaskCmdEditAssignment,
				    _("Assign resource to task"),
				    task_cmd_assign_add_do,
				    task_cmd_assign_add_undo,
				    task_cmd_assign_add_free);

	cmd = (TaskCmdEditAssignment *) cmd_base;
	cmd->task = g_object_ref (task);
	cmd->resource = g_object_ref (resource);
	cmd->old_units = old_units;
	cmd->units = units;

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (main_window),
					   cmd_base);

	return cmd_base;
}

static gboolean
task_cmd_assign_remove_do (PlannerCmd *cmd_base)
{
	TaskCmdEditAssignment *cmd;
	MrpAssignment         *assignment;

	cmd = (TaskCmdEditAssignment *) cmd_base;

	assignment = mrp_task_get_assignment (cmd->task, cmd->resource);
	mrp_object_removed (MRP_OBJECT (assignment));

	return TRUE;
}

static void
task_cmd_assign_remove_undo (PlannerCmd *cmd_base)
{
	TaskCmdEditAssignment *cmd;

	cmd = (TaskCmdEditAssignment *) cmd_base;

	mrp_resource_assign (cmd->resource, cmd->task, cmd->units);
}

static void
task_cmd_assign_remove_free (PlannerCmd *cmd_base)
{
	TaskCmdEditAssignment *cmd;

	cmd = (TaskCmdEditAssignment *) cmd_base;

	g_object_unref (cmd->task);
	g_object_unref (cmd->resource);
}

static PlannerCmd *
task_cmd_assign_remove (PlannerWindow *main_window,
			MrpAssignment *assignment)
{
	PlannerCmd             *cmd_base;
	TaskCmdEditAssignment  *cmd;

	cmd_base = planner_cmd_new (TaskCmdEditAssignment,
				    _("Unassign resource from task"),
				    task_cmd_assign_remove_do,
				    task_cmd_assign_remove_undo,
				    task_cmd_assign_remove_free);

	cmd = (TaskCmdEditAssignment *) cmd_base;
	cmd->task = g_object_ref (mrp_assignment_get_task (assignment));
	cmd->resource = g_object_ref (mrp_assignment_get_resource (assignment));
	cmd->units = mrp_assignment_get_units (assignment);

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (main_window),
					   cmd_base);

	return cmd_base;
}

static gboolean
task_cmd_assign_units_do (PlannerCmd *cmd_base)
{
	TaskCmdEditAssignment *cmd;

	cmd = (TaskCmdEditAssignment *) cmd_base;

	g_object_set (cmd->assignment, "units", cmd->units, NULL);

	return TRUE;
}

static void
task_cmd_assign_units_undo (PlannerCmd *cmd_base)
{
	TaskCmdEditAssignment *cmd;

	cmd = (TaskCmdEditAssignment *) cmd_base;

	g_object_set (cmd->assignment, "units", cmd->old_units, NULL);

}

static void
task_cmd_assign_units_free (PlannerCmd *cmd_base)
{
	TaskCmdEditAssignment *cmd;

	cmd = (TaskCmdEditAssignment *) cmd_base;

	g_object_unref (cmd->task);
	g_object_unref (cmd->resource);
}

static PlannerCmd *
task_cmd_assign_units (PlannerWindow *main_window,
		       MrpAssignment *assignment,
		       guint          units)
{
	PlannerCmd             *cmd_base;
	TaskCmdEditAssignment  *cmd;

	cmd_base = planner_cmd_new (TaskCmdEditAssignment,
				    _("Change resource units in task"),
				    task_cmd_assign_units_do,
				    task_cmd_assign_units_undo,
				    task_cmd_assign_units_free);

	cmd = (TaskCmdEditAssignment *) cmd_base;
	cmd->task = g_object_ref (mrp_assignment_get_task (assignment));
	cmd->resource = g_object_ref (mrp_assignment_get_resource (assignment));
	cmd->assignment = assignment;
	cmd->units = units;
	cmd->old_units = mrp_assignment_get_units (assignment);

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (main_window),
					   cmd_base);

	return cmd_base;
}

static gboolean
task_cmd_edit_pred_do (PlannerCmd *cmd_base)
{
	GError                 *error = NULL;
	gboolean                retval;
	TaskCmdEditPredecessor *cmd = (TaskCmdEditPredecessor*) cmd_base;

	mrp_task_remove_predecessor (cmd->task, cmd->old_predecessor);

	mrp_task_add_predecessor (cmd->task,
				  cmd->predecessor,
				  cmd->rel_type,
				  cmd->lag,
				  &error);

	if (!error) {
		retval = TRUE;
	} else {
		cmd->error = error;
		retval = FALSE;
	}

	return retval;
}

static void
task_cmd_edit_pred_undo (PlannerCmd *cmd_base)
{
	GError                 *error = NULL;
	TaskCmdEditPredecessor *cmd = (TaskCmdEditPredecessor*) cmd_base;

	mrp_task_remove_predecessor (cmd->task, cmd->predecessor);

	mrp_task_add_predecessor (cmd->task,
				  cmd->old_predecessor,
				  cmd->old_rel_type,
				  cmd->old_lag,
				  &error);

	g_assert (error == NULL);
}

static void
task_cmd_edit_pred_free (PlannerCmd *cmd_base)
{
	TaskCmdEditPredecessor *cmd;

	cmd = (TaskCmdEditPredecessor *) cmd_base;

	g_object_unref (cmd->task);
	g_object_unref (cmd->predecessor);
	g_object_unref (cmd->old_predecessor);
}



static PlannerCmd *
planner_task_cmd_edit_predecessor (PlannerWindow   *main_window,
				   MrpTask         *task,
				   MrpTask         *old_predecessor,
				   MrpTask         *predecessor,
				   MrpRelationType  relationship,
				   glong            lag,
				   GError         **error)
{
	PlannerCmd             *cmd_base;
	TaskCmdEditPredecessor *cmd;
	MrpRelation            *relation;

	cmd_base = planner_cmd_new (TaskCmdEditPredecessor,
				    _("Edit task predecessor"),
				    task_cmd_edit_pred_do,
				    task_cmd_edit_pred_undo,
				    task_cmd_edit_pred_free);

	cmd = (TaskCmdEditPredecessor *) cmd_base;

	cmd->old_predecessor = g_object_ref (old_predecessor);
	cmd->predecessor = g_object_ref (predecessor);
	cmd->task = g_object_ref (task);

	relation = mrp_task_get_relation (task, old_predecessor);

	cmd->old_rel_type = mrp_relation_get_relation_type (relation);
	cmd->rel_type = relationship;
	cmd->old_lag = mrp_relation_get_lag (relation);
	cmd->lag = lag;

	planner_cmd_manager_insert_and_do (
		planner_window_get_cmd_manager (main_window), cmd_base);

	if (cmd->error) {
		g_propagate_error (error, cmd->error);
		return NULL;
	}

	return cmd_base;
}

static gboolean
task_cmd_edit_lag_do (PlannerCmd *cmd_base)
{
	TaskCmdEditLag *cmd = (TaskCmdEditLag *) cmd_base;

	mrp_object_set (cmd->relation, "lag", cmd->lag, NULL);

	return TRUE;
}

static void
task_cmd_edit_lag_undo (PlannerCmd *cmd_base)
{
	TaskCmdEditLag *cmd = (TaskCmdEditLag *) cmd_base;

	mrp_object_set (cmd->relation, "lag", cmd->old_lag, NULL);
}

static void
task_cmd_edit_lag_free (PlannerCmd *cmd_base)
{
	TaskCmdEditLag *cmd = (TaskCmdEditLag *) cmd_base;

	g_object_unref (cmd->relation);
}

static PlannerCmd *
task_cmd_edit_lag (PlannerWindow *main_window,
		   MrpRelation   *relation,
		   guint          lag)
{
	PlannerCmd          *cmd_base;
	TaskCmdEditLag      *cmd;
	guint                old_lag;

	mrp_object_get (relation, "lag", &old_lag, NULL);

	if (old_lag == lag) {
		return NULL;
	}

	cmd_base = planner_cmd_new (TaskCmdEditType,
				    _("Edit lag predecessor"),
				    task_cmd_edit_lag_do,
				    task_cmd_edit_lag_undo,
				    task_cmd_edit_lag_free);


	cmd = (TaskCmdEditLag *) cmd_base;

	cmd->relation = g_object_ref (relation);

	cmd->old_lag = old_lag;
	cmd->lag = lag;

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (main_window),
					   cmd_base);

	return cmd_base;
}

static gboolean
task_cmd_edit_note_do (PlannerCmd *cmd_base)
{
	TaskCmdEditNote *cmd = (TaskCmdEditNote*) cmd_base;

	g_object_set (cmd->task, "note", cmd->note, NULL);

	return TRUE;
}

static void
task_cmd_edit_note_undo (PlannerCmd *cmd_base)
{

	TaskCmdEditNote *cmd = (TaskCmdEditNote*) cmd_base;

	g_object_set (cmd->task, "note", cmd->old_note, NULL);
}

static void
task_cmd_edit_note_free (PlannerCmd *cmd_base)
{
	TaskCmdEditNote *cmd = (TaskCmdEditNote*) cmd_base;

	g_free (cmd->note);
	g_free (cmd->old_note);
	g_free (cmd->property);

	g_object_unref (cmd->task);
}

static PlannerCmd *
task_cmd_edit_note (DialogData  *data,
		    const gchar *focus_in_note)
{
	TaskCmdEditNote *cmd;
	gchar           *note;

	g_object_get (data->task, "note", &note, NULL);

	if (strcmp (note, focus_in_note) == 0) {
		g_free (note);
		return NULL;
	}

	cmd = (TaskCmdEditNote *) planner_cmd_new (TaskCmdEditNote,
						   _("Edit task note"),
						   task_cmd_edit_note_do,
						   task_cmd_edit_note_undo,
						   task_cmd_edit_note_free);

	cmd->property = g_strdup ("note");
	cmd->task = g_object_ref (data->task);

	cmd->old_note = g_strdup (focus_in_note);
	cmd->note = note;

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (data->main_window),
					   (PlannerCmd *) cmd);

	return (PlannerCmd *) cmd;
}


static void
task_dialog_close_clicked_cb (GtkWidget *w, DialogData *data)
{
	gtk_widget_destroy (data->dialog);
}

static void
task_dialog_task_removed_cb (MrpObject *object, GtkWidget *dialog)
{
	gtk_widget_destroy (dialog);
}

static void
task_dialog_task_name_changed_cb (MrpTask *task, GParamSpec *pspec, GtkWidget *dialog)
{
	DialogData *data;
	gchar      *name;

	data = DIALOG_GET_DATA (dialog);

	g_object_get (task, "name", &name, NULL);

	g_signal_handlers_block_by_func (data->name_entry,
					 task_dialog_name_changed_cb,
					 dialog);

	gtk_entry_set_text (GTK_ENTRY (data->name_entry), name);

	task_dialog_update_title (data);

	g_signal_handlers_unblock_by_func (data->name_entry,
					   task_dialog_name_changed_cb,
					   dialog);

	g_free (name);
}

static void
task_dialog_name_changed_cb (GtkWidget *w, DialogData *data)
{
	const gchar *name;

	name = gtk_entry_get_text (GTK_ENTRY (w));

	g_signal_handlers_block_by_func (data->task,
					 task_dialog_task_name_changed_cb,
					 data->dialog);

	g_object_set (data->task, "name", name, NULL);

	task_dialog_update_title (data);

	g_signal_handlers_unblock_by_func (data->task,
					   task_dialog_task_name_changed_cb,
					   data->dialog);
}

static gboolean
task_dialog_name_focus_out_cb (GtkWidget     *w,
			       GdkEventFocus *event,
			       DialogData    *data)
{
	gchar        *focus_in_name;
	GValue        value = { 0 };

	g_assert (MRP_IS_TASK (data->task));

	focus_in_name = g_object_get_data (G_OBJECT (data->task),"focus_in_name");

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, focus_in_name);

	task_cmd_edit_property_focus (data->main_window,
				      data->task, "name", &value);

	g_free (focus_in_name);

	return FALSE;
}

static gboolean
task_dialog_name_focus_in_cb (GtkWidget     *w,
			      GdkEventFocus *event,
			      DialogData    *data)
{
	gchar  *name;

	name = g_strdup (gtk_entry_get_text (GTK_ENTRY (w)));

	g_object_set_data (G_OBJECT (data->task),
			   "focus_in_name", (gpointer) name);

	return FALSE;
}

static void
task_dialog_task_type_changed_cb (MrpTask *task, GParamSpec *pspec, GtkWidget *dialog)
{
	DialogData  *data;
	MrpTaskType  type;

	data = DIALOG_GET_DATA (dialog);

	g_object_get (task, "type", &type, NULL);

	g_signal_handlers_block_by_func (data->milestone_checkbutton,
					 task_dialog_type_toggled_cb,
					 dialog);

	if (type == MRP_TASK_TYPE_MILESTONE) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data->milestone_checkbutton), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data->milestone_checkbutton), FALSE);
	}

	g_signal_handlers_unblock_by_func (data->milestone_checkbutton,
					   task_dialog_type_toggled_cb,
					   dialog);

	task_dialog_update_sensitivity (data);
}

static void
task_dialog_type_toggled_cb (GtkWidget *w, DialogData *data)
{
	MrpTaskType type;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->milestone_checkbutton))) {
		type = MRP_TASK_TYPE_MILESTONE;
	} else {
		type = MRP_TASK_TYPE_NORMAL;
	}

	g_signal_handlers_block_by_func (data->task,
					 task_dialog_task_type_changed_cb,
					 data->dialog);

	/* g_object_set (data->task, "type", type, NULL); */
	task_cmd_edit_type (data->main_window, data->task, type);

	g_signal_handlers_unblock_by_func (data->task,
					   task_dialog_task_type_changed_cb,
					   data->dialog);

	task_dialog_update_sensitivity (data);
}

static void
task_dialog_task_sched_changed_cb (MrpTask *task, GParamSpec *pspec, GtkWidget *dialog)
{
	DialogData   *data;
	MrpTaskSched  sched;

	data = DIALOG_GET_DATA (dialog);

	g_object_get (task, "sched", &sched, NULL);

	g_signal_handlers_block_by_func (data->fixed_checkbutton,
					 task_dialog_fixed_toggled_cb,
					 dialog);

	if (sched == MRP_TASK_SCHED_FIXED_DURATION) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data->fixed_checkbutton), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data->fixed_checkbutton), FALSE);
	}

	/* Set the toggle */

	g_signal_handlers_unblock_by_func (data->fixed_checkbutton,
					   task_dialog_fixed_toggled_cb,
					   dialog);

	task_dialog_update_sensitivity (data);
}

static void
task_dialog_fixed_toggled_cb (GtkWidget *w, DialogData *data)
{
	MrpTaskSched sched;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (data->fixed_checkbutton))) {
		sched = MRP_TASK_SCHED_FIXED_DURATION;
	} else {
		sched = MRP_TASK_SCHED_FIXED_WORK;
	}

	g_signal_handlers_block_by_func (data->task,
					 task_dialog_task_sched_changed_cb,
					 data->dialog);

	/* g_object_set (data->task, "sched", sched, NULL); */
	task_cmd_edit_sched (data->main_window, data->task, sched);

	g_signal_handlers_unblock_by_func (data->task,
					   task_dialog_task_sched_changed_cb,
					   data->dialog);

	task_dialog_update_sensitivity (data);
}

static void
task_dialog_task_work_changed_cb (MrpTask    *task,
				  GParamSpec *pspec,
				  GtkWidget  *dialog)
{
	DialogData *data;
	MrpProject *project;
	gint        work;
	gchar      *str;

	data = DIALOG_GET_DATA (dialog);

	g_object_get (task,
		      "work", &work,
		      "project", &project,
		      NULL);

	str = planner_format_duration (project, work);
	gtk_entry_set_text (GTK_ENTRY (data->work_entry), str);
	g_free (str);
}

static gboolean
task_dialog_work_focus_out_cb (GtkWidget     *w,
			       GdkEventFocus *event,
			       DialogData    *data)
{
	MrpProject   *project;
	gint          current_work;
	gint          focus_in_work;
	GValue        value = { 0 };

	g_object_get (data->task, "project", &project, NULL);

	focus_in_work = GPOINTER_TO_INT (
		g_object_get_data (G_OBJECT (data->task), "focus_in_work"));

	current_work = planner_parse_duration (
		project, gtk_entry_get_text (GTK_ENTRY (w)));

	if (focus_in_work == current_work) {
		return FALSE;
	}

	g_object_set (data->task, "work", current_work, NULL);

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, focus_in_work);

	task_cmd_edit_property_focus (data->main_window,
				      data->task, "work",
				      &value);

	return FALSE;
}

static gboolean
task_dialog_work_focus_in_cb (GtkWidget     *w,
			      GdkEventFocus *event,
			      DialogData    *data)
{
	MrpProject  *project;
	const gchar *str;
	gint         work;

	str = gtk_entry_get_text (GTK_ENTRY (w));

	g_object_get (data->task, "project", &project, NULL);

	work = planner_parse_duration (project, str);

	g_object_set_data (G_OBJECT (data->task),
			   "focus_in_work",
			   GINT_TO_POINTER (work));

	return FALSE;
}

static void
task_dialog_task_duration_changed_cb (MrpTask    *task,
				      GParamSpec *pspec,
				      GtkWidget  *dialog)
{
	DialogData *data;
	MrpProject *project;
	gint        duration;
	gchar      *str;

	data = DIALOG_GET_DATA (dialog);

	g_object_get (task,
		      "duration", &duration,
		      "project", &project,
		      NULL);

	str = planner_format_duration (project, duration);
	gtk_entry_set_text (GTK_ENTRY (data->duration_entry), str);
	g_free (str);
}

static gboolean
task_dialog_duration_focus_out_cb (GtkWidget     *w,
				   GdkEventFocus *event,
				   DialogData    *data)
{
	MrpProject   *project;
	gint          current_duration;
	gint          focus_in_duration;
	GValue        value = { 0 };

	g_object_get (data->task, "project", &project, NULL);

	focus_in_duration = GPOINTER_TO_INT (
		g_object_get_data (G_OBJECT (data->task), "focus_in_duration"));

	current_duration = planner_parse_duration (
		project, gtk_entry_get_text (GTK_ENTRY (w)));

	if (focus_in_duration == current_duration) {
		return FALSE;
	}

	g_object_set (data->task, "duration", current_duration, NULL);

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, focus_in_duration);

	task_cmd_edit_property_focus (data->main_window,
				      data->task, "duration",
				      &value);

	return FALSE;
}

static gboolean
task_dialog_duration_focus_in_cb (GtkWidget     *w,
				  GdkEventFocus *event,
				  DialogData    *data)
{
	const gchar *str;
	gint         duration;
	MrpProject  *project;

	str = gtk_entry_get_text (GTK_ENTRY (w));

	g_object_get (data->task, "project", &project, NULL);

	duration = planner_parse_duration (project, str);

	g_object_set_data (G_OBJECT (data->task),
			   "focus_in_duration",
			   GINT_TO_POINTER (duration));

	return FALSE;
}

static void
task_dialog_task_constraint_changed_cb (MrpTask    *task,
					GParamSpec *pspec,
					GtkWidget  *dialog)
{
	DialogData *data;

	data = DIALOG_GET_DATA (dialog);

	task_dialog_update_schedule_label (data);
}

static void
task_dialog_update_schedule_label (DialogData *data)
{
	MrpConstraint     *constraint;
	gchar             *time_str;
	gchar             *str;

	g_object_get (data->task, "constraint", &constraint, NULL);

	switch (constraint->type) {
	case MRP_CONSTRAINT_ASAP:
		time_str = NULL;
		str = g_strdup (_("As soon as possible"));
		break;
	case MRP_CONSTRAINT_SNET:
		time_str = mrp_time_format_locale (constraint->time);
		str = g_strdup_printf (_("No earlier than %s"), time_str);
		break;
	case MRP_CONSTRAINT_MSO:
		time_str = mrp_time_format_locale (constraint->time);
		str = g_strdup_printf (_("On fixed date %s"), time_str);
		break;
	default:
		str = NULL;
		g_assert_not_reached ();
	}

	gtk_label_set_text (GTK_LABEL (data->schedule_label), str);

	g_free (str);
	g_free (time_str);

	g_free (constraint);
}

static void
task_dialog_task_complete_changed_cb (MrpTask    *task,
				      GParamSpec *pspec,
				      GtkWidget  *dialog)
{
	DialogData *data;
	gshort      complete;

	data = DIALOG_GET_DATA (dialog);

	complete = mrp_task_get_percent_complete (task);

	g_signal_handlers_block_by_func (data->complete_spinbutton,
					 task_dialog_complete_changed_cb,
					 data);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (data->complete_spinbutton),
				   complete);

	g_signal_handlers_unblock_by_func (data->complete_spinbutton,
					   task_dialog_complete_changed_cb,
					   data);
}

static void
task_dialog_complete_changed_cb (GtkWidget  *w,
				 DialogData *data)
{
	gint complete;

	complete = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

	g_signal_handlers_block_by_func (data->task,
					 task_dialog_task_complete_changed_cb,
					 data->dialog);

	g_object_set (data->task, "percent_complete", complete, NULL);

	g_signal_handlers_unblock_by_func (data->task,
					   task_dialog_task_complete_changed_cb,
					   data->dialog);
}

static gboolean
task_dialog_complete_focus_out_cb (GtkWidget     *w,
				   GdkEventFocus *event,
				   DialogData    *data)
{
	guint         current_complete;
	guint         focus_in_complete;
	GValue        value = { 0 };

	g_assert (MRP_IS_TASK (data->task));

	focus_in_complete = GPOINTER_TO_INT (g_object_get_data
					     (G_OBJECT (data->task), "focus_in_complete"));

	gtk_spin_button_update (GTK_SPIN_BUTTON (w));
	current_complete = gtk_spin_button_get_value (GTK_SPIN_BUTTON (w));

	if (focus_in_complete == current_complete) {
		return FALSE;
	}

	g_object_set (data->task, "percent_complete", current_complete, NULL);

	g_value_init (&value, G_TYPE_UINT);
	g_value_set_uint (&value, focus_in_complete);

	task_cmd_edit_property_focus (data->main_window,
				      data->task, "percent_complete", &value);

	return FALSE;
}

static gboolean
task_dialog_complete_focus_in_cb (GtkWidget     *w,
				  GdkEventFocus *event,
				  DialogData    *data)
{
	guint complete;

	complete = gtk_spin_button_get_value (GTK_SPIN_BUTTON (w));

	g_object_set_data (G_OBJECT (data->task),
			   "focus_in_complete", GINT_TO_POINTER (complete));

	return FALSE;
}

static void
task_dialog_task_priority_changed_cb (MrpTask    *task,
				      GParamSpec *pspec,
				      GtkWidget  *dialog)
{
	DialogData *data;
	gint       priority;

	data = DIALOG_GET_DATA (dialog);

	g_object_get (task, "priority", &priority, NULL);

	g_signal_handlers_block_by_func (data->priority_spinbutton,
					 task_dialog_priority_changed_cb,
					 data);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (data->priority_spinbutton),
				   priority);

	g_signal_handlers_unblock_by_func (data->priority_spinbutton,
					   task_dialog_priority_changed_cb,
					   data);
}

static void
task_dialog_priority_changed_cb (GtkWidget  *w,
				 DialogData *data)
{
	gint priority;

	priority = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

	g_signal_handlers_block_by_func (data->task,
					 task_dialog_task_priority_changed_cb,
					 data->dialog);

	g_object_set (data->task, "priority", priority, NULL);

	g_signal_handlers_unblock_by_func (data->task,
					   task_dialog_task_priority_changed_cb,
					   data->dialog);
}

static gboolean
task_dialog_priority_focus_out_cb (GtkWidget     *w,
				   GdkEventFocus *event,
				   DialogData    *data)
{
	gint          current_priority;
	gint          focus_in_priority;
	GValue        value = { 0 };
	PlannerCmd __attribute__ ((unused))  *cmd;

	g_assert (MRP_IS_TASK (data->task));

	focus_in_priority = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (data->task),
								"focus_in_priority"));

	gtk_spin_button_update (GTK_SPIN_BUTTON (w));
	current_priority = gtk_spin_button_get_value (GTK_SPIN_BUTTON (w));

	if (focus_in_priority == current_priority) {
		return FALSE;
	}

	g_object_set (data->task, "priority", current_priority, NULL);

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, focus_in_priority);

	cmd = task_cmd_edit_property_focus (data->main_window,
					    data->task, "priority", &value);

	return FALSE;
}

static gboolean
task_dialog_priority_focus_in_cb (GtkWidget     *w,
				  GdkEventFocus *event,
				  DialogData    *data)
{
	gint priority;

	priority = gtk_spin_button_get_value (GTK_SPIN_BUTTON (w));

	g_object_set_data (G_OBJECT (data->task),
			   "focus_in_priority", GINT_TO_POINTER (priority));

	return FALSE;
}

static void
task_dialog_task_note_changed_cb (MrpTask    *task,
				  GParamSpec *pspec,
				  GtkWidget  *dialog)
{
	DialogData *data;
	gchar      *note;

	data = DIALOG_GET_DATA (dialog);

	g_object_get (task, "note", &note, NULL);

	g_signal_handlers_block_by_func (data->note_buffer,
					 task_dialog_note_changed_cb,
					 data);

	gtk_text_buffer_set_text (data->note_buffer, note, -1);

	g_signal_handlers_unblock_by_func (data->note_buffer,
					   task_dialog_note_changed_cb,
					   data);

	g_free (note);
}

static void
task_dialog_note_changed_cb (GtkWidget  *w,
			     DialogData *data)
{
	const gchar   *note;
	GtkTextIter    start, end;
	GtkTextBuffer *buffer;

	buffer = GTK_TEXT_BUFFER (w);

	gtk_text_buffer_get_bounds (buffer, &start, &end);

	note = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	g_signal_handlers_block_by_func (data->task,
					 task_dialog_task_note_changed_cb,
					 data->dialog);

	g_object_set (data->task, "note", note, NULL);

	g_signal_handlers_unblock_by_func (data->task,
					   task_dialog_task_note_changed_cb,
					   data->dialog);
}

static gboolean
task_dialog_note_focus_out_cb (GtkWidget     *w,
			       GdkEventFocus *event,
			       DialogData    *data)
{
	gchar        *focus_in_note;

	focus_in_note = g_object_get_data (G_OBJECT (data->task),"focus_in_note");

	task_cmd_edit_note (data, focus_in_note);

	if (g_getenv ("PLANNER_DEBUG_UNDO_TASK")) {
		gchar *note;

		g_object_get (data->task, "note", &note, NULL);
		g_message ("Note focus out value: %s", note);
		g_free (note);
	}

	g_free (focus_in_note);

	return FALSE;
}

static gboolean
task_dialog_note_focus_in_cb (GtkWidget     *w,
			      GdkEventFocus *event,
			      DialogData    *data)
{
	gchar *note;

	g_object_get (data->task, "note", &note, NULL);

	if (g_getenv ("PLANNER_DEBUG_UNDO_RESOURCE")) {
		g_message ("Note focus in value: %s", note);
	}

	gtk_text_buffer_set_text (data->note_buffer, note, -1);

	g_object_set_data (G_OBJECT (data->task),
			   "focus_in_note", (gpointer) note);

	return FALSE;
}

static void
task_dialog_note_stamp_cb (GtkWidget  *w,
			   DialogData *data)
{
	GtkTextIter  end;
	time_t       t;
	struct tm   *tm;
	gchar        stamp[128];
	gchar       *utf8;
	GtkTextMark *mark;

	t = time (NULL);
	tm = localtime (&t);

	/* i18n: time stamp format for notes in task dialog, see strftime(3) for
	 * a detailed description. */
	strftime (stamp, sizeof (stamp), _("%a %d %b %Y, %H:%M\n"), tm);

	utf8 = g_locale_to_utf8 (stamp, -1, NULL, NULL, NULL);

	gtk_text_buffer_get_end_iter (data->note_buffer, &end);

	if (!gtk_text_iter_starts_line (&end)) {
		gtk_text_buffer_insert (data->note_buffer, &end, "\n", 1);
		gtk_text_buffer_get_end_iter (data->note_buffer, &end);
	}

	gtk_text_buffer_insert (data->note_buffer, &end, utf8, -11);

	g_free (utf8);

	gtk_text_buffer_get_end_iter (data->note_buffer, &end);

	mark = gtk_text_buffer_create_mark (data->note_buffer,
					    NULL,
					    &end,
					    TRUE);

	gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (data->note_textview), mark);
}

static void
task_dialog_task_child_added_or_removed_cb (MrpTask   *task,
					    GtkWidget *dialog)
{
	DialogData *data;

	data = DIALOG_GET_DATA (dialog);

	task_dialog_update_sensitivity (data);
}

static GtkWidget *
task_dialog_predecessor_dialog_new (MrpTask       *task,
				    PlannerWindow *main_window)
{
	MrpProject		*project;
	GladeXML		*glade;
	GtkWidget		*dialog;
	GtkWidget  		*w;
	GList      		*tasks;
	gchar      		*filename;
	GList			*relations, *l;
	MrpRelationType		 rel_type;
	MrpConstraint		 constraint;

	/* check for attempt to add relation when Must Start On constraint is present */
	constraint = imrp_task_get_constraint (task);
	if (constraint.type == MRP_CONSTRAINT_MSO) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 "You cannot add a relationship to a task with a Must Start On constraint.");

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return NULL;
	}


	/* check for attempt to add relation when SF or FF already present */
	relations = mrp_task_get_predecessor_relations (task);
	for (l = relations; l; l = l->next) {

		rel_type = mrp_relation_get_relation_type (l->data);
		if (rel_type == MRP_RELATION_SF || rel_type == MRP_RELATION_FF) {
 			dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
				     			 "You cannot add a relationship if a Start to Finish or Finish to Finish relationship already exists.");

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return NULL;
		}
	}


	mrp_object_get (task, "project", &project, NULL);

	filename = mrp_paths_get_glade_dir ("add-predecessor.glade");
	glade = glade_xml_new (filename, NULL, NULL);
	g_free (filename);

	dialog = glade_xml_get_widget (glade, "add_predecessor_dialog");

	g_object_set_data (G_OBJECT (dialog), "task_main", task);
	g_object_set_data (G_OBJECT (dialog), "main_window", main_window);
	w = glade_xml_get_widget (glade, "predecessor_combo");
	g_object_set_data (G_OBJECT (dialog), "predecessor_combo", w);

	tasks = mrp_project_get_all_tasks (project);
	tasks = g_list_remove (tasks, task);
	task_dialog_setup_task_combo (GTK_COMBO_BOX_ENTRY (w), tasks);
	g_list_free (tasks);

	w = glade_xml_get_widget (glade, "type_optionmenu");
	g_object_set_data (G_OBJECT (dialog), "type_optionmenu", w);

	task_dialog_setup_option_menu (w,
				       NULL,
				       NULL,
				       _("Finish to start (FS)"), MRP_RELATION_FS,
				       _("Finish to finish (FF)"), MRP_RELATION_FF,
				       _("Start to start (SS)"), MRP_RELATION_SS,
				       _("Start to finish (SF)"), MRP_RELATION_SF,
				       NULL);

	w = glade_xml_get_widget (glade, "lag_entry");
	g_object_set_data (G_OBJECT (dialog), "lag_entry", w);

	w = glade_xml_get_widget (glade, "cancel_button");
	g_signal_connect (w,
			  "clicked",
			  G_CALLBACK (task_dialog_new_pred_cancel_clicked_cb),
			  dialog);

	w = glade_xml_get_widget (glade, "ok_button");
	g_signal_connect (w,
			  "clicked",
			  G_CALLBACK (task_dialog_new_pred_ok_clicked_cb),
			  dialog);

	return dialog;
}

static void
task_dialog_new_pred_ok_clicked_cb (GtkWidget *button,
				    GtkWidget *dialog)
{
	PlannerWindow *main_window;
	PlannerCmd    *cmd;
	GtkWidget     *w;
	GError        *error = NULL;
	MrpTask       *task_main;
	MrpTask       *new_task_pred = NULL;
	MrpProject    *project;
	gint           lag;
	gint           pred_type;
	GtkTreeIter    iter;
	const gchar   *str;

	main_window = g_object_get_data (G_OBJECT (dialog), "main_window");

	task_main = g_object_get_data (G_OBJECT (dialog), "task_main");
	mrp_object_get (task_main, "project", &project, NULL);

	/* Predecessor lag. */
	w = g_object_get_data (G_OBJECT (dialog), "lag_entry");
	str = gtk_entry_get_text (GTK_ENTRY (w));
	lag = planner_parse_duration_with_day_length (str, 24*60*60);

	/* Predecessor type. */
	w = g_object_get_data (G_OBJECT(dialog), "type_optionmenu");
	pred_type = task_dialog_option_menu_get_selected (w);

	/* Predecessor task. */
	w = g_object_get_data (G_OBJECT (dialog), "predecessor_combo");

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (w), &iter)) {
		gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (w)),
				    &iter, 1, &new_task_pred, -1);
	}

	if (new_task_pred == NULL) {
		g_warning (_("Can't add new predecessor. No task selected!"));
                return;
        }

	cmd = planner_task_cmd_link (main_window, new_task_pred, task_main,
				     pred_type, lag, &error);

	if (!cmd) {
		GtkWidget *err_dialog;

		err_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog),
						     GTK_DIALOG_DESTROY_WITH_PARENT,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_OK,
						     "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (err_dialog));
		gtk_widget_destroy (err_dialog);

		g_error_free (error);
	} else {
		gtk_widget_destroy (dialog);
	}
}

static void
task_dialog_new_pred_cancel_clicked_cb (GtkWidget *w,
					GtkWidget *dialog)
{
	gtk_widget_destroy (dialog);
}

static void
task_dialog_add_predecessor_cb (GtkWidget  *widget,
				DialogData *data)
{
	GtkWidget *dialog;

	dialog = task_dialog_predecessor_dialog_new (data->task, data->main_window);
	if (dialog) {
		gtk_widget_show (dialog);
	}
}

static void
task_dialog_remove_predecessor_cb (GtkWidget  *widget,
				   DialogData *data)
{
	GtkTreeView             *tree;
	MrpTask                 *predecessor;
	MrpRelation             *relation;
	PlannerPredecessorModel *model;
	GtkTreeSelection        *selection;
	GtkTreeIter              iter;

	tree = GTK_TREE_VIEW (data->predecessor_list);
	model = PLANNER_PREDECESSOR_MODEL (gtk_tree_view_get_model (tree));

	selection = gtk_tree_view_get_selection (tree);
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                return;
        }

	predecessor = MRP_TASK (planner_list_model_get_object (PLANNER_LIST_MODEL (model), &iter));
	relation = mrp_task_get_relation (data->task, predecessor);
	planner_task_cmd_unlink (data->main_window, relation);
}

static void
task_dialog_resource_units_cell_edited (GtkCellRendererText *cell,
					gchar               *path_str,
					gchar               *new_text,
					DialogData          *data)
{
	GtkTreeView   *tree;
	GtkTreeModel  *model;
	GtkTreePath   *path;
	GtkTreeIter    iter;
	MrpResource   *resource;
	MrpAssignment *assignment;
	int            value;

	tree = GTK_TREE_VIEW (data->resource_list);

	model = gtk_tree_view_get_model (tree);

	path = gtk_tree_path_new_from_string (path_str);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	resource = ((GList *)iter.user_data)->data;

	assignment = mrp_task_get_assignment (data->task, resource);
	if (assignment) {
		value = atoi (new_text);

		if (value != mrp_assignment_get_units (assignment)) {
			task_cmd_assign_units (data->main_window, assignment, atoi (new_text));
		}
	}
}

static void
task_dialog_pred_cell_edited (GtkCellRendererText *cell,
			      gchar               *path_str,
			      gchar               *new_text,
			      DialogData          *data)
{
	GtkTreeView             *tree;
	GtkTreePath             *path;
	GtkTreeIter              iter;
	GtkTreeModel            *model;
	MrpRelation             *relation;
	MrpTask                 *task_main;
	MrpTask                 *task_pred;
	PlannerCellRendererList *planner_cell;
	gint                     column;
	gint                     lag;
	MrpRelationType          type, new_type;

	tree = GTK_TREE_VIEW (data->predecessor_list);

	model = gtk_tree_view_get_model (tree);
	path = gtk_tree_path_new_from_string (path_str);
	column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

	gtk_tree_model_get_iter (model, &iter, path);

	task_pred = MRP_TASK (planner_list_model_get_object (PLANNER_LIST_MODEL (model),
							     &iter));
	task_main = data->task;

	relation = mrp_task_get_relation (task_main, task_pred);
	lag = mrp_relation_get_lag (relation) / (60*60);
	type = mrp_relation_get_relation_type (relation);

	switch (column) {
	case PREDECESSOR_COL_NAME:
		g_assert_not_reached ();
		break;

	case PREDECESSOR_COL_TYPE: {
		GError     *error = NULL;
		PlannerCmd *cmd;

		planner_cell = PLANNER_CELL_RENDERER_LIST (cell);

		new_type = cell_index_to_relation_type (planner_cell->selected_index);

		cmd = planner_task_cmd_edit_predecessor (data->main_window,
							 task_main, task_pred,
							 task_pred,
							 new_type,
							 lag, &error);

		if (!cmd) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (
				NULL,
				GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s", error->message);

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			g_error_free (error);

			/* Restore the previous state. */
			mrp_task_add_predecessor (task_main,
						  task_pred,
						  type,
						  lag,
						  NULL);
		}
		break;
	}
	case PREDECESSOR_COL_LAG:
		lag = planner_parse_duration_with_day_length (new_text, 24*60*60);
		task_cmd_edit_lag (data->main_window, relation, lag);
		break;

	default:
		g_assert_not_reached ();
		break;
	}

	gtk_tree_path_free (path);
}

static MrpRelationType
cell_index_to_relation_type (gint i)
{
	switch (i) {
	case 0:
		return MRP_RELATION_FS;
	case 1:
		return MRP_RELATION_FF;
	case 2:
		return MRP_RELATION_SS;
	case 3:
		return MRP_RELATION_SF;
	default:
		g_warning ("Unknown relation type index");
		return MRP_RELATION_FS;
	}
}

static void
task_dialog_cell_type_show_popup (PlannerCellRendererList *cell,
				  const gchar        *path_string,
				  gint                x1,
				  gint                y1,
				  gint                x2,
				  gint                y2,
				  DialogData         *data)
{
	GtkTreeView       *tree;
	GtkTreeModel      *model;
	PlannerListModel  *list_model;
	GtkTreeIter        iter;
	GtkTreePath       *path;
	MrpTask           *predecessor;
	MrpRelation       *relation;
	GList             *list;

	tree = GTK_TREE_VIEW (data->predecessor_list);
	model = gtk_tree_view_get_model (tree);
	list_model = PLANNER_LIST_MODEL (model);

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (model, &iter, path);

	predecessor = MRP_TASK (planner_list_model_get_object (list_model, &iter));

	relation = mrp_task_get_relation (data->task, predecessor);

	list = NULL;
	list = g_list_append (list, g_strdup (_("FS")));
	list = g_list_append (list, g_strdup (_("FF")));
	list = g_list_append (list, g_strdup (_("SS")));
	list = g_list_append (list, g_strdup (_("SF")));

	cell->list = list;

	switch (mrp_relation_get_relation_type (relation)) {
	case MRP_RELATION_FS:
		cell->selected_index = 0;
		break;
	case MRP_RELATION_FF:
		cell->selected_index = 1;
		break;
	case MRP_RELATION_SS:
		cell->selected_index = 2;
		break;
	case MRP_RELATION_SF:
		cell->selected_index = 3;
		break;
	default:
		cell->selected_index = 0;
		break;
	}
}

static void
task_dialog_cell_type_hide_popup (PlannerCellRendererList *cell,
				  GtkWidget          *view)
{
       GList *l;

       for (l = cell->user_data; l; l = l->next) {
               if (l->data) {
                       g_object_unref (l->data);
               }
       }

       g_list_free (cell->user_data);
       /* cell->user_data = NULL; */
}

static void
task_dialog_schedule_date_selected_cb (GtkWidget          *button,
				       PlannerPopupButton *popup_button)
{
	planner_popup_button_popdown (popup_button, TRUE);
}

static void
task_dialog_schedule_cancelled_cb (GtkWidget          *button,
				   PlannerPopupButton *popup_button)
{
	planner_popup_button_popdown (popup_button, FALSE);
}

static GtkWidget *
task_dialog_schedule_popup_cb (PlannerPopupButton *popup_button,
			       DialogData         *data)
{
	GtkWidget     *widget;
	MrpConstraint *constraint;

	widget = planner_task_date_widget_new ();

	g_object_get (data->task, "constraint", &constraint, NULL);

	planner_task_date_widget_set_constraint_type (PLANNER_TASK_DATE_WIDGET (widget),
						      constraint->type);

	if (constraint->type != MRP_CONSTRAINT_ASAP) {
		planner_task_date_widget_set_date (PLANNER_TASK_DATE_WIDGET (widget),
						   constraint->time);
	}

	g_free (constraint);

	g_signal_connect (widget,
			  "date_selected",
			  G_CALLBACK (task_dialog_schedule_date_selected_cb),
			  popup_button);
	g_signal_connect (widget,
			  "cancelled",
			  G_CALLBACK (task_dialog_schedule_cancelled_cb),
			  popup_button);

	return widget;
}

static void
task_dialog_schedule_popdown_cb (PlannerPopupButton *popup_button,
				 GtkWidget          *widget,
				 gboolean            ok,
				 DialogData         *data)
{
	MrpConstraint	 constraint;
	GList		*relations, *l;
	GtkWidget	*dialog;
	MrpRelationType  rel_type;

	if (ok) {
		constraint.time = planner_task_date_widget_get_date (PLANNER_TASK_DATE_WIDGET (widget));
		constraint.type = planner_task_date_widget_get_constraint_type (PLANNER_TASK_DATE_WIDGET (widget));

		relations = mrp_task_get_predecessor_relations (data->task);

		/* check for attempt to add MSO constraint when relations are present */
		if (constraint.type == MRP_CONSTRAINT_MSO && relations) {

			dialog = gtk_message_dialog_new (GTK_WINDOW (data->main_window),
							 GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							 "You cannot add a Must Start On constraint with predecessor relations defined for this task.");

			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			gtk_widget_destroy (widget);

			return;
		}

		/* check for attempt to add MSO constraint when relations are present */
		if (constraint.type == MRP_CONSTRAINT_SNET && relations) {
			for (l = relations; l; l = l->next) {

				rel_type = mrp_relation_get_relation_type (l->data);
				if (rel_type == MRP_RELATION_SF || rel_type == MRP_RELATION_FF) {
					dialog = gtk_message_dialog_new (GTK_WINDOW (data->main_window),
									 GTK_DIALOG_DESTROY_WITH_PARENT,
									 GTK_MESSAGE_ERROR,
									 GTK_BUTTONS_CLOSE,
									 "You cannot add a Start No Earlier Than constraint because a Start to Finish or Finish to Finish predecessor relationship exists for this task.");

					gtk_dialog_run (GTK_DIALOG (dialog));
					gtk_widget_destroy (dialog);
					gtk_widget_destroy (widget);

					return;
				}
			}
		}

		task_cmd_edit_constraint (data->main_window,
					  data->task,
					  &constraint);
	}

	gtk_widget_destroy (widget);
}

static void
task_dialog_setup_widgets (DialogData *data,
			   GladeXML   *glade)
{
	GtkWidget    *w;
	gchar        *name;
	MrpTaskType   type;
	MrpTaskSched  sched;
	gchar        *note;
	gint          int_value;
	gchar        *str;
	GtkWidget    *hbox;

	w = glade_xml_get_widget (glade, "close_button");
	g_signal_connect (w,
			  "clicked",
			  G_CALLBACK (task_dialog_close_clicked_cb),
			  data);

	data->name_entry = glade_xml_get_widget (glade, "name_entry");

	g_object_get (data->task, "name", &name, NULL);
	if (name) {
		gtk_entry_set_text (GTK_ENTRY (data->name_entry), name);
		g_free (name);
	}

	g_signal_connect (data->name_entry,
			  "changed",
			  G_CALLBACK (task_dialog_name_changed_cb),
			  data);

	g_signal_connect (data->name_entry,
			  "focus_in_event",
			  G_CALLBACK (task_dialog_name_focus_in_cb),
			  data);

	g_signal_connect (data->name_entry,
			  "focus_out_event",
			  G_CALLBACK (task_dialog_name_focus_out_cb),
			  data);

	data->milestone_checkbutton = glade_xml_get_widget (glade, "milestone_checkbutton");
	g_object_get (data->task, "type", &type, NULL);
	if (type == MRP_TASK_TYPE_MILESTONE) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data->milestone_checkbutton), TRUE);
	}
	g_signal_connect (data->milestone_checkbutton,
			  "toggled",
			  G_CALLBACK (task_dialog_type_toggled_cb),
			  data);

	data->fixed_checkbutton = glade_xml_get_widget (glade, "fixed_checkbutton");
	g_object_get (data->task, "sched", &sched, NULL);
	if (sched == MRP_TASK_SCHED_FIXED_DURATION) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (data->fixed_checkbutton), TRUE);
	}
	g_signal_connect (data->fixed_checkbutton,
			  "toggled",
			  G_CALLBACK (task_dialog_fixed_toggled_cb),
			  data);

	data->work_entry = glade_xml_get_widget (glade, "work_entry");
	g_object_get (data->task, "work", &int_value, NULL);
	str = planner_format_duration (mrp_object_get_project (MRP_OBJECT (data->task)), int_value);
	gtk_entry_set_text (GTK_ENTRY (data->work_entry), str);
	g_free (str);
	g_signal_connect (data->work_entry,
			  "focus_in_event",
			  G_CALLBACK (task_dialog_work_focus_in_cb),
			  data);
	g_signal_connect (data->work_entry,
			  "focus_out_event",
			  G_CALLBACK (task_dialog_work_focus_out_cb),
			  data);

	data->duration_entry = glade_xml_get_widget (glade, "duration_entry");
	g_object_get (data->task, "duration", &int_value, NULL);
	str = planner_format_duration (mrp_object_get_project (MRP_OBJECT (data->task)), int_value);
	gtk_entry_set_text (GTK_ENTRY (data->duration_entry), str);
	g_free (str);
	g_signal_connect (data->duration_entry,
			  "focus_in_event",
			  G_CALLBACK (task_dialog_duration_focus_in_cb),
			  data);
	g_signal_connect (data->duration_entry,
			  "focus_out_event",
			  G_CALLBACK (task_dialog_duration_focus_out_cb),
			  data);

	/* Schedule. */
	data->schedule_label = glade_xml_get_widget (glade, "schedule_label");
	task_dialog_update_schedule_label (data);

	hbox = glade_xml_get_widget (glade, "schedule_hbox");
	data->schedule_button = planner_popup_button_new (_("Change..."));
	gtk_widget_show (data->schedule_button);
	gtk_box_pack_start (GTK_BOX (hbox), data->schedule_button, FALSE, FALSE, 0);

	g_signal_connect (data->schedule_button,
			  "popup",
			  G_CALLBACK (task_dialog_schedule_popup_cb),
			  data);

	g_signal_connect (data->schedule_button,
			  "popdown",
			  G_CALLBACK (task_dialog_schedule_popdown_cb),
			  data);

	data->complete_spinbutton = glade_xml_get_widget (glade, "complete_spinbutton");
	g_object_get (data->task, "percent_complete", &int_value, NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (data->complete_spinbutton), int_value);
	g_signal_connect (data->complete_spinbutton,
			  "value_changed",
			  G_CALLBACK (task_dialog_complete_changed_cb),
			  data);
	g_signal_connect (data->complete_spinbutton,
			  "focus_in_event",
			  G_CALLBACK (task_dialog_complete_focus_in_cb),
			  data);
	g_signal_connect (data->complete_spinbutton,
			  "focus_out_event",
			  G_CALLBACK (task_dialog_complete_focus_out_cb),
			  data);

	data->priority_spinbutton = glade_xml_get_widget (glade, "priority_spinbutton");
	g_object_get (data->task, "priority", &int_value, NULL);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (data->priority_spinbutton), int_value);
	g_signal_connect (data->priority_spinbutton,
			  "value_changed",
			  G_CALLBACK (task_dialog_priority_changed_cb),
			  data);
	g_signal_connect (data->priority_spinbutton,
			  "focus_in_event",
			  G_CALLBACK (task_dialog_priority_focus_in_cb),
			  data);
	g_signal_connect (data->priority_spinbutton,
			  "focus_out_event",
			  G_CALLBACK (task_dialog_priority_focus_out_cb),
			  data);

	data->note_textview = glade_xml_get_widget (glade, "note_textview");

	data->note_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (data->note_textview));

	g_object_get (data->task, "note", &note, NULL);
	if (note) {
		gtk_text_buffer_set_text (data->note_buffer, note, -1);
		g_free (note);
	}

	g_signal_connect (data->note_buffer,
			  "changed",
			  G_CALLBACK (task_dialog_note_changed_cb),
			  data);
	g_signal_connect (data->note_textview,
			  "focus_out_event",
			  G_CALLBACK (task_dialog_note_focus_out_cb),
			  data);

	g_signal_connect (data->note_textview,
			  "focus_in_event",
			  G_CALLBACK (task_dialog_note_focus_in_cb),
			  data);

	w = glade_xml_get_widget (glade, "stamp_button");
	g_signal_connect (w,
			  "clicked",
			  G_CALLBACK (task_dialog_note_stamp_cb),
			  data);
}

static void
task_dialog_assignment_toggled_cb (GtkCellRendererText *cell,
				   gchar               *path_str,
				   DialogData          *data)
{
	GtkTreeView  *tree;
	GtkTreeModel *model;
	GtkTreePath  *path;
	GtkTreeIter   iter;
	MrpResource  *resource;
	gboolean      active;

	tree = GTK_TREE_VIEW (data->resource_list);

	g_object_get (cell, "active", &active, NULL);

	model = gtk_tree_view_get_model (tree);

	path = gtk_tree_path_new_from_string (path_str);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_path_free (path);

	resource = ((GList *)iter.user_data)->data;

 	if (!active) {
		task_cmd_assign_add (data->main_window, data->task, resource, 100);
	} else {
		MrpAssignment *assignment;

		assignment = mrp_task_get_assignment (data->task, resource);

		if (assignment) {
			task_cmd_assign_remove (data->main_window, assignment);
		}
	}
}

static void
task_dialog_lag_data_func (GtkTreeViewColumn *tree_column,
			   GtkCellRenderer   *cell,
			   GtkTreeModel      *tree_model,
			   GtkTreeIter       *iter,
			   DialogData        *data)
{
	gint   lag;
	gchar *ret;

	gtk_tree_model_get (tree_model,
			    iter,
			    PREDECESSOR_COL_LAG,
			    &lag,
			    -1);

	ret = planner_format_duration_with_day_length (lag, 24*60*60);
	g_object_set (cell, "text", ret, NULL);
	g_free (ret);
}

static void
task_dialog_setup_resource_list (DialogData *data)
{
	GtkTreeView       *tree = GTK_TREE_VIEW (data->resource_list);
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkTreeModel      *model;

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (tree),
				     GTK_SELECTION_SINGLE);

	/* Name */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "editable", FALSE, NULL);

	col = gtk_tree_view_column_new_with_attributes (
		_("Name"),
		cell,
		"text", RESOURCE_ASSIGNMENT_COL_NAME,
		NULL);

	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_min_width (col, 200);
	gtk_tree_view_append_column (tree, col);

	/* Assigned. */
	cell = gtk_cell_renderer_toggle_new ();
	g_object_set (cell,
		      "activatable", TRUE,
		      NULL);

	g_signal_connect (cell,
			  "toggled",
			  G_CALLBACK (task_dialog_assignment_toggled_cb),
			  data);

	col = gtk_tree_view_column_new_with_attributes (
		_("Assigned"),
		cell,
		"active", RESOURCE_ASSIGNMENT_COL_ASSIGNED,
		NULL);

	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_append_column (tree, col);

	/* Assigned units */
	cell = gtk_cell_renderer_text_new ();

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (task_dialog_resource_units_cell_edited),
			  data);

	col = gtk_tree_view_column_new_with_attributes (
		_("Units"),
		cell,
		"text", RESOURCE_ASSIGNMENT_COL_ASSIGNED_UNITS,
		"editable", RESOURCE_ASSIGNMENT_COL_ASSIGNED_UNITS_EDITABLE,
		NULL);

	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_min_width (col, 50);
	gtk_tree_view_append_column (tree, col);

	model = GTK_TREE_MODEL (planner_assignment_model_new (data->task));

	gtk_tree_view_set_model (tree, model);
}

static void
task_dialog_setup_predecessor_list (DialogData *data)
{
	GtkTreeView       *tree;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkTreeModel      *model;

	tree = GTK_TREE_VIEW (data->predecessor_list);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (tree),
				     GTK_SELECTION_SINGLE);

	/* Name */
	cell = planner_cell_renderer_list_new ();

	g_object_set (cell, "editable", FALSE, NULL);

	g_object_set_data (G_OBJECT (cell),
			   "column",
			   GINT_TO_POINTER (PREDECESSOR_COL_NAME));

	col = gtk_tree_view_column_new_with_attributes (_("Name"),
							cell,
							"text", PREDECESSOR_COL_NAME,
							NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_min_width (col, 200);
	gtk_tree_view_append_column (tree, col);

	/* Type */
	cell = planner_cell_renderer_list_new ();

	g_object_set (cell, "editable", TRUE, NULL);

	g_object_set_data (G_OBJECT (cell),
			   "column",
			   GINT_TO_POINTER (PREDECESSOR_COL_TYPE));

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (task_dialog_pred_cell_edited),
			  data);

	g_signal_connect (cell,
			  "show_popup",
			  G_CALLBACK (task_dialog_cell_type_show_popup),
			  data);

	g_signal_connect_after (cell,
				"hide_popup",
				G_CALLBACK (task_dialog_cell_type_hide_popup),
				data);

	col = gtk_tree_view_column_new_with_attributes (_("Type"),
							cell,
							"text", PREDECESSOR_COL_TYPE,
							NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_append_column (tree, col);

	/* Lag */
	cell = gtk_cell_renderer_text_new ();

	g_object_set (cell, "editable", TRUE, NULL);

	g_object_set_data (G_OBJECT (cell),
			   "column",
			   GINT_TO_POINTER (PREDECESSOR_COL_LAG));

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (task_dialog_pred_cell_edited),
			  data);

	col = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (col, _("Lag"));

	gtk_tree_view_column_pack_start (col,
					 cell,
					 TRUE);

	gtk_tree_view_column_set_cell_data_func (col,
						 cell,
						 (GtkTreeCellDataFunc) task_dialog_lag_data_func,
						 data,
						 NULL);

	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_append_column (tree, col);

	model = planner_predecessor_model_new (data->task);

	gtk_tree_view_set_model (tree, model);
}

static void
task_dialog_connect_to_task (DialogData *data)
{
	g_signal_connect_object (data->task,
				 "notify::name",
				 G_CALLBACK (task_dialog_task_name_changed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "notify::type",
				 G_CALLBACK (task_dialog_task_type_changed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "notify::sched",
				 G_CALLBACK (task_dialog_task_sched_changed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "notify::note",
				 G_CALLBACK (task_dialog_task_note_changed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "notify::work",
				 G_CALLBACK (task_dialog_task_work_changed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "notify::duration",
				 G_CALLBACK (task_dialog_task_duration_changed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "notify::percent-complete",
				 G_CALLBACK (task_dialog_task_complete_changed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "notify::priority",
				 G_CALLBACK (task_dialog_task_priority_changed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "notify::constraint",
				 G_CALLBACK (task_dialog_task_constraint_changed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "child_added",
				 G_CALLBACK (task_dialog_task_child_added_or_removed_cb),
				 data->dialog,
				 0);

	g_signal_connect_object (data->task,
				 "child_removed",
				 G_CALLBACK (task_dialog_task_child_added_or_removed_cb),
				 data->dialog,
				 0);
}

static void
task_dialog_calendar_changed_cb (MrpCalendar *calendar,
				 GtkWidget   *dialog)
{
	DialogData *data;

	data = DIALOG_GET_DATA (dialog);

	/* We want the work field to update according to the potentially new day
	 * length.
	 */
	task_dialog_task_work_changed_cb (data->task,
					  NULL,
					  dialog);

	task_dialog_task_duration_changed_cb (data->task,
					      NULL,
					      dialog);
}

static void
task_dialog_update_sensitivity (DialogData *data)
{
	MrpTaskType  type;
	MrpTaskSched sched;
	gboolean     leaf, milestone, fixed;

	leaf = (mrp_task_get_n_children (data->task) == 0);

	type = mrp_task_get_task_type (data->task);
	milestone = (type == MRP_TASK_TYPE_MILESTONE);

	sched = mrp_task_get_sched (data->task);
	fixed = (sched == MRP_TASK_SCHED_FIXED_DURATION);

	gtk_widget_set_sensitive (data->milestone_checkbutton, leaf);
	gtk_widget_set_sensitive (data->schedule_button, leaf);

	gtk_widget_set_sensitive (data->fixed_checkbutton, leaf && !milestone);

	gtk_widget_set_sensitive (data->duration_entry, leaf && !milestone && fixed);
	gtk_widget_set_sensitive (data->work_entry, leaf && !milestone);
}

static void
task_dialog_update_title (DialogData *data)
{
	const gchar *name;
	gchar       *title;

	name = mrp_task_get_name (data->task);

	if (!name || strlen (name) == 0) {
		title = g_strdup (_("Edit task properties"));
	} else {
		title = g_strconcat (name, " - ", _("Edit task properties"), NULL);
	}
	gtk_window_set_title (GTK_WINDOW (data->dialog), title);
	g_free (title);
}

static void
task_dialog_parent_destroy_cb (GtkWidget *parent,
			       GtkWidget *dialog)
{
	gtk_widget_destroy (dialog);
}

static void
task_dialog_destroy_cb (GtkWidget  *parent,
			DialogData *data)
{
	g_hash_table_remove (dialogs, data->task);
}

GtkWidget *
planner_task_dialog_new (PlannerWindow *window,
			 MrpTask       *task,
			 PlannerTaskDialogPage  page)
{
	DialogData   *data;
	GladeXML     *glade;
	GtkWidget    *dialog;
	GtkWidget    *w;
	GtkSizeGroup *size_group;
	MrpProject   *project;
	MrpCalendar  *calendar;
	gchar        *filename;

	g_return_val_if_fail (MRP_IS_TASK (task), NULL);

	if (!dialogs) {
		dialogs = g_hash_table_new (NULL, NULL);
	}

	dialog = g_hash_table_lookup (dialogs, task);
	if (dialog) {
		gtk_window_present (GTK_WINDOW (dialog));
		return dialog;
	}

	filename = mrp_paths_get_glade_dir ("task-dialog.glade");
	glade = glade_xml_new (filename, NULL, NULL);
	g_free (filename);

	if (!glade) {
		g_warning ("Could not create task dialog.");
		return NULL;
	}

	dialog = glade_xml_get_widget (glade, "task_dialog");

	data = g_new0 (DialogData, 1);

	data->main_window = window;
	data->dialog = dialog;
	data->task = task;

	g_hash_table_insert (dialogs, task, dialog);

	g_signal_connect (dialog,
			  "destroy",
			  G_CALLBACK (task_dialog_destroy_cb),
			  data);

	g_signal_connect_object (window,
				 "destroy",
				 G_CALLBACK (task_dialog_parent_destroy_cb),
				 dialog,
				 0);

	g_signal_connect_object (task,
				 "removed",
				 G_CALLBACK (task_dialog_task_removed_cb),
				 dialog,
				 0);

	w = glade_xml_get_widget (glade, "task_notebook");

	gtk_notebook_set_current_page (GTK_NOTEBOOK (w), page);

	data->resource_list = glade_xml_get_widget (glade, "resource_list");
	task_dialog_setup_resource_list (data);

	data->predecessor_list = glade_xml_get_widget (glade, "predecessor_list");
	task_dialog_setup_predecessor_list (data);

	w = glade_xml_get_widget (glade, "add_predecessor_button");
	g_signal_connect (w,
			  "clicked",
			  G_CALLBACK (task_dialog_add_predecessor_cb),
			  data);

	w = glade_xml_get_widget (glade, "remove_predecessor_button");
	g_signal_connect (w,
			  "clicked",
			  G_CALLBACK (task_dialog_remove_predecessor_cb),
			  data);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

	w = glade_xml_get_widget (glade, "name_pad");
	gtk_size_group_add_widget (size_group, w);

	w = glade_xml_get_widget (glade, "milestone_pad");
	gtk_size_group_add_widget (size_group, w);

	w = glade_xml_get_widget (glade, "complete_pad");
	gtk_size_group_add_widget (size_group, w);

	w = glade_xml_get_widget (glade, "priority_pad");
	gtk_size_group_add_widget (size_group, w);

	g_object_unref (size_group);

	g_object_set_data_full (G_OBJECT (dialog),
				"data", data,
				g_free);

	task_dialog_setup_widgets (data, glade);

	task_dialog_update_sensitivity (data);

	task_dialog_update_title (data);

	task_dialog_connect_to_task (data);

	g_object_get (task, "project", &project, NULL);
	calendar = mrp_project_get_calendar (project);

	g_signal_connect_object (calendar,
				 "calendar-changed",
				 G_CALLBACK (task_dialog_calendar_changed_cb),
				 dialog,
				 0);

	return dialog;
}
