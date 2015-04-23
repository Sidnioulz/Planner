/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2002 Alvaro del Castillo <acs@barrapunto.com>
 * Copyright (C) 2004 Lincoln Phipps <lincoln.phipps@openmutual.net>
 * Copyright (C) 2004 Imendio AB
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <libplanner/mrp-object.h>
#include <libplanner/mrp-property.h>
#include "libplanner/mrp-paths.h"
#include "planner-cell-renderer-list.h"
#include "planner-property-dialog.h"
#include "planner-property-model.h"

#define GET_PRIV(d) (g_object_get_data (G_OBJECT (d), "priv"))


typedef struct {
	PlannerWindow    *main_window;
	MrpProject       *project;
	GtkTreeModel     *model;
	GtkWidget        *tree;
	GType             owner;
	MrpPropertyStore *shop;
} PlannerPropertyDialogPriv;

typedef struct {
	PlannerCmd        base;

	PlannerWindow	 *window;
	MrpProject       *project;
	gchar	 	 *name;
	MrpPropertyType	  type;
	gchar	 	 *label_text;
	gchar	 	 *description;
	GType		  owner;
	gboolean	  user_defined;
} PropertyCmdAdd;

typedef struct {
	PlannerCmd       base;

	MrpProject      *project;
	gchar	 	*name;
	MrpProperty	*property;
	MrpPropertyType	 type;
	gchar	 	*label_text;
	gchar	 	*description;
	GType		 owner;
	gboolean	 user_defined;

	GHashTable      *tasks;
	GHashTable      *resources;
} PropertyCmdRemove;

typedef struct {
	PlannerCmd     base;

	PlannerWindow *window;
	MrpProject    *project;
	GType          owner;
	gchar	      *name;
	gchar         *old_text;
	gchar  	      *new_text;
} PropertyCmdLabelEdited;


static void        property_dialog_setup_option_menu    (GtkWidget           *option_menu,
							 GCallback            func,
							 gpointer             user_data,
							 gconstpointer        str1,
							 ...);
static gint        property_dialog_get_selected         (GtkWidget           *option_menu);
static void        property_dialog_close_cb             (GtkWidget           *button,
							 GtkWidget           *dialog);
static void        property_dialog_type_selected_cb     (GtkWidget           *widget,
							 GtkWidget           *dialog);
static gboolean    property_dialog_label_changed_cb     (GtkWidget           *label_entry,
							 GdkEvent            *event,
							 GtkWidget           *name_entry);
static void        property_dialog_add_cb               (GtkWidget           *button,
							 GtkWidget           *dialog);
static void        property_dialog_remove_cb            (GtkWidget           *button,
							 GtkWidget           *dialog);
static void        property_dialog_label_edited         (GtkCellRendererText *cell,
							 gchar               *path_str,
							 gchar               *new_text,
							 GtkWidget           *dialog);
static void        property_dialog_setup_list           (GtkWidget           *dialog);
static void        property_dialog_setup_widgets        (GtkWidget           *dialog,
							 GladeXML            *glade);
static PlannerCmd *property_cmd_add                     (PlannerWindow       *window,
							 MrpProject          *project,
							 GType                owner,
							 const gchar         *name,
							 MrpPropertyType      type,
							 const gchar         *label_text,
							 const gchar         *description,
							 gboolean             user_defined);
static gboolean    property_cmd_add_do                  (PlannerCmd          *cmd_base);
static void        property_cmd_add_undo                (PlannerCmd          *cmd_base);
static void        property_cmd_add_free                (PlannerCmd          *cmd_base);
static PlannerCmd *property_cmd_remove                  (PlannerWindow       *window,
							 MrpProject          *project,
							 GType                owner,
							 const gchar         *name);
static gboolean    property_cmd_remove_do               (PlannerCmd          *cmd_base);
static void        property_cmd_remove_undo             (PlannerCmd          *cmd_base);
static void        property_cmd_remove_free             (PlannerCmd          *cmd_base);
static PlannerCmd *property_cmd_label_edited            (PlannerWindow       *window,
							 MrpProperty         *property,
							 MrpProject          *project,
							 GType                owner,
							 const gchar         *new_text);
