/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2005 Imendio AB
 * Copyright (C) 2002      CodeFactory AB
 * Copyright (C) 2002      Richard Hult <richard@imendio.com>
 * Copyright (C) 2002      Mikael Hallendal <micke@imendio.com>
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
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libplanner/mrp-project.h>
#include "libplanner/mrp-paths.h"
#include "planner-resource-view.h"
#include "planner-cell-renderer-list.h"
#include "planner-group-dialog.h"
#include "planner-format.h"
#include "planner-resource-dialog.h"
#include "planner-resource-input-dialog.h"
#include "planner-table-print-sheet.h"
#include "planner-property-dialog.h"
#include "planner-resource-cmd.h"
#include "planner-column-dialog.h"

struct _PlannerResourceViewPriv {
	GtkItemFactory         *popup_factory;
	GtkTreeView            *tree_view;
	GHashTable             *property_to_column;

	GtkWidget              *group_dialog;
	GtkWidget              *resource_input_dialog;

	PlannerTablePrintSheet *print_sheet;

	GtkUIManager           *ui_manager;
	GtkActionGroup         *actions;
	guint                   merged_id;
};

typedef struct {
	PlannerView *view;
	MrpProperty *property;
} ColPropertyData;

enum {
	COL_RESOURCE,
	NUM_OF_COLS
};

static void           resource_view_finalize                 (GObject                 *object);
static void           resource_view_insert_resource_cb       (GtkAction               *action,
							      gpointer                 data);
static void           resource_view_insert_resources_cb      (GtkAction               *action,
							      gpointer                 data);
static void           resource_view_remove_resource_cb       (GtkAction               *action,
							      gpointer                 data);
static void           resource_view_edit_resource_cb         (GtkAction               *action,
							      gpointer                 data);
static void           resource_view_popup_insert_resource_cb (gpointer                 callback_data,
							      guint                    action,
							      GtkWidget               *widget);
static void           resource_view_popup_remove_resource_cb (gpointer                 callback_data,
							      guint                    action,
							      GtkWidget               *widget);
static void           resource_view_popup_edit_resource_cb   (gpointer                 callback_data,
							      guint                    action,
							      GtkWidget               *widget);
static void           resource_view_edit_columns_cb           (GtkAction              *action,
							       gpointer                data);
static void           resource_view_select_all_cb            (GtkAction               *action,
							      gpointer                 data);
static void           resource_view_edit_custom_props_cb     (GtkAction               *action,
							      gpointer                 data);
static void           resource_view_selection_changed_cb     (GtkTreeSelection        *selection,
							      PlannerView             *view);
static void           resource_view_group_dialog_closed      (GtkWidget               *widget,
							      gpointer                 data);
static void           resource_view_setup_tree_view          (PlannerView             *view);
static void           resource_view_cell_name_edited         (GtkCellRendererText     *cell,
							      gchar                   *path_string,
							      gchar                   *new_text,
							      gpointer                 user_data);
static void           resource_view_cell_short_name_edited   (GtkCellRendererText     *cell,
							      gchar                   *path_string,
							      gchar                   *new_text,
							      gpointer                 user_data);
static void           resource_view_cell_email_edited        (GtkCellRendererText     *cell,
							      gchar                   *path_string,
							      gchar                   *new_text,
							      gpointer                 user_data);
static void           resource_view_cell_type_edited         (PlannerCellRendererList *cell,
							      gchar                   *path_string,
							      gchar                   *new_text,
							      gpointer                 user_data);
static void           resource_view_cell_group_edited        (PlannerCellRendererList *cell,
							      gchar                   *path_string,
							      gchar                   *new_text,
							      gpointer                 user_data);
static void           resource_view_cell_cost_edited         (GtkCellRendererText     *cell,
							      gchar                   *path_string,
							      gchar                   *new_text,
							      gpointer                 user_data);
static void           resource_view_property_value_edited    (GtkCellRendererText     *cell,
							      gchar                   *path_string,
							      gchar                   *new_text,
							      ColPropertyData         *data);
static void           resource_view_cell_type_show_popup     (PlannerCellRendererList *cell,
							      const gchar             *path_string,
							      gint                     x1,
							      gint                     y1,
							      gint                     x2,
							      gint                     y2,
							      PlannerView             *view);
static void           resource_view_cell_group_show_popup    (PlannerCellRendererList *cell,
							      const gchar             *path_string,
							      gint                     x1,
							      gint                     y1,
							      gint                     x2,
							      gint                     y2,
							      PlannerView             *view);
static void           resource_view_cell_group_hide_popup    (PlannerCellRendererList *cell,
							      PlannerView             *view);
static void           resource_view_edit_groups_cb           (GtkAction               *action,
							      gpointer                 data);
static void           resource_view_project_loaded_cb        (MrpProject              *project,
							      PlannerView             *view);
static GList *        resource_view_selection_get_list       (PlannerView             *view);
static void           resource_view_update_ui                (PlannerView             *view);
static void           resource_view_property_added           (MrpProject              *project,
							      GType                    object_type,
							      MrpProperty             *property,
							      PlannerView             *view);
static void           resource_view_property_removed         (MrpProject              *project,
							      MrpProperty             *property,
							      PlannerView             *view);
static void           resource_view_property_changed         (MrpProject              *project,
							      MrpProperty             *property,
							      PlannerView             *view);
static void           resource_view_name_data_func           (GtkTreeViewColumn       *tree_column,
							      GtkCellRenderer         *cell,
							      GtkTreeModel            *tree_model,
							      GtkTreeIter             *iter,
							      gpointer                 data);
static void           resource_view_short_name_data_func     (GtkTreeViewColumn       *tree_column,
							      GtkCellRenderer         *cell,
							      GtkTreeModel            *tree_model,
							      GtkTreeIter             *iter,
							      gpointer                 data);
static void           resource_view_type_data_func           (GtkTreeViewColumn       *tree_column,
							      GtkCellRenderer         *cell,
							      GtkTreeModel            *tree_model,
							      GtkTreeIter             *iter,
							      gpointer                 data);
static void           resource_view_group_data_func          (GtkTreeViewColumn       *tree_column,
							      GtkCellRenderer         *cell,
							      GtkTreeModel            *tree_model,
							      GtkTreeIter             *iter,
							      gpointer                 data);
static void           resource_view_email_data_func          (GtkTreeViewColumn       *tree_column,
							      GtkCellRenderer         *cell,
							      GtkTreeModel            *tree_model,
							      GtkTreeIter             *iter,
							      gpointer                 data);
static void           resource_view_cost_data_func           (GtkTreeViewColumn       *tree_column,
							      GtkCellRenderer         *cell,
							      GtkTreeModel            *tree_model,
							      GtkTreeIter             *iter,
							      gpointer                 data);
static void           resource_view_popup_menu               (GtkWidget               *widget,
							      PlannerView             *view);
static void           resource_view_edit_resource_cb         (GtkAction               *action,
							      gpointer                 data);
static void           resource_view_property_data_func       (GtkTreeViewColumn       *tree_column,
							      GtkCellRenderer         *cell,
							      GtkTreeModel            *tree_model,
							      GtkTreeIter             *iter,
							      gpointer                 data);
static void           resource_view_activate                 (PlannerView             *view);
static void           resource_view_deactivate               (PlannerView             *view);
static void           resource_view_setup                    (PlannerView             *view,
							      PlannerWindow           *main_window);
static const gchar   *resource_view_get_label                (PlannerView             *view);
static const gchar   *resource_view_get_menu_label           (PlannerView             *view);
static const gchar   *resource_view_get_icon                 (PlannerView             *view);
static const gchar   *resource_view_get_name                 (PlannerView             *view);
static GtkWidget     *resource_view_get_widget               (PlannerView             *view);
static void           resource_view_print_init               (PlannerView             *view,
							      PlannerPrintJob         *job);
static void           resource_view_print                    (PlannerView             *view,
							      gint                     page_nr);
static gint           resource_view_print_get_n_pages        (PlannerView             *view);
static void           resource_view_print_cleanup            (PlannerView             *view);
static void           resource_view_resource_notify_cb       (MrpResource             *resource,
							      GParamSpec              *pspec,
							      PlannerView             *view);
static void           resource_view_resource_prop_changed_cb (MrpResource             *resource,
							      MrpProperty             *propert,
							      GValue                  *value,
							      PlannerView             *view);
static void           resource_view_resource_added_cb        (MrpProject              *project,
							      MrpResource             *resource,
							      PlannerView             *view);
static void           resource_view_resource_removed_cb      (MrpProject              *project,
							      MrpResource             *resource,
							      PlannerView             *view);
static const gchar *  resource_view_get_type_string          (MrpResourceType          type);
static void           resource_view_save_columns             (PlannerResourceView     *view);
static void           resource_view_load_columns             (PlannerResourceView     *view);

static PlannerViewClass *parent_class = NULL;

static const GtkActionEntry entries[] = {
	{ "InsertResource",   "planner-stock-insert-resource", N_("_Insert Resource"),
	  "<Control>i",        N_("Insert a new resource"),
	  G_CALLBACK (resource_view_insert_resource_cb) },
	{ "InsertResources",  "planner-stock-insert-resource", N_("In_sert Resources..."),
	  NULL,                NULL,
	  G_CALLBACK (resource_view_insert_resources_cb) },
	{ "RemoveResource",   "planner-stock-remove-resource", N_("_Remove Resource"),
	  "Delete",            N_("Remove the selected resource"),
	  G_CALLBACK (resource_view_remove_resource_cb) },
	{ "EditResource",     GTK_STOCK_PROPERTIES,            N_("_Edit Resource Properties..."),
	  "<Control>e",        NULL,
	  G_CALLBACK (resource_view_edit_resource_cb) },
	{ "EditGroups",       "planner-stock-edit-groups",     N_("Edit _Groups"),
	  NULL,                N_("Edit resource groups"),
	  G_CALLBACK (resource_view_edit_groups_cb) },
	{ "SelectAll",        NULL,                            N_("Select _All"),
	  NULL,                N_("Select all resources"),
	  G_CALLBACK (resource_view_select_all_cb) },
	{ "EditCustomProps",  GTK_STOCK_PROPERTIES,            N_("Edit _Custom Properties..."),
	  NULL,                NULL,
	  G_CALLBACK (resource_view_edit_custom_props_cb) },
	{ "EditColumns",     NULL,                           N_("Edit _Visible Columns"),
	  NULL,                N_("Edit visible columns"),
	  G_CALLBACK (resource_view_edit_columns_cb) }
};

enum {
	POPUP_NONE,
	POPUP_INSERT,
	POPUP_REMOVE,
	POPUP_EDIT
};

#define GIF_CB(x) ((GtkItemFactoryCallback)(x))

static GtkItemFactoryEntry popup_menu_items[] = {
	{ N_("/_Insert resource"),   NULL, GIF_CB (resource_view_popup_insert_resource_cb),
	  POPUP_INSERT,  "<Item>",       NULL
	},
	{ N_("/_Remove resource"),   NULL, GIF_CB (resource_view_popup_remove_resource_cb),
	  POPUP_REMOVE,  "<StockItem>",  GTK_STOCK_DELETE
	},
	{ N_("/_Edit resource..."),  NULL, GIF_CB (resource_view_popup_edit_resource_cb),
	  POPUP_EDIT,    "<Item>",       NULL }
};

/*
 * Commands
 */

typedef struct {
	PlannerCmd   base;

	MrpProject  *project;
	const gchar *name;
	MrpResource *resource;
	MrpGroup    *group;
} ResourceCmdInsert;

typedef struct {
	PlannerCmd   base;

	MrpProject  *project;
	MrpResource *resource;
	GList       *assignments;
} ResourceCmdRemove;

typedef struct {
	PlannerCmd   base;

	MrpResource *resource;
	gchar       *property;
	GValue      *value;
	GValue      *old_value;
} ResourceCmdEditProperty;

typedef struct {
	PlannerCmd    base;

	MrpResource  *resource;
	MrpProperty  *property;
	GValue       *value;
	GValue       *old_value;
} ResourceCmdEditCustomProperty;

G_DEFINE_TYPE (PlannerResourceView, planner_resource_view, PLANNER_TYPE_VIEW);


static void
planner_resource_view_class_init (PlannerResourceViewClass *klass)
{
	GObjectClass     *o_class;
	PlannerViewClass *view_class;

	parent_class = g_type_class_peek_parent (klass);

	o_class = (GObjectClass *) klass;
	view_class = PLANNER_VIEW_CLASS (klass);

	o_class->finalize = resource_view_finalize;

	view_class->setup = resource_view_setup;
	view_class->get_label = resource_view_get_label;
	view_class->get_menu_label = resource_view_get_menu_label;
	view_class->get_icon = resource_view_get_icon;
	view_class->get_name = resource_view_get_name;
	view_class->get_widget = resource_view_get_widget;
	view_class->activate = resource_view_activate;
	view_class->deactivate = resource_view_deactivate;
	view_class->print_init = resource_view_print_init;
	view_class->print_get_n_pages = resource_view_print_get_n_pages;
	view_class->print = resource_view_print;
	view_class->print_cleanup = resource_view_print_cleanup;
}

static void
planner_resource_view_init (PlannerResourceView *view)
{
	view->priv = g_new0 (PlannerResourceViewPriv, 1);
}

static void
resource_view_finalize (GObject *object)
{
	PlannerResourceView *view;

	view = PLANNER_RESOURCE_VIEW (object);

	if (PLANNER_VIEW (view)->activated) {
		resource_view_deactivate (PLANNER_VIEW (view));
	}

	if (view->priv->popup_factory) {
		g_object_unref (view->priv->popup_factory);
		view->priv->popup_factory = NULL;
	}

	if (view->priv->property_to_column) {
		g_hash_table_unref (view->priv->property_to_column);
		view->priv->property_to_column = NULL;
	}

	g_free (view->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize) {
		(*G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
resource_view_activate (PlannerView *view)
{
	PlannerResourceViewPriv *priv;
	gchar                   *filename;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	priv->actions = gtk_action_group_new ("ResourceView");
	gtk_action_group_set_translation_domain (priv->actions, GETTEXT_PACKAGE);

	gtk_action_group_add_actions (priv->actions, entries, G_N_ELEMENTS (entries), view);

	gtk_ui_manager_insert_action_group (priv->ui_manager, priv->actions, 0);
	filename = mrp_paths_get_ui_dir ("resource-view.ui");
	priv->merged_id = gtk_ui_manager_add_ui_from_file (priv->ui_manager,
							   filename,
							   NULL);
	g_free (filename);
	gtk_ui_manager_ensure_update (priv->ui_manager);

	/* Set the initial UI state. */
	resource_view_update_ui (view);

	gtk_widget_grab_focus (GTK_WIDGET (priv->tree_view));
}

static char *
resource_view_item_factory_trans (const char *path, gpointer data)
{
	return _((gchar*) path);
}

static void
resource_view_deactivate (PlannerView *view)
{
	PlannerResourceViewPriv *priv;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	gtk_ui_manager_remove_ui (priv->ui_manager, priv->merged_id);
	gtk_ui_manager_remove_action_group (priv->ui_manager, priv->actions);
	g_object_unref (priv->actions);
	priv->actions = NULL;
}

static void
resource_view_setup (PlannerView *view, PlannerWindow *main_window)
{
	PlannerResourceViewPriv *priv;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	priv->property_to_column = g_hash_table_new (NULL, NULL);

	priv->popup_factory = gtk_item_factory_new (GTK_TYPE_MENU,
						    "<main>",
						    NULL);

	gtk_item_factory_set_translate_func (priv->popup_factory,
					     resource_view_item_factory_trans,
					     NULL,
					     NULL);

	gtk_item_factory_create_items (priv->popup_factory,
				       G_N_ELEMENTS (popup_menu_items),
				       popup_menu_items,
				       view);

	priv->ui_manager = planner_window_get_ui_manager (main_window);
}

static const gchar *
resource_view_get_label (PlannerView *view)
{
	return _("Resources");
}

static const gchar *
resource_view_get_menu_label (PlannerView *view)
{
	return _("_Resources");
}

static const gchar *
resource_view_get_icon (PlannerView *view)
{
	static gchar *filename = NULL;

	if (!filename) {
		filename = mrp_paths_get_image_dir ("resources.png");
	}

	return filename;
}

static const gchar *
resource_view_get_name (PlannerView *view)
{
	return "resource_view";
}

static void
resource_view_tree_view_columns_changed_cb (GtkTreeView         *tree_view,
					    PlannerResourceView *view)
{
	resource_view_save_columns (view);
}

static void
resource_view_tree_view_destroy_cb (GtkTreeView         *tree_view,
				    PlannerResourceView *view)
{
	/* Block, we don't want to save the column configuration when they are
	 * removed by the destruction.
	 */
	g_signal_handlers_block_by_func (tree_view,
					 resource_view_tree_view_columns_changed_cb,
					 view);
}

/* Note: this is not ideal, it emits the signal as soon as the width is changed
 * during the resize. We should only emit it when the resizing is done.
 */
static void
resource_view_column_notify_width_cb (GtkWidget           *column,
				      GParamSpec          *spec,
				      PlannerResourceView *view)
{
	if (gtk_widget_get_realized (GTK_WIDGET (view->priv->tree_view))) {
		g_signal_emit_by_name (view->priv->tree_view, "columns-changed");
	}
}

static GtkWidget *
resource_view_get_widget (PlannerView *view)
{
	PlannerResourceViewPriv *priv;
	GtkWidget               *resource_table;
	GtkWidget               *sw;
	GtkWidget               *frame;
 	GtkTreeModel            *model;
	MrpProject              *project;
	GtkTreeSelection        *selection;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	project = planner_window_get_project (view->main_window);

	g_signal_connect (project,
			  "loaded",
			  G_CALLBACK (resource_view_project_loaded_cb),
			  view);

	g_signal_connect (project,
			  "property_added",
			  G_CALLBACK (resource_view_property_added),
			  view);

	g_signal_connect (project,
			  "property_removed",
			  G_CALLBACK (resource_view_property_removed),
			  view);

	g_signal_connect (project,
			  "property_changed",
			  G_CALLBACK (resource_view_property_changed),
			  view);

	g_signal_connect (project,
			  "resource_added",
			  G_CALLBACK (resource_view_resource_added_cb),
			  view);

	g_signal_connect (project,
			  "resource_removed",
			  G_CALLBACK (resource_view_resource_removed_cb),
			  view);

	model = GTK_TREE_MODEL (gtk_list_store_new (NUM_OF_COLS,
						    G_TYPE_POINTER));

	resource_table = gtk_tree_view_new_with_model (model);

	priv->tree_view = GTK_TREE_VIEW (resource_table);

	resource_view_setup_tree_view (view);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (resource_table));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (resource_view_selection_changed_cb),
			  view);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (sw), resource_table);
	gtk_container_add (GTK_CONTAINER (frame), sw);

	return frame;
}