static gboolean    property_cmd_label_edited_do         (PlannerCmd          *cmd_base);
static void        property_cmd_label_edited_undo       (PlannerCmd          *cmd_base);
static void        property_cmd_label_edited_free       (PlannerCmd          *cmd_base);


static void
property_dialog_setup_option_menu (GtkWidget     *option_menu,
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
property_dialog_get_selected (GtkWidget *option_menu)
{
	GtkWidget *menu;
	GtkWidget *item;
	gint       ret;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));

	item = gtk_menu_get_active (GTK_MENU (menu));

	ret = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "data"));

	return ret;
}

static void
property_dialog_close_cb (GtkWidget *button,
			  GtkWidget *dialog)
{
	gtk_widget_destroy (dialog);
}

static void
property_dialog_type_selected_cb (GtkWidget *widget,
				  GtkWidget *dialog)
{
	gint type;

	type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "data"));

	g_object_set_data (G_OBJECT (dialog), "type", GINT_TO_POINTER (type));
}

static gboolean
property_dialog_label_changed_cb (GtkWidget *label_entry,
				  GdkEvent  *event,
				  GtkWidget *name_entry)
{
	const gchar *name;
	const gchar *label;

	name = gtk_entry_get_text (GTK_ENTRY (name_entry));

	if (name == NULL || name[0] == 0) {
		label = gtk_entry_get_text (GTK_ENTRY (label_entry));
		gtk_entry_set_text (GTK_ENTRY (name_entry), label);
	}

	return FALSE;
}

static void
property_dialog_add_cb (GtkWidget *button,
			GtkWidget *dialog)
{
	PlannerPropertyDialogPriv *priv;
	MrpPropertyType            type;
	const gchar               *label;
	const gchar               *name;
	const gchar               *description;
	GladeXML                  *glade;
	GtkWidget                 *label_entry;
	GtkWidget                 *name_entry;
	GtkWidget                 *add_dialog;
	GtkWidget                 *w;
	gint                       response;
	gboolean                   finished = FALSE;
	gunichar                   c;
	gchar                     *filename;

	priv = GET_PRIV (dialog);

	filename = mrp_paths_get_glade_dir ("new-property.glade");
	glade = glade_xml_new (filename,
			       NULL,
			       NULL);
	g_free (filename);

	add_dialog = glade_xml_get_widget (glade, "add_dialog");

	label_entry = glade_xml_get_widget (glade, "label_entry");
	name_entry = glade_xml_get_widget (glade, "name_entry");

	g_signal_connect (label_entry,
			  "focus_out_event",
			  G_CALLBACK (property_dialog_label_changed_cb),
			  name_entry);

	property_dialog_setup_option_menu (
		glade_xml_get_widget (glade, "type_menu"),
		G_CALLBACK (property_dialog_type_selected_cb),
		add_dialog,
		mrp_property_type_as_string (MRP_PROPERTY_TYPE_STRING),
		MRP_PROPERTY_TYPE_STRING,
		mrp_property_type_as_string (MRP_PROPERTY_TYPE_INT),
		MRP_PROPERTY_TYPE_INT,
		mrp_property_type_as_string (MRP_PROPERTY_TYPE_FLOAT),
		MRP_PROPERTY_TYPE_FLOAT,
/*  		mrp_property_type_as_string (MRP_PROPERTY_TYPE_DATE), */
/*  		MRP_PROPERTY_TYPE_DATE, */
/*  		mrp_property_type_as_string (MRP_PROPERTY_TYPE_DURATION), */
/*  		MRP_PROPERTY_TYPE_DURATION, */
/* 		mrp_property_type_as_string (MRP_PROPERTY_TYPE_COST), */
/*  		MRP_PROPERTY_TYPE_COST, */
		NULL);

	while (!finished) {
		response = gtk_dialog_run (GTK_DIALOG (add_dialog));

		switch (response) {
		case GTK_RESPONSE_OK:
			label = gtk_entry_get_text (GTK_ENTRY (label_entry));
			if (label == NULL || label[0] == 0) {
				finished = FALSE;
				break;
			}

			name = gtk_entry_get_text (GTK_ENTRY (name_entry));
			if (name == NULL || name[0] == 0) {
				finished = FALSE;
				break;
			}

			c = g_utf8_get_char (name);
			if (!g_unichar_isalpha (c)) {
				GtkWidget *msg_dialog;

				msg_dialog = gtk_message_dialog_new (
					GTK_WINDOW (add_dialog),
					GTK_DIALOG_MODAL |
					GTK_DIALOG_DESTROY_WITH_PARENT,
					GTK_MESSAGE_WARNING,
					GTK_BUTTONS_OK,
					_("The name of the custom property needs to start with a letter."));

				gtk_dialog_run (GTK_DIALOG (msg_dialog));
				gtk_widget_destroy (msg_dialog);

				finished = FALSE;
				break;
			}

			w = glade_xml_get_widget (glade, "description_entry");
			description = gtk_entry_get_text (GTK_ENTRY (w));

			w = glade_xml_get_widget (glade, "type_menu");
			type = property_dialog_get_selected (w);

			if (type != MRP_PROPERTY_TYPE_NONE) {
				property_cmd_add (priv->main_window,
						  priv->project,
						  priv->owner,
						  name,
						  type,
						  label,
						  description,
						  TRUE);
			}

			finished = TRUE;
			break;

		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CANCEL:
			finished = TRUE;
			break;

		default:
			break;
		}
	}

 	gtk_widget_destroy (add_dialog);
	g_object_unref (glade);
}