static void
resource_view_print_init (PlannerView *view, PlannerPrintJob *job)
{
	PlannerResourceViewPriv *priv;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	priv->print_sheet = planner_table_print_sheet_new (view, job,
							   priv->tree_view);
}

static void
resource_view_print (PlannerView *view, gint page_nr)

{
	planner_table_print_sheet_output (PLANNER_RESOURCE_VIEW (view)->priv->print_sheet, page_nr);
}

static gint
resource_view_print_get_n_pages (PlannerView *view)
{
	return planner_table_print_sheet_get_n_pages (PLANNER_RESOURCE_VIEW (view)->priv->print_sheet);
}

static void
resource_view_print_cleanup (PlannerView *view)

{
	planner_table_print_sheet_free (PLANNER_RESOURCE_VIEW (view)->priv->print_sheet);
	PLANNER_RESOURCE_VIEW (view)->priv->print_sheet = NULL;
}

typedef struct {
	MrpResource *resource;
	GtkTreePath *found_path;
	GtkTreeIter *found_iter;
} FindResourceData;


static void
resource_view_free_find_resource_data (FindResourceData *data)
{
	if (data->found_path) {
		gtk_tree_path_free (data->found_path);
	}
	if (data->found_iter) {
		gtk_tree_iter_free (data->found_iter);
	}

	g_free (data);
}

static gboolean
resource_view_foreach_find_resource_func (GtkTreeModel     *model,
					  GtkTreePath      *path,
					  GtkTreeIter      *iter,
					  FindResourceData *data)
{
	MrpResource *resource;

	gtk_tree_model_get (model, iter,
			    COL_RESOURCE, &resource,
			    -1);

	if (resource == data->resource) {
		data->found_path = gtk_tree_path_copy (path);
		data->found_iter = gtk_tree_iter_copy (iter);
		return TRUE;
	}

	return FALSE;
}

static FindResourceData *
resource_view_find_resource (PlannerView *view, MrpResource *resource)
{
	FindResourceData *data;
	GtkTreeModel     *model;

	data = g_new0 (FindResourceData, 1);
	data->resource = resource;
	data->found_path = NULL;

	model = gtk_tree_view_get_model (PLANNER_RESOURCE_VIEW (view)->priv->tree_view);

	gtk_tree_model_foreach (model,
				(GtkTreeModelForeachFunc) resource_view_foreach_find_resource_func,
				data);

	if (data->found_path) {
		return data;
	}

	g_free (data);
	return NULL;
}

static void
resource_view_resource_notify_cb (MrpResource *resource,
				  GParamSpec  *pspec,
				  PlannerView *view)
{
	GtkTreeModel     *model;
 	FindResourceData *data;

	model = gtk_tree_view_get_model (PLANNER_RESOURCE_VIEW (view)->priv->tree_view);

	data = resource_view_find_resource (view, resource);

	if (data) {
		gtk_tree_model_row_changed (GTK_TREE_MODEL (model),
					    data->found_path,
					    data->found_iter);

		resource_view_free_find_resource_data (data);
	}
}

static void
resource_view_resource_prop_changed_cb (MrpResource *resource,
					MrpProperty *propert,
					GValue      *value,
					PlannerView *view)
{
	GtkTreeModel     *model;
 	FindResourceData *data;

	model = gtk_tree_view_get_model (PLANNER_RESOURCE_VIEW (view)->priv->tree_view);

	data = resource_view_find_resource (view, resource);

	if (data) {
		gtk_tree_model_row_changed (GTK_TREE_MODEL (model),
					    data->found_path,
					    data->found_iter);

		resource_view_free_find_resource_data (data);
	}
}

static void
resource_view_resource_added_cb (MrpProject  *project,
				 MrpResource *resource,
				 PlannerView *view)
{
	GtkTreeModel *model;
	GtkTreeIter   iter;

	model = gtk_tree_view_get_model (PLANNER_RESOURCE_VIEW (view)->priv->tree_view);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    COL_RESOURCE, g_object_ref (resource),
			    -1);

	g_signal_connect (resource, "notify",
			  G_CALLBACK (resource_view_resource_notify_cb),
			  view);
	g_signal_connect (resource, "prop_changed",
			  G_CALLBACK (resource_view_resource_prop_changed_cb),
			  view);
}

static void
resource_view_resource_removed_cb (MrpProject  *project,
				   MrpResource *resource,
				   PlannerView *view)
{
	GtkTreeModel     *model;
	FindResourceData *data;

	g_signal_handlers_disconnect_by_func (resource,
					      resource_view_resource_notify_cb,
					      view);
	g_signal_handlers_disconnect_by_func (resource,
					      resource_view_resource_prop_changed_cb,
					      view);

	model = gtk_tree_view_get_model (PLANNER_RESOURCE_VIEW (view)->priv->tree_view);

	data = resource_view_find_resource (view, resource);

	if (data) {
		gtk_widget_grab_focus (GTK_WIDGET (PLANNER_RESOURCE_VIEW (view)->priv->tree_view));
		gtk_list_store_remove (GTK_LIST_STORE (model),
				       data->found_iter);
		resource_view_free_find_resource_data (data);
	}
}

static const gchar *
resource_view_get_type_string (MrpResourceType type)
{
	switch (type) {
	case MRP_RESOURCE_TYPE_NONE:
		return "";
		break;
	case MRP_RESOURCE_TYPE_WORK:
		return _("Work");
		break;
	case MRP_RESOURCE_TYPE_MATERIAL:
		return _("Material");
		break;
	default:
		g_assert_not_reached ();
		return NULL;
	};
}

#if 0
static MrpResourceType
resource_view_get_type_enum (const gchar *type_str)
{
	gchar          *in_str   = g_utf8_casefold (type_str, -1);
	gchar          *work     = g_utf8_casefold (_("Work"), -1);
	gchar          *material = g_utf8_casefold (_("Material"), -1);
	MrpResourceType type;

	if (!g_utf8_collate (work, in_str)) {
		type = MRP_RESOURCE_TYPE_WORK;
	}
	else if (!g_utf8_collate (material, in_str)) {
		type = MRP_RESOURCE_TYPE_MATERIAL;
	} else {
		type = MRP_RESOURCE_TYPE_NONE;
	}

	g_free (in_str);
	g_free (work);
	g_free (material);

	return type;
}
#endif

/* Command callbacks. */

static void
resource_view_popup_insert_resource_cb  (gpointer   callback_data,
					 guint      action,
					 GtkWidget *widget)
{
	resource_view_insert_resource_cb (NULL, callback_data);
}

static void
resource_view_popup_remove_resource_cb   (gpointer   callback_data,
					  guint      action,
					  GtkWidget *widget)
{
	resource_view_remove_resource_cb (NULL, callback_data);
}