static void
property_dialog_remove_cb (GtkWidget *button, GtkWidget *dialog)
{
	PlannerPropertyDialogPriv *priv;
	GtkTreeSelection          *selection;
	GtkTreeIter                iter;
	gchar                     *name;
	GtkWidget                 *remove_dialog;
	gint                       response;

	priv = GET_PRIV (dialog);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return;
	}

	gtk_tree_model_get (priv->model, &iter,
			    COL_NAME, &name,
			    -1);

	remove_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog),
						GTK_DIALOG_MODAL |
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_QUESTION,
						GTK_BUTTONS_YES_NO,
						_("Do you really want to remove the property '%s' from "
						  "the project?"),
						name);

	response = gtk_dialog_run (GTK_DIALOG (remove_dialog));

	switch (response) {
	case GTK_RESPONSE_YES:
		property_cmd_remove (priv->main_window,
				     priv->project,
				     priv->owner,
				     name);
		break;

	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CANCEL:
		break;

	default:
		break;
	}

	gtk_widget_destroy (remove_dialog);
	g_free (name);
}

static void
property_dialog_label_edited (GtkCellRendererText *cell,
			      gchar               *path_str,
			      gchar               *new_text,
			      GtkWidget           *dialog)
{
	PlannerPropertyDialogPriv *priv;
	GtkTreePath               *path;
	GtkTreeIter                iter;
	GtkTreeModel              *model;
	MrpProperty               *property;

	priv = GET_PRIV (dialog);

	model = priv->model;
	path = gtk_tree_path_new_from_string (path_str);

	gtk_tree_model_get_iter (model, &iter, path);

	gtk_tree_model_get (priv->model, &iter,
			    COL_PROPERTY, &property,
			    -1);

	if (strcmp (new_text, mrp_property_get_label (property)) != 0 ) {
		property_cmd_label_edited (priv->main_window,
					   property,
					   priv->project,
					   priv->owner,
					   new_text);
	}

	gtk_tree_path_free (path);
}