static void
resource_view_popup_edit_resource_cb     (gpointer   callback_data,
					  guint      action,
					  GtkWidget *widget)
{
	resource_view_edit_resource_cb (NULL, callback_data);
}

static void
resource_view_insert_resource_cb (GtkAction *action,
				  gpointer   data)
{
	PlannerView       *view;
	PlannerResourceViewPriv   *priv;
	GtkTreeModel      *model;
	FindResourceData  *find_data;
	GtkTreePath       *path;
	ResourceCmdInsert *cmd;

	view = PLANNER_VIEW (data);
	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	cmd = (ResourceCmdInsert*) planner_resource_cmd_insert (view->main_window, NULL);

	if (! gtk_widget_has_focus (GTK_WIDGET (priv->tree_view))) {
		gtk_widget_grab_focus (GTK_WIDGET (priv->tree_view));
	}

	find_data = resource_view_find_resource (view, cmd->resource);
	if (find_data) {
		model = gtk_tree_view_get_model (priv->tree_view);
		path = gtk_tree_model_get_path (model, find_data->found_iter);

		gtk_tree_view_set_cursor (priv->tree_view,
					  path,
					  gtk_tree_view_get_column (priv->tree_view, 0),
					  FALSE);

		gtk_tree_path_free (path);

		resource_view_free_find_resource_data (find_data);
	}
}

static void
resource_view_insert_resources_cb (GtkAction *action,
				   gpointer   data)
{
	PlannerView     *view;
	PlannerResourceViewPriv *priv;

	view = PLANNER_VIEW (data);
	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	/* We only want one of these dialogs at a time. */
	if (priv->resource_input_dialog) {
		gtk_window_present (GTK_WINDOW (priv->resource_input_dialog));
		return;
	}

	priv->resource_input_dialog = planner_resource_input_dialog_new (view->main_window);

	gtk_window_set_transient_for (GTK_WINDOW (priv->resource_input_dialog),
				      GTK_WINDOW (view->main_window));

	gtk_widget_show (priv->resource_input_dialog);

	g_object_add_weak_pointer (G_OBJECT (priv->resource_input_dialog),
				   (gpointer) &priv->resource_input_dialog);
}

static gboolean
resource_cmd_remove_do (PlannerCmd *cmd_base)
{
	ResourceCmdRemove *cmd;
	GList             *assignments, *l;

	cmd = (ResourceCmdRemove*) cmd_base;

	assignments = mrp_resource_get_assignments (cmd->resource);

	for (l = assignments; l; l = l->next) {
		cmd->assignments = g_list_append (cmd->assignments,
						  g_object_ref (l->data));
	}

	mrp_project_remove_resource (cmd->project, cmd->resource);

	return TRUE;
}

static void
resource_cmd_remove_undo (PlannerCmd *cmd_base)
{
	ResourceCmdRemove *cmd;
	GList             *l;

	cmd = (ResourceCmdRemove*) cmd_base;

	mrp_project_add_resource (cmd->project, cmd->resource);

	for (l = cmd->assignments; l; l = l->next) {
		MrpTask *task;
		guint    units;

		task = mrp_assignment_get_task (l->data);
		units = mrp_assignment_get_units (l->data);

		mrp_resource_assign (cmd->resource, task, units);
	}

	g_list_foreach (cmd->assignments, (GFunc) g_object_unref, NULL);
	g_list_free (cmd->assignments);
	cmd->assignments = NULL;
}

static void
resource_cmd_remove_free (PlannerCmd *cmd_base)
{
	ResourceCmdRemove *cmd;
	cmd = (ResourceCmdRemove*) cmd_base;

	g_list_free (cmd->assignments);

	g_object_unref (cmd->resource);
}

static PlannerCmd *
resource_cmd_remove (PlannerView *view,
		     MrpResource *resource)
{
	PlannerCmd          *cmd_base;
	ResourceCmdRemove   *cmd;

	cmd_base = planner_cmd_new (ResourceCmdRemove,
				    _("Remove resource"),
				    resource_cmd_remove_do,
				    resource_cmd_remove_undo,
				    resource_cmd_remove_free);

	cmd = (ResourceCmdRemove *) cmd_base;

	cmd->project = planner_window_get_project (view->main_window);
	cmd->resource = g_object_ref (resource);

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (view->main_window),
					   cmd_base);

	return cmd_base;
}

static void
resource_view_remove_resource_cb (GtkAction *action,
				  gpointer   data)
{
	PlannerView             *view;
	GList                   *list, *node;

	view = PLANNER_VIEW (data);

	list = resource_view_selection_get_list (view);

	for (node = list; node; node = node->next) {
		resource_cmd_remove (view, MRP_RESOURCE (node->data));
	}

	g_list_free (list);
}



static void
resource_view_edit_resource_cb (GtkAction *action,
				gpointer   data)
{
	PlannerView             *view;
	MrpResource             *resource;
	GtkWidget               *dialog;
	GList                   *list;

	view = PLANNER_VIEW (data);

	list = resource_view_selection_get_list (view);

	resource = MRP_RESOURCE (list->data);
	if (resource) {
		dialog = planner_resource_dialog_new (view->main_window, resource);
		gtk_widget_show (dialog);
	}

	g_list_free (list);
}

static void
resource_view_edit_columns_cb (GtkAction *action,
			       gpointer   data)
{
	PlannerView             *view;
	PlannerResourceViewPriv *priv;

	view = PLANNER_VIEW (data);
	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	planner_column_dialog_show (PLANNER_VIEW (view)->main_window,
				    _("Edit Resource Columns"),
				    GTK_TREE_VIEW (priv->tree_view));
}

static void
resource_view_select_all_cb (GtkAction *action,
			     gpointer   data)
{
	PlannerView             *view;
	PlannerResourceViewPriv *priv;
	GtkTreeSelection        *selection;

	view = PLANNER_VIEW (data);
	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	selection = gtk_tree_view_get_selection (priv->tree_view);

	gtk_tree_selection_select_all (selection);
}

static void
resource_view_edit_custom_props_cb (GtkAction *action,
				    gpointer   data)
{
	PlannerView *view;
	GtkWidget   *dialog;
	MrpProject  *project;

	view = PLANNER_VIEW (data);

	project = planner_window_get_project (view->main_window);

	dialog = planner_property_dialog_new (view->main_window,
						project,
					      MRP_TYPE_RESOURCE,
					      _("Edit custom resource properties"));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 300);
	gtk_widget_show (dialog);
}

static void
resource_view_update_ui (PlannerView *view)
{
	PlannerResourceViewPriv *priv;
	GList                   *list;
	gboolean                 value;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	list = resource_view_selection_get_list (view);
	value = (list != NULL);
	g_list_free (list);

	if (!view->activated) {
		return;
	}

	g_object_set (gtk_action_group_get_action (priv->actions, "RemoveResource"),
		      "sensitive", value,
		      NULL);
	g_object_set (gtk_action_group_get_action (priv->actions, "EditResource"),
		      "sensitive", value,
		      NULL);
}

static void
resource_view_selection_changed_cb (GtkTreeSelection *selection, PlannerView *view)
{

	resource_view_update_ui (view);
}

static void
resource_view_group_dialog_closed (GtkWidget *widget, gpointer data)
{
	PlannerResourceView *view;

	view = PLANNER_RESOURCE_VIEW (data);

	view->priv->group_dialog = NULL;
}

static gboolean
resource_view_button_press_event (GtkTreeView    *tv,
				  GdkEventButton *event,
				  PlannerView    *view)
{
	PlannerResourceViewPriv *priv;
	GtkTreePath             *path;
	GtkItemFactory          *factory;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;
	factory = priv->popup_factory;

	if (event->button == 3) {
		gtk_widget_grab_focus (GTK_WIDGET (tv));

		/* Select our row */
		if (gtk_tree_view_get_path_at_pos (tv, event->x, event->y, &path, NULL, NULL, NULL)) {
			gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (tv));

			gtk_tree_selection_select_path (gtk_tree_view_get_selection (tv), path);

			gtk_widget_set_sensitive (
				gtk_item_factory_get_widget_by_action (factory, POPUP_REMOVE), TRUE);
			gtk_widget_set_sensitive (
				gtk_item_factory_get_widget_by_action (factory, POPUP_EDIT), TRUE);
			gtk_tree_path_free (path);
		} else {
			gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (tv));

			gtk_widget_set_sensitive (
				gtk_item_factory_get_widget_by_action (factory, POPUP_REMOVE), FALSE);
			gtk_widget_set_sensitive (
				gtk_item_factory_get_widget_by_action (factory, POPUP_EDIT), FALSE);
		}

		gtk_item_factory_popup (factory, event->x_root, event->y_root,
					event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

static void
resource_view_setup_tree_view (PlannerView *view)
{
	GtkTreeView       *tree_view;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;

	tree_view = GTK_TREE_VIEW (PLANNER_RESOURCE_VIEW (view)->priv->tree_view);

	gtk_tree_view_set_rules_hint (tree_view, TRUE);

	g_signal_connect (tree_view,
			  "popup_menu",
			  G_CALLBACK (resource_view_popup_menu),
			  view);

	g_signal_connect (tree_view,
			  "button_press_event",
			  G_CALLBACK (resource_view_button_press_event),
			  view);

	/* Name */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "editable", TRUE, NULL);

	col = gtk_tree_view_column_new_with_attributes (_("Name"), cell, NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_min_width (col, 150);
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_FIXED);

	gtk_tree_view_column_set_cell_data_func (col, cell,
						 resource_view_name_data_func,
						 NULL, NULL);
	g_object_set_data (G_OBJECT (col),
			   "data-func", resource_view_name_data_func);
	g_object_set_data (G_OBJECT (col), "id", "name");

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (resource_view_cell_name_edited),
			  view);
	g_signal_connect (col,
			  "notify::width",
			  G_CALLBACK (resource_view_column_notify_width_cb),
			  view);
	gtk_tree_view_append_column (tree_view, col);

	/* Short name. */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "editable", TRUE, NULL);

	col = gtk_tree_view_column_new_with_attributes (_("Short name"), cell, NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_sizing (col, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_min_width (col, 75);

	gtk_tree_view_column_set_cell_data_func (col, cell,
						 resource_view_short_name_data_func,
						 NULL, NULL);
	g_object_set_data (G_OBJECT (col),
			   "data-func", resource_view_short_name_data_func);
	g_object_set_data (G_OBJECT (col), "id", "short_name");

	gtk_tree_view_append_column (tree_view, col);
	g_signal_connect (col,
			  "notify::width",
			  G_CALLBACK (resource_view_column_notify_width_cb),
			  view);

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (resource_view_cell_short_name_edited),
			  view);

	/* Type */
	cell = planner_cell_renderer_list_new ();
	g_object_set (cell, "editable", TRUE, NULL);

	col = gtk_tree_view_column_new_with_attributes (_("Type"), cell, NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);

	gtk_tree_view_column_set_cell_data_func (col, cell,
						 resource_view_type_data_func,
						 NULL, NULL);
	g_object_set_data (G_OBJECT (col),
			   "data-func", resource_view_type_data_func);
	g_object_set_data (G_OBJECT (col), "id", "type");

	gtk_tree_view_append_column (tree_view, col);
	g_signal_connect (col,
			  "notify::width",
			  G_CALLBACK (resource_view_column_notify_width_cb),
			  view);

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (resource_view_cell_type_edited),
			  view);
	g_signal_connect (cell,
			  "show-popup",
			  G_CALLBACK (resource_view_cell_type_show_popup),
			  view);

	/* Group */
	cell = planner_cell_renderer_list_new ();
	g_object_set (cell, "editable", TRUE, NULL);

	col = gtk_tree_view_column_new_with_attributes (_("Group"),
							cell, NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);

	gtk_tree_view_column_set_cell_data_func (col, cell,
						 resource_view_group_data_func,
						 NULL, NULL);
	g_object_set_data (G_OBJECT (col),
			   "data-func", resource_view_group_data_func);
	g_object_set_data (G_OBJECT (col), "id", "group");

	gtk_tree_view_append_column (tree_view, col);
	g_signal_connect (col,
			  "notify::width",
			  G_CALLBACK (resource_view_column_notify_width_cb),
			  view);

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (resource_view_cell_group_edited),
			  view);
	g_signal_connect (cell,
			  "show-popup",
			  G_CALLBACK (resource_view_cell_group_show_popup),
			  view);
	g_signal_connect_after (cell,
				"hide-popup",
				G_CALLBACK (resource_view_cell_group_hide_popup),
				view);

	/* Email */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "editable", TRUE, NULL);

	col = gtk_tree_view_column_new_with_attributes (_("Email"),
							cell, NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_min_width (col, 150);

	gtk_tree_view_column_set_cell_data_func (col, cell,
						 resource_view_email_data_func,
						 NULL, NULL);
	g_object_set_data (G_OBJECT (col),
			   "data-func", resource_view_email_data_func);
	g_object_set_data (G_OBJECT (col), "id", "email");

	gtk_tree_view_append_column (tree_view, col);
	g_signal_connect (col,
			  "notify::width",
			  G_CALLBACK (resource_view_column_notify_width_cb),
			  view);

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (resource_view_cell_email_edited),
			  view);

	/* Cost */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "editable", TRUE, NULL);

	col = gtk_tree_view_column_new_with_attributes (_("Cost"),
							cell, NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	/*gtk_tree_view_column_set_min_width (col, 150);*/

	gtk_tree_view_column_set_cell_data_func (col, cell,
						 resource_view_cost_data_func,
						 NULL, NULL);
	g_object_set_data (G_OBJECT (col),
			   "data-func", resource_view_cost_data_func);
	g_object_set_data (G_OBJECT (col), "id", "cost");

	gtk_tree_view_append_column (tree_view, col);
	g_signal_connect (col,
			  "notify::width",
			  G_CALLBACK (resource_view_column_notify_width_cb),
			  view);

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (resource_view_cell_cost_edited),
			  view);

	/*
	project = planner_window_get_project (view->main_window);
	properties = mrp_project_get_properties_from_type (project,
							   MRP_TYPE_RESOURCE);

	for (l = properties; l; l = l->next) {
		resource_view_property_added (project, MRP_TYPE_RESOURCE, l->data, view);
	}
	*/

	resource_view_load_columns (PLANNER_RESOURCE_VIEW (view));

	g_signal_connect (tree_view,
			  "columns-changed",
			  G_CALLBACK (resource_view_tree_view_columns_changed_cb),
			  view);

	g_signal_connect (tree_view,
			  "destroy",
			  G_CALLBACK (resource_view_tree_view_destroy_cb),
			  view);
}

static gboolean
resource_cmd_edit_property_do (PlannerCmd *cmd_base)
{
	ResourceCmdEditProperty *cmd;

	cmd = (ResourceCmdEditProperty*) cmd_base;

	g_object_set_property (G_OBJECT (cmd->resource),
			       cmd->property,
			       cmd->value);

	return TRUE;
}

static void
resource_cmd_edit_property_undo (PlannerCmd *cmd_base)
{
	ResourceCmdEditProperty *cmd;

	cmd = (ResourceCmdEditProperty*) cmd_base;

	g_object_set_property (G_OBJECT (cmd->resource),
			       cmd->property,
			       cmd->old_value);
}

static void
resource_cmd_edit_property_free (PlannerCmd *cmd_base)
{
	ResourceCmdEditProperty *cmd;

	cmd = (ResourceCmdEditProperty*) cmd_base;

	g_value_unset (cmd->value);
	g_value_unset (cmd->old_value);
	g_free (cmd->value);
	g_free (cmd->old_value);

	g_object_unref (cmd->resource);
	g_free (cmd->property);

}

static PlannerCmd *
resource_cmd_edit_property (PlannerView  *view,
			    MrpResource  *resource,
			    const gchar  *property,
			    const GValue *value)
{
	PlannerCmd              *cmd_base;
	ResourceCmdEditProperty *cmd;

	cmd_base = planner_cmd_new (ResourceCmdEditProperty,
				    _("Edit resource property"),
				    resource_cmd_edit_property_do,
				    resource_cmd_edit_property_undo,
				    resource_cmd_edit_property_free);

	cmd = (ResourceCmdEditProperty *) cmd_base;

	cmd->property = g_strdup (property);
	cmd->resource = g_object_ref (resource);

	cmd->value = g_new0 (GValue, 1);
	g_value_init (cmd->value, G_VALUE_TYPE (value));
	g_value_copy (value, cmd->value);

	cmd->old_value = g_new0 (GValue, 1);
	g_value_init (cmd->old_value, G_VALUE_TYPE (value));

	g_object_get_property (G_OBJECT (cmd->resource),
			       cmd->property,
			       cmd->old_value);

	/* FIXME: if old and new value are the same, do nothing
	   How we can compare values?
	*/

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (view->main_window),
					   cmd_base);

	return cmd_base;
}