static void
property_dialog_setup_list (GtkWidget *dialog)
{
	PlannerPropertyDialogPriv *priv;
	GtkTreeView               *tree;
	GtkTreeViewColumn         *col;
	GtkCellRenderer           *cell;
	GtkTreeModel              *model;

	priv = g_object_get_data (G_OBJECT (dialog), "priv");

	tree = GTK_TREE_VIEW (priv->tree);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (tree),
				     GTK_SELECTION_SINGLE);

	gtk_tree_view_set_headers_visible (tree, TRUE);

	/* Name */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "editable", FALSE, NULL);

	col = gtk_tree_view_column_new_with_attributes (_("Name"),
							cell,
							"text", COL_NAME,
							NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_min_width (col, 100);
	gtk_tree_view_append_column (tree, col);

	/* Label */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "editable", TRUE, NULL);

	g_signal_connect (cell,
			  "edited",
			  G_CALLBACK (property_dialog_label_edited),
			  dialog);

	col = gtk_tree_view_column_new_with_attributes (_("Label"),
							cell,
							"text", COL_LABEL,
							NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	gtk_tree_view_column_set_min_width (col, 200);
	gtk_tree_view_append_column (tree, col);

	/* Type */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell, "editable", FALSE, NULL);

	col = gtk_tree_view_column_new_with_attributes (_("Type"),
							cell,
							"text", COL_TYPE,
							NULL);
	gtk_tree_view_column_set_resizable (col, TRUE);
	/* gtk_tree_view_column_set_min_width (col, 200); */
	gtk_tree_view_append_column (tree, col);

	/* Create the shop (a type of store). */
	priv->shop = g_new0 (MrpPropertyStore, 1);

	model = planner_property_model_new (priv->project, priv->owner, priv->shop);
	priv->model = model;

	gtk_tree_view_set_model (tree, model);
}

static void
property_dialog_setup_widgets (GtkWidget *dialog,
			       GladeXML  *glade)
{
	PlannerPropertyDialogPriv *priv;
	GtkWidget            *w;

	priv = GET_PRIV (dialog);

	w = glade_xml_get_widget (glade, "close_button");
	g_signal_connect (w,
			  "clicked",
			  G_CALLBACK (property_dialog_close_cb),
			  dialog);

	w = glade_xml_get_widget (glade, "add_button");
	g_signal_connect (w,
			  "clicked",
			  G_CALLBACK (property_dialog_add_cb),
			  dialog);

	w = glade_xml_get_widget (glade, "remove_button");
	g_signal_connect (w,
			  "clicked",
			  G_CALLBACK (property_dialog_remove_cb),
			  dialog);

	w = glade_xml_get_widget (glade, "treeview");
	priv->tree = w;

	property_dialog_setup_list (dialog);
}

GtkWidget *
planner_property_dialog_new (PlannerWindow *main_window,
			     MrpProject    *project,
			     GType          owner,
			     const gchar   *title)
{
	GladeXML                  *glade;
	GtkWidget                 *dialog;
	PlannerPropertyDialogPriv *priv;
	gchar                     *filename;

	g_return_val_if_fail (MRP_IS_PROJECT (project), NULL);

	priv = g_new0 (PlannerPropertyDialogPriv, 1);

	filename = mrp_paths_get_glade_dir ("property-dialog.glade");
	glade = glade_xml_new (filename,
			       NULL, NULL);
	g_free (filename);

	dialog = glade_xml_get_widget (glade, "dialog");

	gtk_window_set_title (GTK_WINDOW (dialog), title);

	g_object_set_data (G_OBJECT (dialog), "priv", priv);

	priv->main_window = main_window;
	priv->project = project;
	priv->owner = owner;

	property_dialog_setup_widgets (dialog, glade);

	g_object_unref (glade);

	return dialog;
}

static gboolean
property_cmd_add_do (PlannerCmd *cmd_base)
{
	PropertyCmdAdd *cmd;
	MrpProperty    *property;

	cmd = (PropertyCmdAdd*) cmd_base;

	/* Why is this check here? */
	if (cmd->name == NULL) {
		return FALSE;
	}

	property = mrp_property_new (cmd->name,
				     cmd->type,
				     cmd->label_text,
				     cmd->description,
				     cmd->user_defined);

	mrp_project_add_property (cmd->project,
				  cmd->owner,
				  property,
				  cmd->user_defined);

	if (!mrp_project_has_property (cmd->project, cmd->owner, cmd->name)) {
		return FALSE;
	}

	return TRUE;
}

static void
property_cmd_add_undo (PlannerCmd *cmd_base)
{
	PropertyCmdAdd *cmd;

	cmd = (PropertyCmdAdd*) cmd_base;

	mrp_project_remove_property (cmd->project,
				     cmd->owner,
				     cmd->name);
}