static gboolean
resource_cmd_edit_custom_property_do (PlannerCmd *cmd_base)
{
	ResourceCmdEditCustomProperty *cmd;
	cmd = (ResourceCmdEditCustomProperty*) cmd_base;

	mrp_object_set_property (MRP_OBJECT (cmd->resource),
				 cmd->property,
				 cmd->value);

	return TRUE;
}

static void
resource_cmd_edit_custom_property_undo (PlannerCmd *cmd_base)
{
	ResourceCmdEditCustomProperty *cmd;

	cmd = (ResourceCmdEditCustomProperty*) cmd_base;

	/* FIXME: delay in the UI when setting the property */
	mrp_object_set_property (MRP_OBJECT (cmd->resource),
				 cmd->property,
				 cmd->old_value);

}

static void
resource_cmd_edit_custom_property_free (PlannerCmd *cmd_base)
{
	ResourceCmdEditCustomProperty *cmd;

	cmd = (ResourceCmdEditCustomProperty*) cmd_base;

	g_value_unset (cmd->value);
	g_value_unset (cmd->old_value);

	g_object_unref (cmd->resource);
}

static PlannerCmd *
resource_cmd_edit_custom_property (PlannerView  *view,
				   MrpResource  *resource,
				   MrpProperty  *property,
				   const GValue *value)
{
	PlannerCmd                    *cmd_base;
	ResourceCmdEditCustomProperty *cmd;

	cmd_base = planner_cmd_new (ResourceCmdEditCustomProperty,
				    _("Edit resource property"),
				    resource_cmd_edit_custom_property_do,
				    resource_cmd_edit_custom_property_undo,
				    resource_cmd_edit_custom_property_free);

	cmd = (ResourceCmdEditCustomProperty *) cmd_base;

	cmd->property = property;
	cmd->resource = g_object_ref (resource);

	cmd->value = g_new0 (GValue, 1);
	g_value_init (cmd->value, G_VALUE_TYPE (value));
	g_value_copy (value, cmd->value);

	cmd->old_value = g_new0 (GValue, 1);
	g_value_init (cmd->old_value, G_VALUE_TYPE (value));

	mrp_object_get_property (MRP_OBJECT (cmd->resource),
				 cmd->property,
				 cmd->old_value);

	/* FIXME: if old and new value are the same, do nothing
	   How we can compare values?
	*/

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (view->main_window),
					   cmd_base);

	return cmd_base;
}

static void
resource_view_cell_name_edited (GtkCellRendererText *cell,
				gchar               *path_string,
				gchar               *new_text,
				gpointer             user_data)
{
	PlannerView      *view;
	MrpResource      *resource;
	GtkTreeView      *tree_view;
	GtkTreeModel     *model;
	GtkTreePath      *path;
	GtkTreeIter       iter;
	GValue            value = { 0 };

	view = PLANNER_VIEW (user_data);

	tree_view = PLANNER_RESOURCE_VIEW (view)->priv->tree_view;
	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter, COL_RESOURCE, &resource, -1);

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, new_text);
	resource_cmd_edit_property (view, resource, "name", &value);
	g_value_unset (&value);

	gtk_tree_path_free (path);
}

static void
resource_view_cell_short_name_edited (GtkCellRendererText *cell,
				      gchar               *path_string,
				      gchar               *new_text,
				      gpointer             user_data)
{
	PlannerResourceView *view;
	MrpResource         *resource;
	GtkTreeView         *tree_view;
	GtkTreeModel        *model;
	GtkTreePath         *path;
	GtkTreeIter          iter;
	GValue               value = { 0 };

	view = PLANNER_RESOURCE_VIEW (user_data);

	tree_view = view->priv->tree_view;

	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter, COL_RESOURCE, &resource, -1);

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, new_text);
	resource_cmd_edit_property (PLANNER_VIEW (view), resource, "short_name", &value);
	g_value_unset (&value);

	gtk_tree_path_free (path);
}


static void
resource_view_cell_email_edited (GtkCellRendererText *cell,
				 gchar               *path_string,
				 gchar               *new_text,
				 gpointer             user_data)
{
	PlannerView      *view;
	MrpResource      *resource;
	GtkTreeView      *tree_view;
	GtkTreeModel     *model;
	GtkTreePath      *path;
	GtkTreeIter       iter;
	GValue            value = { 0 };

	view = PLANNER_VIEW (user_data);

	tree_view = PLANNER_RESOURCE_VIEW (view)->priv->tree_view;

	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter, COL_RESOURCE, &resource, -1);

	g_value_init (&value, G_TYPE_STRING);
	g_value_set_string (&value, new_text);
	resource_cmd_edit_property (view, resource, "email", &value);
	g_value_unset (&value);

	gtk_tree_path_free (path);
}

static void
resource_view_cell_cost_edited (GtkCellRendererText *cell,
				gchar               *path_string,
				gchar               *new_text,
				gpointer             user_data)
{
	PlannerView  *view;
	MrpResource  *resource;
	GtkTreeView  *tree_view;
	GtkTreeModel *model;
	GtkTreePath  *path;
	GtkTreeIter   iter;
	GValue        value = { 0 };
	gfloat        fvalue;

	view = PLANNER_VIEW (user_data);

	tree_view = PLANNER_RESOURCE_VIEW (view)->priv->tree_view;

	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter, COL_RESOURCE, &resource, -1);

	fvalue = planner_parse_float (new_text);
	g_value_init (&value, G_TYPE_FLOAT);
	g_value_set_float (&value, fvalue);
	resource_cmd_edit_property (view, resource, "cost", &value);
	g_value_unset (&value);

	gtk_tree_path_free (path);
}

static void
resource_view_cell_type_edited (PlannerCellRendererList *cell,
				gchar                   *path_string,
				gchar                   *new_text,
				gpointer                 user_data)
{
	PlannerView      *view;
	MrpResource      *resource;
	MrpResourceType   type;
	GtkTreeView      *tree_view;
	GtkTreeModel     *model;
	GtkTreePath      *path;
	GtkTreeIter       iter;
	GValue            value = { 0 };

	view = PLANNER_VIEW (user_data);

	tree_view = PLANNER_RESOURCE_VIEW (view)->priv->tree_view;

	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter, COL_RESOURCE, &resource, -1);

	if (cell->selected_index == 0) {
		type = MRP_RESOURCE_TYPE_WORK;
	} else {
		type = MRP_RESOURCE_TYPE_MATERIAL;
	}

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, type);
	resource_cmd_edit_property (view, resource, "type", &value);
	g_value_unset (&value);

	gtk_tree_path_free (path);
}

static void
resource_view_cell_type_show_popup (PlannerCellRendererList *cell,
				    const gchar        *path_string,
				    gint                x1,
				    gint                y1,
				    gint                x2,
				    gint                y2,
				    PlannerView             *view)
{
	GtkTreeView      *tree_view;
	GtkTreeModel     *model;
	GtkTreePath      *path;
	GtkTreeIter       iter;
	MrpResource      *resource;
	GList            *list;
	MrpResourceType   type;

	tree_view = GTK_TREE_VIEW (PLANNER_RESOURCE_VIEW (view)->priv->tree_view);
	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter, COL_RESOURCE, &resource, -1);

	list = NULL;
	list = g_list_append (list, g_strdup (_("Work")));
	list = g_list_append (list, g_strdup (_("Material")));

	cell->list = list;

	mrp_object_get (resource, "type", &type, NULL);

	if (type == MRP_RESOURCE_TYPE_WORK) {
		cell->selected_index = 0;
	} else {
		cell->selected_index = 1;
	}

	gtk_tree_path_free (path);
}

static void
resource_view_cell_group_edited (PlannerCellRendererList *cell,
				 gchar                   *path_string,
				 gchar                   *new_text,
				 gpointer                 user_data)
{
	PlannerView      *view;
	MrpResource      *resource;
	MrpGroup         *group;
	GtkTreeView      *tree_view;
	GtkTreeModel     *model;
	GtkTreePath      *path;
	GtkTreeIter       iter;
	GList            *list;
	GValue            value = { 0 };

	view = PLANNER_VIEW (user_data);

	tree_view = PLANNER_RESOURCE_VIEW (view)->priv->tree_view;

	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter, COL_RESOURCE, &resource, -1);

	list = g_list_nth (cell->user_data, cell->selected_index);
	if (list == NULL) {
		/* We should probably parse the string here and set
		 * the right group. See #110.
		 */
		return;
	}

	group = list->data;

	g_value_init (&value, MRP_TYPE_GROUP);
	g_value_set_object (&value, group);

	resource_cmd_edit_property (view, resource, "group", &value);
	g_value_unset (&value);

	gtk_tree_path_free (path);
}

static GValue
resource_view_custom_property_set_value (MrpProperty *property,
					 gchar       *new_text)
{
	GValue              value = { 0 };
	MrpPropertyType     type;
	gfloat              fvalue;

	/* FIXME: implement mrp_object_set_property like
	 * g_object_set_property that takes a GValue.
	 */
	type = mrp_property_get_property_type (property);

	switch (type) {
	case MRP_PROPERTY_TYPE_STRING:
		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, new_text);

		break;
	case MRP_PROPERTY_TYPE_INT:
		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, atoi (new_text));

		break;
	case MRP_PROPERTY_TYPE_FLOAT:
		fvalue = planner_parse_float (new_text);
		g_value_init (&value, G_TYPE_FLOAT);
		g_value_set_float (&value, fvalue);

		break;

	case MRP_PROPERTY_TYPE_DURATION:
		/* FIXME: support reading units etc... */
		g_value_init (&value, G_TYPE_INT);
		g_value_set_int (&value, atoi (new_text) *8*60*60);

		break;


	case MRP_PROPERTY_TYPE_DATE:

		break;
	case MRP_PROPERTY_TYPE_COST:
		fvalue = planner_parse_float (new_text);
		g_value_init (&value, G_TYPE_FLOAT);
		g_value_set_float (&value, fvalue);

		break;

	default:
		g_assert_not_reached ();
		break;
	}

	return value;
}

static void
resource_view_property_value_edited (GtkCellRendererText *cell,
				     gchar               *path_str,
				     gchar               *new_text,
				     ColPropertyData     *data)
{
	PlannerView        *view;
	GtkTreePath        *path;
	GtkTreeIter         iter;
	GtkTreeModel       *model;
	MrpProperty        *property;
	MrpResource        *resource;
	GValue              value;

	view = data->view;
	model = gtk_tree_view_get_model (PLANNER_RESOURCE_VIEW (view)->priv->tree_view);
	property = data->property;

	path = gtk_tree_path_new_from_string (path_str);
	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (model, &iter,
			    COL_RESOURCE, &resource,
			    -1);

	value = resource_view_custom_property_set_value (property, new_text);

	resource_cmd_edit_custom_property (view, resource,
					   property,
					   &value);
	g_value_unset (&value);

	gtk_tree_path_free (path);
}

static void
resource_view_cell_group_show_popup (PlannerCellRendererList *cell,
				     const gchar        *path_string,
				     gint                x1,
				     gint                y1,
				     gint                x2,
				     gint                y2,
				     PlannerView        *view)
{
	GtkTreeView      *tree_view;
	GtkTreeModel     *model;
	GtkTreeIter       iter;
	GtkTreePath      *path;
	GList            *list;
	MrpProject       *project;
	MrpResource      *resource;
	MrpGroup         *group;
	MrpGroup         *current_group;
	GList            *groups, *l;
	gchar            *name;
	gint              i, index;

	tree_view = PLANNER_RESOURCE_VIEW (view)->priv->tree_view;

	model = gtk_tree_view_get_model (tree_view);

	path = gtk_tree_path_new_from_string (path_string);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);


	project = planner_window_get_project (view->main_window);

	gtk_tree_model_get (model, &iter, COL_RESOURCE, &resource, -1);

	mrp_object_get (resource,
			"group", &current_group,
			NULL);

	i = 0;
	index = 0;
	list = NULL;
	groups = g_list_copy (mrp_project_get_groups (project));
	groups = g_list_prepend (groups, NULL);

	for (l = groups; l; l = l->next) {
		group = l->data;

		if (group != NULL) {
			mrp_object_get (group, "name", &name, NULL);
			g_object_ref (group);
		} else {
			name = g_strdup (_("(None)"));
		}

		list = g_list_prepend (list, name);

		if (current_group == group) {
			index = i;
		}

		i++;
	}

	cell->list = g_list_reverse (list);
	cell->user_data = groups;
	cell->selected_index = index;
}

static void
resource_view_cell_group_hide_popup (PlannerCellRendererList *cell,
				     PlannerView             *view)
{
	GList *l;

	for (l = cell->user_data; l; l = l->next) {
		if (l->data) {
			g_object_unref (l->data);
		}
	}

	g_list_free (cell->user_data);
	cell->user_data = NULL;
}

static void
resource_view_edit_groups_cb (GtkAction *action,
			      gpointer   data)
{
	PlannerView *view;

	view = PLANNER_VIEW (data);

	/* FIXME: we have to destroy group_dialog correctly */
	if (PLANNER_RESOURCE_VIEW (view)->priv->group_dialog == NULL) {
		PLANNER_RESOURCE_VIEW (view)->priv->group_dialog = planner_group_dialog_new (view);

		g_signal_connect (PLANNER_RESOURCE_VIEW (view)->priv->group_dialog,
				  "destroy",
				  G_CALLBACK (resource_view_group_dialog_closed),
				  view);
	} else {
		gtk_window_present (GTK_WINDOW (PLANNER_RESOURCE_VIEW (view)->priv->group_dialog));
	}
}

static void
resource_view_project_loaded_cb (MrpProject *project, PlannerView *view)
{
	GtkTreeModel *model;
	GList        *resources, *l;
	GtkTreeView  *tree_view;

	tree_view = PLANNER_RESOURCE_VIEW (view)->priv->tree_view;

	model = GTK_TREE_MODEL (gtk_list_store_new (NUM_OF_COLS,
						    G_TYPE_POINTER));

	resources = mrp_project_get_resources (project);
	for (l = resources; l; l = l->next) {
		GtkTreeIter iter;

		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model),
				    &iter,
				    COL_RESOURCE, MRP_RESOURCE (l->data),
				    -1);
	}
	gtk_tree_view_set_model (tree_view, model);

 	g_object_unref (model);
}

static void
resource_view_property_added (MrpProject  *project,
			      GType        object_type,
			      MrpProperty *property,
			      PlannerView *view)
{
	PlannerResourceViewPriv   *priv;
	MrpPropertyType    type;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	ColPropertyData   *data;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	data = g_new0 (ColPropertyData, 1);

	type = mrp_property_get_property_type (property);

	if (object_type != MRP_TYPE_RESOURCE ||
	    !mrp_property_get_user_defined (property)) {
		return;
	}

	if (type == MRP_PROPERTY_TYPE_DATE) {
		return;
	} else {
		cell = gtk_cell_renderer_text_new ();
	}

	g_object_set (cell, "editable", TRUE, NULL);

	g_signal_connect_data (cell,
			       "edited",
			       G_CALLBACK (resource_view_property_value_edited),
			       data,
			       (GClosureNotify) g_free,
			       0);

	col = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_title (col,
					mrp_property_get_label (property));

	g_object_set_data (G_OBJECT (col), "custom", GINT_TO_POINTER (TRUE));

	g_hash_table_insert (priv->property_to_column, property, col);

	data->property = property;
	data->view = view;

	gtk_tree_view_column_pack_start (col, cell, TRUE);

	gtk_tree_view_column_set_cell_data_func (col,
						 cell,
						 resource_view_property_data_func,
						 property,
						 NULL);
	g_object_set_data (G_OBJECT (col),
			   "data-func", resource_view_property_data_func);
	g_object_set_data (G_OBJECT (col),
			   "user-data", property);

	gtk_tree_view_append_column (priv->tree_view, col);
}

static void
resource_view_property_removed (MrpProject  *project,
				MrpProperty *property,
				PlannerView *view)
{
	PlannerResourceViewPriv *priv;
	GtkTreeViewColumn       *col;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	col = g_hash_table_lookup (priv->property_to_column, property);
	if (col) {
		g_hash_table_remove (priv->property_to_column, property);

		gtk_tree_view_remove_column (GTK_TREE_VIEW (priv->tree_view), col);
	}
}

static void
resource_view_property_changed (MrpProject  *project,
			      MrpProperty *property,
			      PlannerView *view)
{
	PlannerResourceViewPriv *priv;
	GtkTreeViewColumn       *col;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	col = g_hash_table_lookup (priv->property_to_column, property);
	if (col) {
		gtk_tree_view_column_set_title (col,
					mrp_property_get_label (property));
	}
}