static void
property_cmd_add_free (PlannerCmd *cmd_base)
{
	PropertyCmdAdd *cmd;

	cmd = (PropertyCmdAdd*) cmd_base;

	g_free (cmd->name);
	g_free (cmd->label_text);
	g_free (cmd->description);
}

static PlannerCmd *
property_cmd_add (PlannerWindow   *window,
		  MrpProject      *project,
		  GType	           owner,
		  const gchar     *name,
		  MrpPropertyType  type,
		  const gchar     *label_text,
		  const gchar     *description,
		  gboolean         user_defined)
{
	PlannerCmd     *cmd_base;
	PropertyCmdAdd *cmd;

	cmd_base = planner_cmd_new (PropertyCmdAdd,
				    _("Add property"),
				    property_cmd_add_do,
				    property_cmd_add_undo,
				    property_cmd_add_free);

	cmd = (PropertyCmdAdd *) cmd_base;

	cmd->window = window;
	cmd->project = project;
	cmd->owner = owner;
	cmd->name = g_strdup (name);
	cmd->type = type;
	cmd->label_text = g_strdup (label_text);
	cmd->description = g_strdup (description);
	cmd->user_defined = user_defined;

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (window),
					   cmd_base);

	return cmd_base;
}


static gboolean
property_cmd_remove_do (PlannerCmd *cmd_base)
{
	PropertyCmdRemove *cmd;

	cmd = (PropertyCmdRemove*) cmd_base;

	if (!mrp_project_has_property (cmd->project, cmd->owner, cmd->name)) {
		return FALSE;
	}

	/* First we need to save the value of the property */
	if (cmd->owner == MRP_TYPE_TASK && g_hash_table_size (cmd->tasks) == 0) {
		GList *l, *tasks = mrp_project_get_all_tasks (cmd->project);

		/* ref tasks */
		for (l = tasks; l; l = l->next) {
			GValue *value = g_new0 (GValue, 1);
			g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (cmd->property));
			mrp_object_get_property (l->data, cmd->property, value);
			g_hash_table_insert (cmd->tasks, g_object_ref (l->data), value);
		}

		g_list_free (tasks);

	}
	else if (cmd->owner == MRP_TYPE_RESOURCE && g_hash_table_size (cmd->resources) == 0) {
		GList *l, *resources = mrp_project_get_resources (cmd->project);

		for (l = resources; l; l = l->next) {
			GValue *value = g_new0 (GValue, 1);
			g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (cmd->property));
			mrp_object_get_property (l->data, cmd->property, value);
			g_hash_table_insert (cmd->resources, g_object_ref (l->data), value);
		}

	}

	mrp_project_remove_property (cmd->project,
				     cmd->owner,
				     cmd->name);

	return TRUE;
}

static void
property_hash_recover  (gpointer key,
			gpointer value,
			gpointer user_data)
{
	PropertyCmdRemove *cmd = (PropertyCmdRemove*) user_data;

	g_assert (MRP_IS_TASK (key) || MRP_IS_RESOURCE (key));
	g_assert (G_IS_VALUE (value));

	mrp_object_set_property (key, cmd->property, value);

}

static void
property_cmd_remove_undo (PlannerCmd *cmd_base)
{
	PropertyCmdRemove *cmd;

	cmd = (PropertyCmdRemove*) cmd_base;

	mrp_property_unref (cmd->property);

	cmd->property = mrp_property_new (cmd->name,
					  cmd->type,
					  cmd->label_text,
					  cmd->description,
					  cmd->user_defined);

	mrp_project_add_property (cmd->project,
				  cmd->owner,
				  cmd->property,
				  cmd->user_defined);

	g_hash_table_foreach (cmd->tasks, property_hash_recover, cmd);
	g_hash_table_foreach (cmd->resources, property_hash_recover, cmd);
}

static void
property_hash_free  (gpointer key,
		     gpointer value,
		     gpointer user_data)
{
	g_object_unref (key);
	g_value_unset ((GValue *) value);
}