static void
resource_view_popup_menu (GtkWidget   *widget,
			  PlannerView *view)
{
	PlannerResourceViewPriv   *priv;
	GtkTreeView       *tree;
	GtkTreePath       *path;
	GtkTreeViewColumn *column;
	GdkRectangle       rect;
	gint               x, y;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;
	tree = GTK_TREE_VIEW (priv->tree_view);

	gtk_tree_view_get_cursor (tree, &path, &column);
	gtk_tree_view_get_cell_area (tree,
				     path,
				     column,
				     &rect);

	x = rect.x;
	y = rect.y;

	/* Note: this is not perfect, but good enough for now. */
	gdk_window_get_root_origin (GTK_WIDGET (tree)->window, &x, &y);
	rect.x += x;
	rect.y += y;

	gtk_widget_translate_coordinates (GTK_WIDGET (tree),
					  gtk_widget_get_toplevel (GTK_WIDGET (tree)),
					  rect.x, rect.y,
					  &x, &y);

	/* Offset so it's not overlapping the cell. */
	rect.x = x + 20;
	rect.y = y + 20;

	gtk_item_factory_popup (priv->popup_factory,
				rect.x, rect.y,
				0,
				gtk_get_current_event_time ());
}

static void
resource_view_name_data_func (GtkTreeViewColumn    *tree_column,
			      GtkCellRenderer      *cell,
			      GtkTreeModel         *tree_model,
			      GtkTreeIter          *iter,
			      gpointer              data)
{
	MrpResource *resource;
	gchar       *name;

	gtk_tree_model_get (tree_model, iter, COL_RESOURCE, &resource, -1);

	g_object_get (resource, "name", &name, NULL);

	g_object_set (cell, "text", name, NULL);
	g_free (name);
}

static void
resource_view_type_data_func (GtkTreeViewColumn    *tree_column,
			      GtkCellRenderer      *cell,
			      GtkTreeModel         *tree_model,
			      GtkTreeIter          *iter,
			      gpointer              data)
{
	MrpResource *resource;
	gint         type;

	gtk_tree_model_get (tree_model, iter, COL_RESOURCE, &resource, -1);

	g_object_get (resource, "type", &type, NULL);

	g_object_set (cell,
		      "text", resource_view_get_type_string (type),
		      NULL);
}

static void
resource_view_group_data_func (GtkTreeViewColumn    *tree_column,
			       GtkCellRenderer      *cell,
			       GtkTreeModel         *tree_model,
			       GtkTreeIter          *iter,
			       gpointer              data)
{
	MrpResource *resource;
	MrpGroup    *group;
	gchar       *name;

	gtk_tree_model_get (tree_model, iter, COL_RESOURCE, &resource, -1);

	g_object_get (resource, "group", &group, NULL);

	if (!group) {
		g_object_set (cell, "text", "", NULL);
		return;
	}

	g_object_get (group, "name", &name, NULL);
	g_object_set (cell, "text", name, NULL);

	g_free (name);
}

static void
resource_view_short_name_data_func (GtkTreeViewColumn    *tree_column,
			       GtkCellRenderer      *cell,
			       GtkTreeModel         *tree_model,
			       GtkTreeIter          *iter,
			       gpointer              data)
{
	MrpResource *resource;
	gchar       *short_name;

	gtk_tree_model_get (tree_model, iter, COL_RESOURCE, &resource, -1);

	g_object_get (resource, "short_name", &short_name, NULL);

	g_object_set (cell, "text", short_name, NULL);
	g_free (short_name);
}

static void
resource_view_email_data_func (GtkTreeViewColumn    *tree_column,
			       GtkCellRenderer      *cell,
			       GtkTreeModel         *tree_model,
			       GtkTreeIter          *iter,
			       gpointer              data)
{
	MrpResource *resource;
	gchar       *email;

	gtk_tree_model_get (tree_model, iter, COL_RESOURCE, &resource, -1);

	g_object_get (resource, "email", &email, NULL);

	g_object_set (cell, "text", email, NULL);
	g_free (email);
}

static void
resource_view_cost_data_func (GtkTreeViewColumn    *tree_column,
			      GtkCellRenderer      *cell,
			      GtkTreeModel         *tree_model,
			      GtkTreeIter          *iter,
			      gpointer              data)
{
	MrpResource *resource;
	gfloat       cost;
	gchar       *cost_text;

	gtk_tree_model_get (tree_model, iter, COL_RESOURCE, &resource, -1);

	g_object_get (resource, "cost", &cost, NULL);
	cost_text = planner_format_float (cost, 2, FALSE);

	g_object_set (cell, "text", cost_text, NULL);
	g_free (cost_text);
}

static void
resource_view_property_data_func (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer   *cell,
				  GtkTreeModel      *model,
				  GtkTreeIter       *iter,
				  gpointer           data)
{
	MrpObject       *object;
	MrpProperty     *property = data;
	MrpPropertyType  type;
	gchar           *svalue;
	gint             ivalue;
	gfloat           fvalue;
	mrptime          tvalue;
	MrpProject      *project;

	gtk_tree_model_get (model, iter,
			    COL_RESOURCE, &object,
			    -1);

	/* FIXME: implement mrp_object_get_property like
	 * g_object_get_property that takes a GValue.
	 */
	type = mrp_property_get_property_type (property);

	switch (type) {
	case MRP_PROPERTY_TYPE_STRING:
		mrp_object_get (object,
				mrp_property_get_name (property), &svalue,
				NULL);

		if (svalue == NULL) {
			svalue = g_strdup ("");
		}

		break;
	case MRP_PROPERTY_TYPE_INT:
		mrp_object_get (object,
				mrp_property_get_name (property), &ivalue,
				NULL);
		svalue = g_strdup_printf ("%d", ivalue);
		break;

	case MRP_PROPERTY_TYPE_FLOAT:
		mrp_object_get (object,
				mrp_property_get_name (property), &fvalue,
				NULL);

		svalue = planner_format_float (fvalue, 4, FALSE);
		break;

	case MRP_PROPERTY_TYPE_DATE:
		mrp_object_get (object,
				mrp_property_get_name (property), &tvalue,
				NULL);
		svalue = planner_format_date (tvalue);
		break;

	case MRP_PROPERTY_TYPE_DURATION:
		mrp_object_get (object,
				mrp_property_get_name (property), &ivalue,
				NULL);
		project = planner_window_get_project (PLANNER_VIEW (data)->main_window);
		svalue = planner_format_duration (project, ivalue);
		break;

	case MRP_PROPERTY_TYPE_COST:
		mrp_object_get (object,
				mrp_property_get_name (property), &fvalue,
				NULL);

		svalue = planner_format_float (fvalue, 2, FALSE);
		break;

	default:
		g_warning ("Type not implemented.");
		svalue = g_strdup ("");
		break;
	}

	g_object_set (cell, "text", svalue, NULL);
	g_free (svalue);
}

static void
resource_view_selection_foreach (GtkTreeModel  *model,
				 GtkTreePath   *path,
				 GtkTreeIter   *iter,
				 GList        **list)
{
	MrpResource *resource;

	gtk_tree_model_get (model, iter,
			    COL_RESOURCE, &resource,
			    -1);

	*list = g_list_prepend (*list, resource);

}

static GList *
resource_view_selection_get_list (PlannerView *view)
{
	PlannerResourceViewPriv *priv;
	GtkTreeSelection        *selection;
	GList                   *ret_list;

	priv = PLANNER_RESOURCE_VIEW (view)->priv;

	ret_list = NULL;

	selection = gtk_tree_view_get_selection (priv->tree_view);

	gtk_tree_selection_selected_foreach (selection,
					     (GtkTreeSelectionForeachFunc) resource_view_selection_foreach,
					     &ret_list);
	return ret_list;
}


static void
resource_view_save_columns (PlannerResourceView *view)
{
	PlannerResourceViewPriv *priv;

	priv = view->priv;

	planner_view_column_save_helper (PLANNER_VIEW (view),
					 GTK_TREE_VIEW (priv->tree_view));
}

static void
resource_view_load_columns (PlannerResourceView *view)
{
	PlannerResourceViewPriv *priv;
	GList                   *columns, *l;
	GtkTreeViewColumn       *column;
	const gchar             *id;
	gint                     i;

	priv = view->priv;

	/* Load the columns. */
	planner_view_column_load_helper (PLANNER_VIEW (view),
					 GTK_TREE_VIEW (priv->tree_view));

	/* Make things a bit more robust by setting defaults if we don't get any
	 * visible columns. Should be done through a schema instead (but we'll
	 * keep this since a lot of people get bad installations when installing
	 * themselves).
	 */
	columns = gtk_tree_view_get_columns (GTK_TREE_VIEW (priv->tree_view));
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

			gtk_tree_view_column_set_visible (column, TRUE);
		}
	}

	g_list_free (columns);
}

PlannerView *
planner_resource_view_new (void)
{
	PlannerView *view;

	view = g_object_new (PLANNER_TYPE_RESOURCE_VIEW, NULL);

	return view;
}