static void
property_cmd_remove_free (PlannerCmd *cmd_base)
{
	PropertyCmdRemove *cmd;

	cmd = (PropertyCmdRemove*) cmd_base;

	mrp_property_unref (cmd->property);

	g_hash_table_foreach (cmd->tasks, property_hash_free, NULL);
	g_hash_table_destroy (cmd->tasks);
	g_hash_table_foreach (cmd->resources, property_hash_free, NULL);
	g_hash_table_destroy (cmd->resources);

	g_free (cmd->name);
	g_free (cmd->label_text);
	g_free (cmd->description);

	g_object_unref (cmd->project);
}

static PlannerCmd *
property_cmd_remove (PlannerWindow *window,
		     MrpProject	   *project,
		     GType          owner,
		     const gchar   *name)
{
	PlannerCmd        *cmd_base;
	PropertyCmdRemove *cmd;
	MrpProperty       *property;

	cmd_base = planner_cmd_new (PropertyCmdRemove,
				    _("Remove property"),
				    property_cmd_remove_do,
				    property_cmd_remove_undo,
				    property_cmd_remove_free);

	cmd = (PropertyCmdRemove *) cmd_base;

	cmd->project = g_object_ref (project);
	cmd->owner = owner;
	cmd->name = g_strdup (name);

	property = mrp_project_get_property (project,
					     name,
					     owner);

	cmd->property = mrp_property_ref (property);
	cmd->type = mrp_property_get_property_type (property);
	cmd->description = g_strdup (mrp_property_get_description (property));
	cmd->label_text = g_strdup ( mrp_property_get_label (property));
	cmd->user_defined = mrp_property_get_user_defined (property);

	cmd->tasks = g_hash_table_new (NULL, NULL);
	cmd->resources = g_hash_table_new (NULL, NULL);

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (window),
					   cmd_base);

	return cmd_base;
}

static gboolean
property_cmd_label_edited_do (PlannerCmd *cmd_base)
{
	PropertyCmdLabelEdited *cmd;
	MrpProperty            *property;

	cmd = (PropertyCmdLabelEdited*) cmd_base;

	property = mrp_project_get_property (cmd->project,
					     cmd->name,
					     cmd->owner);

	if (!property) {
		return FALSE;
	}

	mrp_property_set_label (property, cmd->new_text);

	return TRUE;
}


static void
property_cmd_label_edited_undo (PlannerCmd *cmd_base)
{
	PropertyCmdLabelEdited *cmd;
	MrpProperty            *property;

	cmd = (PropertyCmdLabelEdited*) cmd_base;

	property = mrp_project_get_property (cmd->project,
					     cmd->name,
					     cmd->owner);


	if (property != NULL) {
		mrp_property_set_label (property, cmd->old_text);
	}
}

static void
property_cmd_label_edited_free (PlannerCmd *cmd_base)
{
	PropertyCmdLabelEdited *cmd;

	cmd = (PropertyCmdLabelEdited*) cmd_base;

	g_free (cmd->old_text);
	g_free (cmd->new_text);
	g_free (cmd->name);
}

static PlannerCmd *
property_cmd_label_edited (PlannerWindow *window,
			   MrpProperty	 *property,
			   MrpProject	 *project,
			   GType          owner,
			   const gchar 	 *new_text)
{
	PlannerCmd             *cmd_base;
	PropertyCmdLabelEdited *cmd;

	cmd_base = planner_cmd_new (PropertyCmdLabelEdited,
				    _("Edit property label"),
				    property_cmd_label_edited_do,
				    property_cmd_label_edited_undo,
				    property_cmd_label_edited_free);

	cmd = (PropertyCmdLabelEdited *) cmd_base;

	cmd->window = window;
	cmd->new_text = g_strdup (new_text);

	cmd->old_text = g_strdup (mrp_property_get_label (property));
	cmd->project = project;
	cmd->name = g_strdup (mrp_property_get_name (property));
	cmd->owner = owner;

	planner_cmd_manager_insert_and_do (planner_window_get_cmd_manager (window),
					   cmd_base);

	return cmd_base;
}

