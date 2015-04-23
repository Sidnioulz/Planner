/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2005 Imendio AB
 * Copyright (C) 2001-2003 CodeFactory AB
 * Copyright (C) 2001-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2001-2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2002 Alvaro del Castillo <acs@barrapunto.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <time.h>
#include <math.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include "mrp-marshal.h"
#include "mrp-storage-module.h"
#include "mrp-task-manager.h"
#include "mrp-private.h"
#include "mrp-time.h"
#include "mrp-error.h"

struct _MrpTaskManagerPriv {
	MrpProject *project;
	MrpTask    *root;

	gboolean    block_scheduling;

	/* Whether the dependency graph is valid or needs to be rebuilt. */
	gboolean    needs_rebuild;

	/* Whether the task tree needs to be recalculated. */
	gboolean    needs_recalc;
	gboolean    in_recalc;

	GList      *dependency_list;
};

typedef struct {
	MrpTaskTraverseFunc func;
	gpointer            user_data;
} MrpTaskTraverseData;

/* Properties */
enum {
	PROP_0,
	PROP_PROJECT,
};

static void task_manager_class_init       (MrpTaskManagerClass *klass);
static void task_manager_init             (MrpTaskManager      *tm);
static void task_manager_finalize         (GObject             *object);
static void task_manager_set_property     (GObject             *object,
					   guint                prop_id,
					   const GValue        *value,
					   GParamSpec          *pspec);
static void task_manager_get_property     (GObject             *object,
					   guint                prop_id,
					   GValue              *value,
					   GParamSpec          *pspec);
static void
task_manager_task_duration_notify_cb      (MrpTask             *task,
					   GParamSpec          *spec,
					   MrpTaskManager      *manager);
#ifdef WITH_SIMPLE_PRIORITY_SCHEDULING
static void
task_manager_task_priority_notify_cb      (MrpTask             *task,
					   GParamSpec          *spec,
					   MrpTaskManager      *manager);
#endif
static void
task_manager_task_constraint_notify_cb    (MrpTask             *task,
					   GParamSpec          *spec,
					   MrpTaskManager      *manager);
static void
task_manager_project_start_notify_cb      (MrpProject          *project,
					   GParamSpec          *spec,
					   MrpTaskManager      *manager);
static void
task_manager_task_relation_added_cb       (MrpTask             *task,
					   MrpRelation         *relation,
					   MrpTaskManager      *manager);
static void
task_manager_task_relation_removed_cb     (MrpTask             *task,
					   MrpRelation         *relation,
					   MrpTaskManager      *manager);
static void
task_manager_task_assignment_added_cb     (MrpTask             *task,
					   MrpAssignment       *assignment,
					   MrpTaskManager      *manager);
static void
task_manager_task_assignment_removed_cb   (MrpTask             *task,
					   MrpAssignment       *assignment,
					   MrpTaskManager      *manager);
static void
task_manager_task_relation_notify_cb      (MrpRelation         *relation,
					   GParamSpec          *spec,
					   MrpTaskManager      *manager);
static void
task_manager_assignment_units_notify_cb   (MrpAssignment       *assignment,
					   GParamSpec          *spec,
					   MrpTaskManager      *manager);

static void
task_manager_dump_task_tree               (GNode               *node);


static GObjectClass *parent_class;

static mrptime
task_manager_calculate_task_start_from_finish (MrpTaskManager *manager,
						MrpTask        *task,
						mrptime         finish,
						gint           *duration);


GType
mrp_task_manager_get_type (void)
{
	static GType object_type = 0;

	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (MrpTaskManagerClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) task_manager_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (MrpTaskManager),
			0,              /* n_preallocs */
			(GInstanceInitFunc) task_manager_init,
		};

		object_type = g_type_register_static (G_TYPE_OBJECT,
						      "MrpTaskManager",
						      &object_info,
						      0);
	}

	return object_type;
}

static void
task_manager_class_init (MrpTaskManagerClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize     = task_manager_finalize;
	object_class->set_property = task_manager_set_property;
	object_class->get_property = task_manager_get_property;

	/* Properties. */
	g_object_class_install_property (
		object_class,
		PROP_PROJECT,
		g_param_spec_object ("project",
				     "Project",
				     "The project that this task manager is attached to",
				     G_TYPE_OBJECT,
				     G_PARAM_READWRITE |
				     G_PARAM_CONSTRUCT_ONLY));
}

static void
task_manager_init (MrpTaskManager *man)
{
	MrpTaskManagerPriv *priv;

	man->priv = g_new0 (MrpTaskManagerPriv, 1);
	priv = man->priv;

	priv->needs_recalc = TRUE;
	priv->needs_rebuild = TRUE;
}

static void
task_manager_finalize (GObject *object)
{
	MrpTaskManager *manager = MRP_TASK_MANAGER (object);

	g_object_unref (manager->priv->root);

	g_free (manager->priv);

	if (G_OBJECT_CLASS (parent_class)->finalize) {
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
	}
}

static void
task_manager_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	MrpTaskManager     *manager;
	MrpTaskManagerPriv *priv;

	manager = MRP_TASK_MANAGER (object);
	priv = manager->priv;

	switch (prop_id) {
	case PROP_PROJECT:
		priv->project = g_value_get_object (value);
		g_signal_connect (priv->project,
				  "notify::project-start",
				  G_CALLBACK (task_manager_project_start_notify_cb),
				  manager);

		mrp_task_manager_set_root (manager, mrp_task_new ());
		break;

	default:
		break;
	}
}

static void
task_manager_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	MrpTaskManager *manager;

	manager = MRP_TASK_MANAGER (object);

	switch (prop_id) {
	case PROP_PROJECT:
		g_value_set_object (value, manager->priv->project);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

MrpTaskManager *
mrp_task_manager_new (MrpProject *project)
{
	MrpTaskManager *manager;

	manager = g_object_new (MRP_TYPE_TASK_MANAGER,
				"project", project,
				NULL);

	return manager;
}

static void
task_manager_task_connect_signals (MrpTaskManager *manager,
				   MrpTask        *task)
{
	g_signal_connect (task,
			  "notify::duration",
			  G_CALLBACK (task_manager_task_duration_notify_cb),
			  manager);
#ifdef WITH_SIMPLE_PRIORITY_SCHEDULING
	/* Added to manage the transaction from normal to dominant. */
	g_signal_connect (task,
			  "notify::priority",
			  G_CALLBACK (task_manager_task_priority_notify_cb),
			  manager);
#endif
	g_signal_connect (task,
			  "notify::constraint",
			  G_CALLBACK (task_manager_task_constraint_notify_cb),
			  manager);
	g_signal_connect (task,
			  "relation_added",
			  G_CALLBACK (task_manager_task_relation_added_cb),
			  manager);
	g_signal_connect (task,
			  "relation_removed",
			  G_CALLBACK (task_manager_task_relation_removed_cb),
			  manager);
	g_signal_connect (task,
			  "assignment_added",
			  G_CALLBACK (task_manager_task_assignment_added_cb),
			  manager);
	g_signal_connect (task,
			  "assignment_removed",
			  G_CALLBACK (task_manager_task_assignment_removed_cb),
			  manager);
}

/**
 * mrp_task_manager_insert_task:
 * @manager: A task manager
 * @parent: The parent to insert the task beneath.
 * @position: the position to place task at, with respect to its siblings.
 * If position is -1, task is inserted as the last child of parent.
 * @task: The task to insert.
 *
 * Inserts a task beneath the parent at the given position.
 *
 **/
void
mrp_task_manager_insert_task (MrpTaskManager *manager,
			      MrpTask        *parent,
			      gint            position,
			      MrpTask        *task)
{
	g_return_if_fail (MRP_IS_TASK_MANAGER (manager));
	g_return_if_fail (parent == NULL || MRP_IS_TASK (parent));
	g_return_if_fail (MRP_IS_TASK (task));

	if (parent == NULL) {
		parent = manager->priv->root;
	}

	g_object_set (task,
		      "project", manager->priv->project,
		      NULL);

	imrp_task_insert_child (parent, position, task);

	/* FIXME: implement adding the task to the dependency graph instead. */
	manager->priv->needs_rebuild = TRUE;

	manager->priv->needs_recalc = TRUE;

	imrp_project_task_inserted (manager->priv->project, task);

	mrp_task_manager_recalc (manager, TRUE);

	task_manager_task_connect_signals (manager, task);
}

/**
 * mrp_task_manager_remove_task:
 * @manager: A task manager
 * @task: The task to remove.
 *
 * Removes a task, or a task subtree if the task has children. The root task
 * (with id 0) cannot be removed.
 *
 **/
void
mrp_task_manager_remove_task (MrpTaskManager *manager,
			      MrpTask        *task)
{
	g_return_if_fail (MRP_IS_TASK_MANAGER (manager));
	g_return_if_fail (MRP_IS_TASK (task));

	if (task == manager->priv->root) {
		g_warning ("Can't remove root task");
		return;
	}

	g_object_set (task,
		      "project", NULL,
		      NULL);

	imrp_task_remove_subtree (task);

	manager->priv->needs_rebuild = TRUE;
	mrp_task_manager_recalc (manager, FALSE);
}

static gboolean
task_manager_get_all_tasks_cb (GNode *node, GList **list)
{
	/* Don't add the root. */
	if (node->parent != NULL) {
		*list = g_list_prepend (*list, node->data);
	}

	return FALSE; /* don't stop the traversal */
}

/**
 * mrp_task_manager_get_all_tasks:
 * @manager: A task manager
 *
 * Gets all the tasks in the project.
 *
 * Return value: A list of all the MrpTasks in the project.
 **/
GList *
mrp_task_manager_get_all_tasks (MrpTaskManager *manager)
{
	GList   *tasks;

	g_return_val_if_fail (MRP_IS_TASK_MANAGER (manager), NULL);

	if (manager->priv->root == NULL) {
		return NULL;
	}

	tasks = NULL;

	g_node_traverse (imrp_task_get_node (manager->priv->root),
			 G_PRE_ORDER,
			 G_TRAVERSE_ALL,
			 -1,
			 (GNodeTraverseFunc) task_manager_get_all_tasks_cb,
			 &tasks);

	tasks = g_list_reverse (tasks);

	return tasks;
}

static gboolean
task_manager_traverse_cb (GNode *node, MrpTaskTraverseData *data)
{
	return data->func (node->data, data->user_data);
}

/**
 * mrp_task_manager_traverse:
 * @manager: A task manager
 * @root: The task to start traversing
 * @func: A function to call for each traversed task
 * @user_data: Argument to pass to the callback
 *
 * Calls %func for the subtree starting at %root, until @func returns %TRUE.
 *
 **/
void
mrp_task_manager_traverse (MrpTaskManager      *manager,
			   MrpTask             *root,
			   MrpTaskTraverseFunc  func,
			   gpointer             user_data)
{
	MrpTaskTraverseData data;

	g_return_if_fail (MRP_IS_TASK_MANAGER (manager));
	g_return_if_fail (MRP_IS_TASK (root));

	data.func = func;
	data.user_data = user_data;

	g_node_traverse (imrp_task_get_node (root),
			 G_PRE_ORDER,
			 G_TRAVERSE_ALL,
			 -1,
			 (GNodeTraverseFunc) task_manager_traverse_cb,
			 &data);
}

void
mrp_task_manager_set_root (MrpTaskManager *manager,
			   MrpTask        *task)
{
	GList      *tasks, *l;
	MrpProject *project;

	g_return_if_fail (MRP_IS_TASK_MANAGER (manager));
	g_return_if_fail (task == NULL || MRP_IS_TASK (task));

	if (manager->priv->root != NULL) {
		imrp_task_remove_subtree (manager->priv->root);
	}

	manager->priv->root = task;

	project = manager->priv->project;

	tasks = mrp_task_manager_get_all_tasks (manager);
	for (l = tasks; l; l = l->next) {
		g_object_set (l->data, "project", project, NULL);

		task_manager_task_connect_signals (manager, l->data);
	}

	mrp_task_manager_recalc (manager, FALSE);

	g_object_set (task,
		      "project", project,
		      NULL);

	g_list_free (tasks);
}

MrpTask *
mrp_task_manager_get_root (MrpTaskManager *manager)
{
	g_return_val_if_fail (MRP_IS_TASK_MANAGER (manager), NULL);

	return manager->priv->root;
}

gboolean
mrp_task_manager_move_task (MrpTaskManager  *manager,
			    MrpTask         *task,
			    MrpTask         *sibling,
			    MrpTask         *parent,
			    gboolean         before,
			    GError         **error)
{
	MrpTask *old_parent;

	g_return_val_if_fail (MRP_IS_TASK_MANAGER (manager), FALSE);
	g_return_val_if_fail (MRP_IS_TASK (task), FALSE);
	g_return_val_if_fail (sibling == NULL || MRP_IS_TASK (sibling), FALSE);
	g_return_val_if_fail (MRP_IS_TASK (parent), FALSE);

	old_parent = mrp_task_get_parent (task);

	if (!mrp_task_manager_check_move (manager,
					  task,
					  parent,
					  error)) {
		return FALSE;
	}

	imrp_task_detach (task);
	imrp_task_reattach (task, sibling, parent, before);

	mrp_task_invalidate_cost (old_parent);
	mrp_task_invalidate_cost (parent);

	mrp_task_manager_rebuild (manager);

	imrp_project_task_moved (manager->priv->project, task);

	mrp_task_manager_recalc (manager, FALSE);

	return TRUE;
}

/* ----------------------------------------------------------------------------
 * Test code. Remove at some stage, or move to test framework.
 */

static gchar*
get_n_chars (gint n, gchar c)
{
	GString *str;
	gchar   *ret;
	gint     i;

	str = g_string_new ("");

	for (i = 0; i < n; i++) {
		g_string_append_c (str, c);
	}

	ret = str->str;
	g_string_free (str, FALSE);

	return ret;
}

static void
dump_children (GNode *node, gint depth)
{
	GNode       *child;
	gchar       *padding;
	MrpTask     *task;
	const gchar *name;

	padding = get_n_chars (2 * depth, ' ');

	for (child = g_node_first_child (node); child; child = g_node_next_sibling (child)) {
		task = (MrpTask *) child->data;

		if (MRP_IS_TASK (task)) {
			name = mrp_task_get_name (task);
			g_print ("%sName: %s   ", padding, name);

			if (imrp_task_peek_predecessors (task)) {
				GList *l;
				g_print (" <-[");
				for (l = imrp_task_peek_predecessors (task); l; l = l->next) {
					MrpTask *predecessor = mrp_relation_get_predecessor (l->data);

					if (MRP_IS_TASK (predecessor)) {
						name = mrp_task_get_name (predecessor);
						g_print ("%s, ", name);
					} else {
						g_print ("<unknown>, ");
					}
				}
				g_print ("]");
			}

			if (imrp_task_peek_successors (task)) {
				GList *l;
				g_print (" ->[");
				for (l = imrp_task_peek_successors (task); l; l = l->next) {
					MrpTask *successor = mrp_relation_get_successor (l->data);

					if (MRP_IS_TASK (successor)) {
						name = mrp_task_get_name (successor);
						g_print ("%s, ", name);
					} else {
						g_print ("<unknown>, ");
					}
				}
				g_print ("]");
			}

			g_print ("\n");
		} else {
			g_print ("%s<unknown>\n", padding);
		}

		dump_children (child, depth + 1);
	}

	g_free (padding);
}

static void
task_manager_dump_task_tree (GNode *node)
{
	g_return_if_fail (node != NULL);
	g_return_if_fail (node->parent == NULL);

	g_print ("------------------------------------------\n<Root>\n");

	dump_children (node, 1);

	g_print ("\n");
}

void
mrp_task_manager_dump_task_tree (MrpTaskManager *manager)
{
	g_return_if_fail (MRP_IS_TASK_MANAGER (manager));
	g_return_if_fail (manager->priv->root);

	task_manager_dump_task_tree (imrp_task_get_node (manager->priv->root));
}

void
mrp_task_manager_dump_task_list (MrpTaskManager *manager)
{
	GList       *list, *l;
	const gchar *name;

	g_return_if_fail (MRP_IS_TASK_MANAGER (manager));
	g_return_if_fail (manager->priv->root);

	g_print ("All tasks: ");
	list = mrp_task_manager_get_all_tasks (manager);
	for (l = list; l; l = l->next) {
		if (l != list) {
			g_print (", ");
		}

		if (MRP_IS_TASK (l->data)) {
			name = mrp_task_get_name (l->data);
			g_print ("%s", name);
		} else {
			g_print ("<unknown>");
		}
	}
	g_print ("\n");

	g_list_free (list);
}

/* ------------------------------------------------------------------------ */

/* Functions have not been used since revision 417 */
#if 0

/* Get the ancestor of task_a, that has the same parent as an ancestor or
 * task_b.
 */
static MrpTask *
task_manager_get_ancestor_with_same_parent (MrpTask *task_a, MrpTask *task_b)
{
	MrpTask *ancestor;
	gint     depth_a, depth_b, i;

	depth_a = imrp_task_get_depth (task_a);
	depth_b = imrp_task_get_depth (task_b);

	if (depth_a > depth_b) {
		for (i = depth_a; i > depth_b; i--) {
			task_a = mrp_task_get_parent (task_a);
		}
	}
	else if (depth_a < depth_b) {
		for (i = depth_b; i > depth_a; i--) {
			task_b = mrp_task_get_parent (task_b);
		}
	}

	ancestor = NULL;
	while (task_a != NULL && task_b != NULL) {

		if (mrp_task_get_parent (task_a) == mrp_task_get_parent (task_b)) {
			ancestor = task_a;
			break;
		}

		task_a = mrp_task_get_parent (task_a);
		task_b = mrp_task_get_parent (task_b);
	}

	return ancestor;
}

static void
task_manager_traverse_dependency_graph (MrpTaskManager  *manager,
					MrpTask         *task,
					GList          **output)
{
	GList       *l;
	MrpRelation *relation;
	MrpTask     *ancestor;
	MrpTask     *child;

	if (imrp_task_get_visited (task)) {
		/*g_warning ("Visited!!\n");*/
		return;
	}

	imrp_task_set_visited (task, TRUE);

	/* Follow successors. */
	for (l = imrp_task_peek_successors (task); l; l = l->next) {
		relation = l->data;

		task_manager_traverse_dependency_graph (manager,
							mrp_relation_get_successor (relation),
							output);

		/* Also follow the ancestor of the successor that has the same
		 * parent as an ancestor of the task. This way we make sure that
		 * predecessors are listed before the summary tasks of the
		 * successors.
		 */
		ancestor = task_manager_get_ancestor_with_same_parent (mrp_relation_get_successor (relation),
								       task);

		if (ancestor != NULL) {
			task_manager_traverse_dependency_graph (manager,
								ancestor,
								output);
		}
	}

	/* Follow parent -> child. */
	child = mrp_task_get_first_child (task);
	while (child) {
		task_manager_traverse_dependency_graph (manager, child, output);

		child = mrp_task_get_next_sibling (child);
	}

	if (task != manager->priv->root) {
		g_print ("Adding: %s\n", mrp_task_get_name (task));
		*output = g_list_prepend (*output, task);
	}
}
#endif

static void
dump_task_node (MrpTask *task)
{
	MrpTaskGraphNode *node;
	GList            *l;

	node = imrp_task_get_graph_node (task);

	g_print ("Task: %s\n", mrp_task_get_name (task));

	for (l = node->prev; l; l = l->next) {
		g_print (" from %s\n", mrp_task_get_name (l->data));
	}

	for (l = node->next; l; l = l->next) {
		g_print (" to %s\n", mrp_task_get_name (l->data));
	}
}

static void
dump_all_task_nodes (MrpTaskManager *manager)
{
	GList *tasks, *l;

	tasks = mrp_task_manager_get_all_tasks (manager);
	for (l = tasks; l; l = l->next) {
		dump_task_node (l->data);
	}

	g_list_free (tasks);
}

static void
add_predecessor_to_dependency_graph_recursive (MrpTask *task,
					       MrpTask *predecessor)
{
	MrpTaskGraphNode *predecessor_node;
	MrpTask          *child;
	MrpTaskGraphNode *child_node;

	predecessor_node = imrp_task_get_graph_node (predecessor);

	child = mrp_task_get_first_child (task);
	while (child) {
		child_node = imrp_task_get_graph_node (child);

		child_node->prev = g_list_append (child_node->prev, predecessor);
		predecessor_node->next = g_list_append (predecessor_node->next, child);

		if (mrp_task_get_n_children (child) > 0) {
			add_predecessor_to_dependency_graph_recursive (child, predecessor);
		}

		child = mrp_task_get_next_sibling (child);
	}
}

static void
add_predecessor_to_dependency_graph (MrpTaskManager *manager,
				     MrpTask        *task,
				     MrpTask        *predecessor)
{
	MrpTaskGraphNode   *task_node;
	MrpTaskGraphNode   *predecessor_node;

	predecessor_node = imrp_task_get_graph_node (predecessor);

	task_node = imrp_task_get_graph_node (task);

	task_node->prev = g_list_append (task_node->prev, predecessor);
	predecessor_node->next = g_list_append (predecessor_node->next, task);

	/* Add dependencies from the predecessor to the task's children,
	 * recursively.
	 */
	add_predecessor_to_dependency_graph_recursive (task, predecessor);
}

static void
remove_predecessor_from_dependency_graph_recursive (MrpTask *task,
						    MrpTask *predecessor)
{
	MrpTaskGraphNode *predecessor_node;
	MrpTask          *child;
	MrpTaskGraphNode *child_node;

	predecessor_node = imrp_task_get_graph_node (predecessor);

	child = mrp_task_get_first_child (task);
	while (child) {
		child_node = imrp_task_get_graph_node (child);

		child_node->prev = g_list_remove (child_node->prev, predecessor);
		predecessor_node->next = g_list_remove (predecessor_node->next, child);

		if (mrp_task_get_n_children (child) > 0) {
			remove_predecessor_from_dependency_graph_recursive (child, predecessor);
		}

		child = mrp_task_get_next_sibling (child);
	}
}

static void
remove_predecessor_from_dependency_graph (MrpTaskManager *manager,
					  MrpTask        *task,
					  MrpTask        *predecessor)
{
	MrpTaskGraphNode   *task_node;
	MrpTaskGraphNode   *predecessor_node;

	predecessor_node = imrp_task_get_graph_node (predecessor);

	task_node = imrp_task_get_graph_node (task);

	task_node->prev = g_list_remove (task_node->prev, predecessor);
	predecessor_node->next = g_list_remove (predecessor_node->next, task);

	/* Remove dependencies from the predecessor to the task's children,
	 * recursively.
	 */
	remove_predecessor_from_dependency_graph_recursive (task, predecessor);
}

static void
remove_parent_predecessors_from_dependency_graph (MrpTask *task,
						  MrpTask *parent)
{
	MrpTask            *predecessor;
	MrpTaskGraphNode   *predecessor_node;
	MrpTaskGraphNode   *task_node;
	GList              *list, *l;
	MrpRelation        *relation;

	/* Remove parent's predecessors from task and all of its children. */
	list = imrp_task_peek_predecessors (parent);
	for (l = list; l; l = l->next) {
		relation = l->data;
		predecessor = mrp_relation_get_predecessor (relation);

		/* Remove predecessor from task */
		predecessor_node = imrp_task_get_graph_node (predecessor);
		predecessor_node->next = g_list_remove (predecessor_node->next, task);
		task_node = imrp_task_get_graph_node (task);
		task_node->prev = g_list_remove (task_node->prev, predecessor);

		/* Remove predecessor from all its children */
		if (mrp_task_get_n_children (task) > 0) {
			remove_predecessor_from_dependency_graph_recursive (task, predecessor);
		}
	}
}

static void
remove_parent_from_dependency_graph (MrpTaskManager *manager,
				     MrpTask        *task,
				     MrpTask        *parent)
{
	MrpTaskGraphNode   *task_node;
	MrpTaskGraphNode   *parent_node;

	task_node = imrp_task_get_graph_node (task);
	parent_node = imrp_task_get_graph_node (parent);

	task_node->next = g_list_remove (task_node->next, parent);
	parent_node->prev = g_list_remove (parent_node->prev, task);

	/* Remove dependencies from the parent's predecessors from the task and all
	 * its children.
	 */
	remove_parent_predecessors_from_dependency_graph (task, parent);
}

static void
add_parent_predecessors_to_dependency_graph (MrpTask *task,
					     MrpTask *parent)
{
	MrpTask            *predecessor;
	MrpTaskGraphNode   *predecessor_node;
	MrpTaskGraphNode   *task_node;
	GList              *list, *l;
	MrpRelation        *relation;

	/* Add parent's predecessors to task and all of its children. */
	list = imrp_task_peek_predecessors (parent);
	for (l = list; l; l = l->next) {
		relation = l->data;
		predecessor = mrp_relation_get_predecessor (relation);

		/* Add predecessor to task */
		predecessor_node = imrp_task_get_graph_node (predecessor);
		predecessor_node->next = g_list_append (predecessor_node->next, task);
		task_node = imrp_task_get_graph_node (task);
		task_node->prev = g_list_append (task_node->prev, predecessor);

		/* Add predecessor to all its children */
		if (mrp_task_get_n_children (task) > 0) {
			add_predecessor_to_dependency_graph_recursive (task, predecessor);
		}
	}
}

static void
add_parent_to_dependency_graph (MrpTaskManager *manager,
				MrpTask        *task,
				MrpTask        *parent)
{
	MrpTaskGraphNode   *task_node;
	MrpTaskGraphNode   *parent_node;

	task_node = imrp_task_get_graph_node (task);
	parent_node = imrp_task_get_graph_node (parent);

	task_node->next = g_list_append (task_node->next, parent);
	parent_node->prev = g_list_append (parent_node->prev, task);

	/* Add dependencies from the parent's predecessors to the task and all
	 * its children.
	 */
	add_parent_predecessors_to_dependency_graph (task, parent);
}

static void
remove_task_from_dependency_graph (MrpTaskManager *manager,
				   MrpTask        *task,
				   MrpTask        *parent)
{
	MrpTaskManagerPriv *priv;
	GList              *list, *l;
	MrpRelation        *relation;
	MrpTask            *predecessor;

	priv = manager->priv;

	/* Remove predecessors. */
	list = imrp_task_peek_predecessors (task);
	for (l = list; l; l = l->next) {
		relation = l->data;
		predecessor = mrp_relation_get_predecessor (relation);

		remove_predecessor_from_dependency_graph (manager, task, predecessor);
	}

	/* Remove the parent. */
	if (parent && parent != priv->root) {
		remove_parent_from_dependency_graph (manager, task, parent);
	}
}

static void
add_task_to_dependency_graph (MrpTaskManager *manager,
			      MrpTask        *task,
			      MrpTask        *parent)
{
	MrpTaskManagerPriv *priv;
	GList              *list, *l;
	MrpRelation        *relation;
	MrpTask            *predecessor;

	priv = manager->priv;

	if (task == priv->root) {
		return;
	}

	/* Add predecessors. */
	list = imrp_task_peek_predecessors (task);
	for (l = list; l; l = l->next) {
		relation = l->data;
		predecessor = mrp_relation_get_predecessor (relation);

		add_predecessor_to_dependency_graph (manager, task, predecessor);
	}

	/* Add the parent. */
	if (parent && parent != priv->root) {
		add_parent_to_dependency_graph (manager, task, parent);
	}
}

static gboolean
task_manager_unset_visited_func (MrpTask  *task,
				 gpointer  user_data)
{
	imrp_task_set_visited (task, FALSE);

	return FALSE;
}

static gboolean
task_manager_clean_graph_func (MrpTask  *task,
			       gpointer  user_data)
{
	MrpTaskGraphNode *node;

	imrp_task_set_visited (task, FALSE);

	node = imrp_task_get_graph_node (task);

	g_list_free (node->prev);
	node->prev = NULL;

	g_list_free (node->next);
	node->next = NULL;

	return FALSE;
}

static void
task_manager_build_dependency_graph (MrpTaskManager *manager)
{
	MrpTaskManagerPriv *priv;
	GList              *tasks;
	GList              *l;
	GList              *deps;
	MrpTask            *task;
	MrpTaskGraphNode   *node;
	GList              *queue;

	priv = manager->priv;

	/* Build a directed, acyclic graph, where relation links and children ->
	 * parent are graph links (children must be calculated before
	 * parents). Then do topological sorting on the graph to get the order
	 * to go through the tasks.
	 */

	mrp_task_manager_traverse (manager,
				   priv->root,
				   task_manager_clean_graph_func,
				   NULL);

	/* FIXME: Optimize by not getting all tasks but just traversing and
	 * adding them that way. Saves a constant factor.
	 */
	tasks = mrp_task_manager_get_all_tasks (manager);
	for (l = tasks; l; l = l->next) {
		add_task_to_dependency_graph (manager, l->data, mrp_task_get_parent (l->data));
	}

	/* Do a topological sort. Get the tasks without dependencies to start
	 * with.
	 */
	queue = NULL;
	for (l = tasks; l; l = l->next) {
		task = l->data;

		node = imrp_task_get_graph_node (task);

		if (node->prev == NULL) {
			queue = g_list_prepend (queue, task);
		}
	}

	deps = NULL;
	while (queue) {
		GList *next, *link;

		task = queue->data;

		link = queue;
		queue = g_list_remove_link (queue, link);

		link->next = deps;
		if (deps) {
			deps->prev = link;
		}
		deps = link;

		/* Remove this task from all the dependent tasks. */
		node = imrp_task_get_graph_node (task);
		for (next = node->next; next; next = next->next) {
			MrpTaskGraphNode *next_node;

			next_node = imrp_task_get_graph_node (next->data);
			next_node->prev = g_list_remove (next_node->prev, task);

			/* Add the task to the output queue if it has no
			 * dependencies left.
			 */
			if (next_node->prev == NULL) {
				queue = g_list_prepend (queue, next->data);
			}
		}
	}

	g_list_free (priv->dependency_list);
	priv->dependency_list = g_list_reverse (deps);

	g_list_free (queue);
	g_list_free (tasks);

	mrp_task_manager_traverse (manager,
				   priv->root,
				   task_manager_unset_visited_func,
				   NULL);

	manager->priv->needs_rebuild = FALSE;
	manager->priv->needs_recalc = TRUE;
}

/* Calculate the start time of the task by finding the latest finish of it's
 * predecessors (plus any lag). Also take constraints into consideration.
 */
static mrptime
task_manager_calculate_task_start (MrpTaskManager *manager,
				   MrpTask        *task,
				   gint           *duration)
{
	MrpTaskManagerPriv *priv;
	MrpTask            *tmp_task;
	GList              *predecessors, *l;
	MrpRelation        *relation;
	MrpRelationType     type;
	MrpTask            *predecessor;
	mrptime             project_start;
	mrptime             start;
	mrptime             finish;
	mrptime             dep_start;
	MrpConstraint       constraint;

	priv = manager->priv;

	project_start = mrp_project_get_project_start (priv->project);
	start = project_start;

	tmp_task = task;
	while (tmp_task) {
		predecessors = imrp_task_peek_predecessors (tmp_task);
		for (l = predecessors; l; l = l->next) {
			relation = l->data;
			predecessor = mrp_relation_get_predecessor (relation);

			type = mrp_relation_get_relation_type (relation);

			switch (type) {
			case MRP_RELATION_FF:
				/* finish-to-finish */
				/* predecessor must finish before successor can finish */
				finish = mrp_task_get_finish (predecessor) + mrp_relation_get_lag (relation);
				start =  task_manager_calculate_task_start_from_finish (manager,
											task,
											finish,
											duration);
				dep_start = start;

				break;

			case MRP_RELATION_SF:
				/* start-to-finish */
				/* predecessor must start before successor can finish */
				finish = mrp_task_get_start (predecessor);
				start =  task_manager_calculate_task_start_from_finish (manager,
											task,
											finish,
											duration);

				dep_start = mrp_task_get_start (predecessor) +
					    mrp_relation_get_lag (relation) - (finish - start);
				break;

			case MRP_RELATION_SS:
				/* start-to-start */
				/* predecessor must start before successor can start */
				dep_start = mrp_task_get_start (predecessor) +
					    mrp_relation_get_lag (relation);
				break;

			case MRP_RELATION_FS:
			case MRP_RELATION_NONE:
			default:
				/* finish-to-start */
				/* predecessor must finish before successor can start */
				dep_start = mrp_task_get_finish (predecessor) +
					mrp_relation_get_lag (relation);
				break;
			}

			start = MAX (start, dep_start);
		}

		tmp_task = mrp_task_get_parent (tmp_task);
	}

	/* Take constraint types in consideration. */
	constraint = imrp_task_get_constraint (task);
	switch (constraint.type) {
	case MRP_CONSTRAINT_SNET:
		/* Start-no-earlier-than. */
		start = MAX (start, constraint.time);
		break;

	case MRP_CONSTRAINT_MSO:
		/* Must-start-on. */
		start = MAX (project_start, constraint.time);
		break;

	case MRP_CONSTRAINT_ASAP:
		/* As-soon-as-possible, do nothing. */
		break;

	case MRP_CONSTRAINT_ALAP:
	case MRP_CONSTRAINT_FNLT:
	default:
		g_warning ("Constraint %d not implemented yet.", constraint.type);
		break;
	}

	return start;
}

/* NOTE: MrpUnitsInterval moved in mrp-task.h to enable
         other objects to use it. */
static gint
units_interval_sort_func (gconstpointer a, gconstpointer b)
{
	MrpUnitsInterval *ai = *(MrpUnitsInterval **) a;
	MrpUnitsInterval *bi = *(MrpUnitsInterval **) b;
	mrptime       at, bt;

	if (ai->is_start) {
		at = ai->start;
	} else {
		at = ai->end;
	}

	if (bi->is_start) {
		bt = bi->start;
	} else {
		bt = bi->end;
	}

	if (at < bt) {
		return -1;
	}
	else if (at > bt) {
		return 1;
	} else {
		return 0;
	}
}

static MrpUnitsInterval *
units_interval_new (MrpInterval *ival, gint units, gboolean is_start)
{
	MrpUnitsInterval *unit_ival;

	unit_ival = g_new (MrpUnitsInterval, 1);
	unit_ival->is_start = is_start;
	unit_ival->units = units;

	mrp_interval_get_absolute (ival, 0, &unit_ival->start, &unit_ival->end);

	return unit_ival;
}

/* Get all the intervals from all the assigned resource of this task, for a
 * certain day. Then we split them up in subintervals, at every point in time
 * where an interval is starting or ending.
 *
 * Then we need to merge all points that point at the same time and get the
 * total units at that point, the resulting list is the return value from this
 * function.
 */
static GList *
task_manager_get_task_units_intervals (MrpTaskManager *manager,
				       MrpTask        *task,
				       mrptime         date)
{
	MrpTaskManagerPriv *priv;
	MrpCalendar        *calendar;
	MrpDay             *day;
	GList              *ivals, *l;
	MrpInterval        *ival;

	MrpUnitsInterval   *unit_ival, *new_unit_ival;
	MrpUnitsInterval   *unit_ival_start, *unit_ival_end;
	GList              *unit_ivals = NULL;
	MrpAssignment      *assignment;
	MrpResource        *resource;
	GList              *assignments, *a;
	gint                units, units_full, units_orig;
	mrptime             i_start, i_end;

	mrptime             t;
	mrptime             poc;
	GPtrArray          *array;
	guint               len;
	gint                i;

	gint     res_n;

#ifdef WITH_SIMPLE_PRIORITY_SCHEDULING
	MrpInterval        *ival_split;
	MrpUnitsInterval   *unit_ival_start_cmp;
	MrpUnitsInterval   *split_unit_ival;
	MrpAssignment      *v_assignment;
	MrpResource        *v_resource;
	GList              *v_assignments, *v_a;
	gint                v_units;
	GList              *v_tasks, *v_l;
	GPtrArray          *array_split;
	gint                e, lastct;
	mrptime             v_start, v_end;
	mrptime             i_start_post, i_end_post, i_start_cmp, i_end_cmp;
#endif

	priv = manager->priv;

	assignments = mrp_task_get_assignments (task);

	array = g_ptr_array_new ();

#ifdef WITH_SIMPLE_PRIORITY_SCHEDULING
	v_tasks = mrp_task_manager_get_all_tasks (manager);
#endif

	for (a = assignments; a; a = a->next) {
		assignment = a->data;

		resource = mrp_assignment_get_resource (assignment);
		units_orig = mrp_assignment_get_units (assignment);

		calendar = mrp_resource_get_calendar (resource);
		if (!calendar) {
			calendar = mrp_project_get_calendar (priv->project);
		}
		day = mrp_calendar_get_day (calendar, date, TRUE);
		ivals = mrp_calendar_day_get_intervals (calendar, day, TRUE);

		for (l = ivals; l; l = l->next) {
			ival = l->data;
			mrp_interval_get_absolute (ival, date, &i_start, &i_end);
			units = units_orig;

#ifdef WITH_SIMPLE_PRIORITY_SCHEDULING
			for (v_l = v_tasks; v_l; v_l = v_l->next) {
				MrpTask *v_task;

				v_task =  v_l->data;

				if (v_task == task) {
					continue;
				}

				if (mrp_task_is_dominant (v_task) == FALSE) {
					continue;
				}

				/* If intervals not overlapped -> contine. */
				v_start = mrp_task_get_work_start (v_task);
				v_end = mrp_task_get_finish (v_task);

				if (i_start > v_end || i_end < v_start) {
					continue;
				}

				/* Compare resources task with resources of dominant task. */
				v_assignments = mrp_task_get_assignments (v_task);

				for (v_a = v_assignments; v_a; v_a = v_a->next) {
					v_assignment = v_a->data;

					v_resource = mrp_assignment_get_resource (v_assignment);
					if (v_resource == resource) {
						v_units = mrp_assignment_get_units (v_assignment);
						/*
						   If the dominant cost is compatible with the task
						   request -> break.

						   FIXME - tasks that share the vampirised resource not work!
						*/
						if (100 - v_units > units) {
							break;
						}

						/* Trim the interval of the dominant task. */
						v_start = (v_start < i_start ?
								   i_start : v_start);
						v_end = (v_end > i_end ?
								 i_end : v_end);

						if (i_start < v_start) {
							/*
							     ----...
							   ------...
							   ival len from start to dominant
							*/
							ival = mrp_interval_new (i_start-date, v_start-date);

							unit_ival_start = units_interval_new (ival, units, TRUE);
							unit_ival_start->units_full = units;
							unit_ival_end = units_interval_new (ival, units, FALSE);
							unit_ival_end->units_full = units;
							g_ptr_array_add (array, unit_ival_start);
							g_ptr_array_add (array, unit_ival_end);
						}

						ival = mrp_interval_new (v_start-date, v_end-date);

						unit_ival_start = units_interval_new (ival, (100 - v_units), TRUE);
						unit_ival_start->units_full = units;
						unit_ival_end = units_interval_new (ival, (100 - v_units), FALSE);
						unit_ival_end->units_full = units;
						g_ptr_array_add (array, unit_ival_start);
						g_ptr_array_add (array, unit_ival_end);

						if (v_end < i_end) {
							/*
							   ----  ...
							   ------...
							   ival len from end to dominant
							*/
							ival = mrp_interval_new (v_end-date, i_end-date);

							unit_ival_start = units_interval_new (ival, units, TRUE);
							unit_ival_start->units_full = units;
							unit_ival_end = units_interval_new (ival, units, FALSE);
							unit_ival_end->units_full = units;
							g_ptr_array_add (array, unit_ival_start);
							g_ptr_array_add (array, unit_ival_end);
						}
						break;
					}
				} /* for (v_a = v_assignments; v_a; ... */
				if (v_a != NULL) {
					break;
				}
			} /* for (v_l = v_tasks; v_l; ... */


			if (v_l == NULL) {
#endif /* ifdef WITH_SIMPLE_PRIORITY_SCHEDULING */
			/* Start of the interval. */
				unit_ival_start = units_interval_new (ival, units, TRUE);
				unit_ival_start->units_full = units;

			/* End of the interval. */
				unit_ival_end = units_interval_new (ival, units, FALSE);
				unit_ival_end->units_full = units;

				g_ptr_array_add (array, unit_ival_start);
				g_ptr_array_add (array, unit_ival_end);
#ifdef WITH_SIMPLE_PRIORITY_SCHEDULING
			}
#endif
		} /* for (l = ivals; l; ... */
	} /* for (a = assignments; a; ... */

#ifdef WITH_SIMPLE_PRIORITY_SCHEDULING
	g_list_free (v_tasks);
#endif

	/* If the task is not allocated, we handle it as if we have one resource
	 * assigned to it, 100%, using the project calendar.
	 */
	if (!assignments) {
		calendar = mrp_project_get_calendar (priv->project);

		day = mrp_calendar_get_day (calendar, date, TRUE);
		ivals = mrp_calendar_day_get_intervals (calendar, day, TRUE);

		for (l = ivals; l; l = l->next) {
			ival = l->data;

			/* Start of the interval. */
			unit_ival = units_interval_new (ival, 100, TRUE);
			unit_ival->units_full = 100;
			g_ptr_array_add (array, unit_ival);

			/* End of the interval. */
			unit_ival = units_interval_new (ival, 100, FALSE);
			unit_ival->units_full = 100;
			g_ptr_array_add (array, unit_ival);
		}
	}

#ifdef WITH_SIMPLE_PRIORITY_SCHEDULING
	/* Requantize the time intervals. */
	array_split = g_ptr_array_new ();
	len = array->len;
	poc = -1;
	units = 0;
	/* += 2 because start and end are strictly joined. */
	for (i = 0; i < len; i+= 2) {
		unit_ival_start = g_ptr_array_index (array, i);

		i_start = unit_ival_start->start;
		i_end = unit_ival_start->end;

		lastct = 1;
		i_end_post = i_end;
		for (i_start_post = i_start; i_start_post < i_end && lastct > 0;) {
			lastct = 0;
			i_end_post = i_end;
			for (e = 0 ; e < len ; e+= 2) {
				unit_ival_start_cmp = g_ptr_array_index (array, e);

				i_start_cmp = unit_ival_start_cmp->start;
				i_end_cmp = unit_ival_start_cmp->end;

				/* If not overlapped intervals. */
				if (i_start_post >= i_end_cmp || i_end_post <= i_start_cmp) {
					continue;
				}
				if (i_start_post < i_start_cmp && i_start_cmp < i_end_post) {
					i_end_post = i_start_cmp;
					lastct++;
					continue;
				}
				if (i_start_post < i_end_cmp && i_end_cmp < i_end_post) {
					i_end_post = i_end_cmp;
					lastct++;
					continue;
				}
			}
			ival_split = mrp_interval_new (i_start_post, i_end_post);
			split_unit_ival = units_interval_new (ival_split, unit_ival_start->units, TRUE);
			split_unit_ival->units_full = unit_ival_start->units_full;
			g_ptr_array_add (array_split, split_unit_ival);
			split_unit_ival = units_interval_new (ival_split, unit_ival_start->units, FALSE);
			split_unit_ival->units_full = unit_ival_start->units_full;
			g_ptr_array_add (array_split, split_unit_ival);
			i_start_post = i_end_post;
		}
	}
	for (i = 0; i < array->len; i++) {
		g_free (array->pdata[i]);
	}

	g_ptr_array_free (array, TRUE);
	array = array_split;
#endif /* ifdef WITH_SIMPLE_PRIORITY_SCHEDULING */
	/* Clean and reassign the split_array ptr to the array */
	g_ptr_array_sort (array, units_interval_sort_func);

	len = array->len;


	poc = -1;
	units = 0;
	units_full = 0;
	res_n = 0;
	for (i = 0; i < len; i++) {
		unit_ival = g_ptr_array_index (array, i);

		/* Get the next point of change. */
		t = UNIT_IVAL_GET_TIME (unit_ival);

		if (t != poc) {
			/* Got a new point of change, the previous point is
			 * determined by now.
			 */
			if (poc != -1) {
				new_unit_ival = g_new (MrpUnitsInterval, 1);
				new_unit_ival->units = units;
				new_unit_ival->units_full = units_full;
				new_unit_ival->start = poc;
				new_unit_ival->end = t;
				new_unit_ival->res_n = res_n;
				res_n = 0;
				unit_ivals = g_list_prepend (unit_ivals, new_unit_ival);
			}

			poc = t;
		}

		if (unit_ival->is_start) {
			units += unit_ival->units;
			units_full += unit_ival->units_full;
			if (assignments) {
				res_n++;
			}
		} else {
			units -= unit_ival->units;
			units_full -= unit_ival->units_full;
		}
	}

	for (i = 0; i < array->len; i++) {
		g_free (array->pdata[i]);
	}

	g_ptr_array_free (array, TRUE);

	return g_list_reverse (unit_ivals);
}

static void
task_manager_calculate_milestone_work_start (MrpTaskManager *manager,
					     MrpTask        *task,
					     mrptime         start)

{
	mrptime             t;
	mrptime             t1, t2;
	mrptime             work_start;
	GList              *unit_ivals, *l;
	MrpUnitsInterval   *unit_ival;
	MrpTaskType         type;

	type = mrp_task_get_task_type (task);
	g_return_if_fail (type == MRP_TASK_TYPE_MILESTONE);

	work_start = -1;

	t = mrp_time_align_day (start);

	while (1) {
		unit_ivals = task_manager_get_task_units_intervals (manager, task, t);

		/* If we don't get anywhere in 100 days, then the calendar must
		 * be broken, so we abort the scheduling of this task. It's not
		 * the best solution but fixes the issue for now.
		 */
		if (t - start > (60*60*24*100)) {
			break;
		}

		if (!unit_ivals) {
			t += 60*60*24;
			continue;
		}

		for (l = unit_ivals; l; l = l->next) {
			unit_ival = l->data;

			t1 = t + unit_ival->start;
			t2 = t + unit_ival->end;

			/* Skip any intervals before the task starts. */
			if (t2 < start) {
				continue;
			}

			/* Don't add time before the start of the task. */
			t1 = MAX (t1, start);

			work_start = t1;
			break;
		}

		if (work_start != -1) {
			break;
		}

		t += 60*60*24;
	}

	if (work_start == -1) {
		work_start = start;
	}

	imrp_task_set_work_start (task, work_start);

	g_list_foreach (unit_ivals, (GFunc) g_free, NULL);
	g_list_free (unit_ivals);
}

/* Calculate the finish time from the work needed for the task, and the effort
 * that the allocated resources add to the task. Uses the project calendar if no
 * resources are allocated. This function also sets the work_start property of
 * the task, which is the first time that actually has work scheduled, this can
 * differ from the start if start is inside a non-work period.
 */
static mrptime
task_manager_calculate_task_finish (MrpTaskManager *manager,
				    MrpTask        *task,
				    mrptime         start,
				    gint           *duration)
{
	MrpTaskManagerPriv *priv;
	mrptime             finish;
	mrptime             t;
	mrptime             t1, t2;
	mrptime             work_start;
	gint                work;
	gint                effort;
	gint                delta;
	GList              *unit_ivals, *unit_ivals_tot = NULL, *l = NULL;
	MrpUnitsInterval   *unit_ival;
	MrpTaskType         type;
	MrpTaskSched        sched;

	priv = manager->priv;

	if (task == priv->root) {
		g_warning ("Tried to get duration of root task.");
		return 0;
	}

	/* Milestone tasks can be special cased, no duration. */
	type = mrp_task_get_task_type (task);
	if (type == MRP_TASK_TYPE_MILESTONE) {
		*duration = 0;
		task_manager_calculate_milestone_work_start (manager, task, start);
		return start;
	}

	work = mrp_task_get_work (task);
	sched = mrp_task_get_sched (task);

	if (sched == MRP_TASK_SCHED_FIXED_WORK) {
		*duration = 0;
	} else {
		*duration = mrp_task_get_duration (task);
	}

	effort = 0;

	finish = start;
	work_start = -1;

	t = mrp_time_align_day (start);

	while (1) {
		unit_ivals = task_manager_get_task_units_intervals (manager, task, t);

		/* If we don't get anywhere in 100 days, then the calendar must
		 * be broken, so we abort the scheduling of this task. It's not
		 * the best solution but fixes the issue for now.
		 */
		if (effort == 0 && t - start > (60*60*24*100)) {
			break;
		}

		if (!unit_ivals) {
			/* Holidays for all. */
			t += 60*60*24;
			continue;
		}

		for (l = unit_ivals; l; l = l->next) {
			unit_ival = l->data;

			t1 = t + unit_ival->start;
			t2 = t + unit_ival->end;
			/* Skip any intervals before the task starts. */
			if (t2 < start) {
				continue;
			}

			/* Don't add time before the start of the task. */
			t1 = MAX (t1, start);

			if (t1 == t2) {
				continue;
			}

			if (work_start == -1) {
				work_start = t1;
			}

			/* Effort added by this interval. */
			if (sched == MRP_TASK_SCHED_FIXED_WORK) {
				delta = floor (0.5 + (double) unit_ival->units * (t2 - t1) / 100.0);

				if (unit_ival->units_full > 0) {
					*duration += (t2 - t1);
				}

				if (effort + delta >= work) {
					/* Subtract the spill to duration. */
					if (unit_ival->units) {
						finish = t1 + floor (0.5 + (work - effort) / unit_ival->units * 100.0);
						*duration -= floor (0.5 + (effort + delta - work) / unit_ival->units * 100.0);
					} else {
						finish = t1 + floor (work - effort);
						*duration -= floor (0.5 + (effort + delta - work));
					}

					unit_ival->start = t1;
					unit_ival->end = finish;
					unit_ivals_tot = g_list_prepend (unit_ivals_tot, unit_ival);
					goto done;
				}
				unit_ival->start = t1;
				unit_ival->end = t2;
				unit_ivals_tot = g_list_prepend (unit_ivals_tot, unit_ival);
			}
			else if (sched == MRP_TASK_SCHED_FIXED_DURATION) {
				delta = t2 - t1;

				if (unit_ival->units_full == 0) {
					delta = 0;
				} else if (effort + delta >= *duration) {
					/* Done, make sure we don't spill. */
					finish = t1 + *duration - effort;
					goto done;
				}
			} else {
				delta = 0;
				g_assert_not_reached ();
			}

			effort += delta;
		}
		t += 60*60*24;
	}

 done:

	if (work_start == -1) {
		work_start = start;
	}
	imrp_task_set_work_start (task, work_start);

	/* clean the tail of the list before exit of function. */
	if (l) {
		for (l = l->next ; l; l = l->next) {
			unit_ival = l->data;
		g_free (unit_ival);
		}
	}
	g_list_free (unit_ivals);

	unit_ivals_tot = g_list_reverse (unit_ivals_tot);
	mrp_task_set_unit_ivals (task, unit_ivals_tot);

	return finish;
}

/* Calculate the start time from the work needed for the task, and the effort
 * that the allocated resources add to the task. Uses the project calendar if no
 * resources are allocated. This function also sets the work_start property of
 * the task, which is the first time that actually has work scheduled, this can
 * differ from the start if start is inside a non-work period.
 */
static mrptime
task_manager_calculate_task_start_from_finish (MrpTaskManager *manager,
				    MrpTask        *task,
				    mrptime         finish,
				    gint           *duration)
{
	MrpTaskManagerPriv *priv;
	mrptime             start;
	mrptime             t;
	mrptime             t1, t2;
	mrptime             work_start;
	mrptime             project_start;
	gint                work;
	gint                effort;
	gint                delta;
	GList              *unit_ivals, *l;
	MrpUnitsInterval   *unit_ival;
	MrpTaskType         type;
	MrpTaskSched        sched;

	priv = manager->priv;

	if (task == priv->root) {
		g_warning ("Tried to get duration of root task.");
		return 0;
	}

	effort = 0;
	start = finish;
	work_start = -1;
	t = mrp_time_align_day (start);
	project_start = mrp_project_get_project_start (priv->project);


	/* Milestone tasks can be special cased, no duration. */
	type = mrp_task_get_task_type (task);
	if (type == MRP_TASK_TYPE_MILESTONE) {
		*duration = 0;
		task_manager_calculate_milestone_work_start (manager, task, start);
		return start;
	}

	work = mrp_task_get_work (task);
	sched = mrp_task_get_sched (task);

	if (sched == MRP_TASK_SCHED_FIXED_WORK) {
		*duration = 0;
	} else {
		*duration = mrp_task_get_duration (task);
	}

	while (1) {
		unit_ivals = g_list_reverse (task_manager_get_task_units_intervals (manager, task, t));

		/* If we don't get anywhere in 100 days, then the calendar must
		 * be broken, so we abort the scheduling of this task. It's not
		 * the best solution but fixes the issue for now.
		 */
		if (effort == 0 && finish - t > (60*60*24*100)) {
			break;
		}

		if (!unit_ivals) {
			t -= 60*60*24;
			continue;
		}

		for (l = unit_ivals; l; l = l->next) {
			unit_ival = l->data;

			t1 = t + unit_ival->start;
			t2 = t + unit_ival->end;

			/* Skip any intervals after the task ends. */
			if (t1 > finish) {
				continue;
			}

			/* Don't add time after the finish time of the task. */
			t2 = MIN (t2, finish);

			if (t1 == t2) {
				continue;
			}

			if (work_start == -1) {
				work_start = t1;
			}

			/* Effort added by this interval. */
			if (sched == MRP_TASK_SCHED_FIXED_WORK) {
				delta = floor (0.5 + (double) unit_ival->units * (t2 - t1) / 100.0);

				*duration += (t2 - t1);

				if (effort + delta >= work) {
					/* Subtract the spill to duration. */
					if (unit_ival->units) {
						start = t2 - floor (0.5 + (work - effort) / unit_ival->units * 100.0);
						*duration -= floor (0.5 + (effort + delta - work) / unit_ival->units * 100.0);
					} else {
						start = t2 - floor (0.5 + (work - effort));
						*duration -= floor (0.5 + (effort + delta - work));
					}
					goto done;
				}
			}
			else if (sched == MRP_TASK_SCHED_FIXED_DURATION) {
				delta = t2 - t1;

				if (unit_ival->units_full == 0) {
					delta = 0;
				} else if (effort + delta >= *duration) {
					/* Done, make sure we don't spill. */
					start = t2 - (*duration - effort);
					goto done;
				}
			} else {
				/* Schedule is either fixed work of fixed duration - we should never get here */
				delta = 0;
				g_assert_not_reached ();
			}

			effort += delta;
		}

		t -= 60*60*24;
	}

 done:

	start = MAX (start, project_start);
	if (work_start == -1) {
		work_start = start;
	}
	imrp_task_set_work_start (task, work_start);

	g_list_foreach (unit_ivals, (GFunc) g_free, NULL);
	g_list_free (unit_ivals);

	return start;
}

static void
task_manager_do_forward_pass_helper (MrpTaskManager *manager,
				     MrpTask        *task)
{
	mrptime             sub_start, sub_work_start, sub_finish;
	mrptime             old_start, old_finish;
	mrptime             new_start, new_finish;
	gint                duration;
	gint                old_duration;
	gint                work;
	mrptime             t1, t2;
	MrpTaskSched        sched;

	old_start = mrp_task_get_start (task);
	old_finish = mrp_task_get_finish (task);
	old_duration = old_finish - old_start;
	duration = 0;

	if (mrp_task_get_n_children (task) > 0) {
		MrpTask *child;

		sub_start = -1;
		sub_work_start = -1;
		sub_finish = -1;
		work = 0;

		child = mrp_task_get_first_child (task);
		while (child) {
			t1 = mrp_task_get_start (child);
			if (sub_start == -1) {
				sub_start = t1;
			} else {
				sub_start = MIN (sub_start, t1);
			}

			t2 = mrp_task_get_finish (child);
			if (sub_finish == -1) {
				sub_finish = t2;
			} else {
				sub_finish = MAX (sub_finish, t2);
			}

			t2 = mrp_task_get_work_start (child);
			if (sub_work_start == -1) {
				sub_work_start = t2;
			} else {
				sub_work_start = MIN (sub_work_start, t2);
			}

			work += mrp_task_get_work (child);
			child = mrp_task_get_next_sibling (child);
		}

		imrp_task_set_start (task, sub_start);
		imrp_task_set_work_start (task, sub_work_start);
		imrp_task_set_finish (task, sub_finish);

		duration = mrp_task_manager_calculate_summary_duration (manager,
							     task,
							     sub_start,
							     sub_finish);
		imrp_task_set_work (task, work);
		imrp_task_set_duration (task, duration);
	} else {
		/* Non-summary task. */
		t1 = task_manager_calculate_task_start (manager, task, &duration);
		t2 = task_manager_calculate_task_finish (manager, task, t1, &duration);

		imrp_task_set_start (task, t1);
		imrp_task_set_finish (task, t2);

		sched = mrp_task_get_sched (task);
		if (sched == MRP_TASK_SCHED_FIXED_WORK) {
			imrp_task_set_duration (task, duration);
		} else {
			duration = mrp_task_get_duration (task);
			work = mrp_task_get_work (task);

			/* Update resource units for fixed duration. */
			if (duration > 0) {
				GList         *assignments, *a;
				MrpAssignment *assignment;
				gint           n, units;

				assignments = mrp_task_get_assignments (task);

				n = g_list_length (assignments);
				units = floor (0.5 + 100.0 * (gdouble) work / duration / n);

				for (a = assignments; a; a = a->next) {
					assignment = a->data;

					g_signal_handlers_block_by_func (assignment,
									 task_manager_assignment_units_notify_cb,
									 manager);

					g_object_set (assignment, "units", units, NULL);

					g_signal_handlers_unblock_by_func (assignment,
									   task_manager_assignment_units_notify_cb,
									   manager);
				}
			}
		}
	}

	new_start = mrp_task_get_start (task);
	if (old_start != new_start) {
		g_object_notify (G_OBJECT (task), "start");
	}

	new_finish = mrp_task_get_finish (task);
	if (old_finish != new_finish) {
		g_object_notify (G_OBJECT (task), "finish");
	}

	if (old_duration != (new_finish - new_start)) {
		g_object_notify (G_OBJECT (task), "duration");
	}
}

static void
task_manager_do_forward_pass (MrpTaskManager *manager,
			      MrpTask        *start_task)
{
	MrpTaskManagerPriv *priv;
	GList              *l;

	priv = manager->priv;

	/* Do forward pass, start at the task and do all tasks that come after
	 * it in the dependency list. Note: we could try to skip tasks that are
	 * not dependent, but I don't think that's really worth it.
	*/

	if (start_task) {
		l = g_list_find (priv->dependency_list, start_task);
	} else {
		l = priv->dependency_list;
	}

	while (l) {
		task_manager_do_forward_pass_helper (manager, l->data);
		l = l->next;
	}

	/* FIXME: Might need to rework this if we make the forward/backward
	 * passes only recalculate tasks that depend on the changed task.
	 */
	task_manager_do_forward_pass_helper (manager, priv->root);
}

static void
task_manager_do_backward_pass (MrpTaskManager *manager)
{
	MrpTaskManagerPriv *priv;
	GList              *tasks, *l;
	GList              *successors, *s;
	mrptime             project_finish;
	mrptime             t1, t2;
	gint                duration;
	gboolean            critical;
	gboolean            was_critical;

	priv = manager->priv;

	project_finish = mrp_task_get_finish (priv->root);

	tasks = g_list_reverse (g_list_copy (priv->dependency_list));

	for (l = tasks; l; l = l->next) {
		MrpTask *task, *parent;

		task =  l->data;
		parent = mrp_task_get_parent (task);

		if (!parent || parent == priv->root) {
			t1 = project_finish;
		} else {
			t1 = MIN (project_finish, mrp_task_get_latest_finish (parent));
		}

		successors = imrp_task_peek_successors (task);
		for (s = successors; s; s = s->next) {
			MrpRelation *relation;
			MrpTask     *successor, *child;

			relation = s->data;
			successor = mrp_relation_get_successor (relation);

			child = mrp_task_get_first_child (successor);
			if (child) {
				/* If successor has children go through them
				 * instead of the successor itself.
				 */
				for (; child; child = mrp_task_get_next_sibling (child)) {
					successor = child;

					t2 = mrp_task_get_latest_start (successor) -
						mrp_relation_get_lag (relation);

					t1 = MIN (t1, t2);
				}
			} else {
				/* No children, check the real successor. */
				t2 = mrp_task_get_latest_start (successor) -
					mrp_relation_get_lag (relation);

				t1 = MIN (t1, t2);
			}
		}

		imrp_task_set_latest_finish (task, t1);

		/* Use the calendar duration to get the actual latest start, or
		 * calendars will make this break.
		 */
		duration = mrp_task_get_finish (task) - mrp_task_get_start (task);
		t1 -= duration;
		imrp_task_set_latest_start (task, t1);

		t2 = mrp_task_get_start (task);

		was_critical = mrp_task_get_critical (task);
		critical = (t1 == t2);

		/* FIXME: Bug in critical path for A -> B when B is SNET.
		 *
		 * The reason is that latest start for B becomes 00:00 instead
		 * of 17:00 the day before. So the slack becomes 7 hours
		 * (24-17).
		 */
#if 0
		g_print ("Task %s:\n", mrp_task_get_name (task));
		g_print ("  latest start   : "); mrp_time_debug_print (mrp_task_get_latest_start (task));
		g_print ("  latest finish  : "); mrp_time_debug_print (mrp_task_get_latest_finish (task));

#endif

		if (was_critical != critical) {
			g_object_set (task, "critical", critical, NULL);
		}
	}

	g_list_free (tasks);
}

void
mrp_task_manager_set_block_scheduling (MrpTaskManager *manager, gboolean block)
{
	MrpTaskManagerPriv *priv;

	g_return_if_fail (MRP_IS_TASK_MANAGER (manager));

	priv = manager->priv;

	if (priv->block_scheduling == block) {
		return;
	}

	priv->block_scheduling = block;

	if (!block) {
		mrp_task_manager_recalc (manager, TRUE);
	}
}

gboolean
mrp_task_manager_get_block_scheduling (MrpTaskManager *manager)
{
	g_return_val_if_fail (MRP_IS_TASK_MANAGER (manager), FALSE);

	return manager->priv->block_scheduling;
}

void
mrp_task_manager_rebuild (MrpTaskManager *manager)
{
	MrpTaskManagerPriv *priv;

	g_return_if_fail (MRP_IS_TASK_MANAGER (manager));
	g_return_if_fail (manager->priv->root != NULL);

	priv = manager->priv;

	if (priv->block_scheduling) {
		return;
	}

	task_manager_build_dependency_graph (manager);

	priv->needs_rebuild = FALSE;
	priv->needs_recalc = TRUE;
}

void
mrp_task_manager_recalc (MrpTaskManager *manager,
			 gboolean        force)
{
	MrpTaskManagerPriv *priv;
	MrpProject         *project;

	g_return_if_fail (MRP_IS_TASK_MANAGER (manager));
	g_return_if_fail (manager->priv->root != NULL);

	priv = manager->priv;

	if (priv->block_scheduling) {
		return;
	}

	if (priv->in_recalc) {
		return;
	}

	priv->needs_recalc |= force;

	if (!priv->needs_recalc && !priv->needs_rebuild) {
		return;
	}

	/* If we don't have any children yet, or if the root is not inserted
	 * properly into the project yet, just postpone the recalc.
	 */
	if (mrp_task_get_n_children (priv->root) == 0) {
		return;
	}

	project = mrp_object_get_project (MRP_OBJECT (priv->root));
	if (!project) {
		return;
	}

	priv->in_recalc = TRUE;

	if (priv->needs_rebuild) {
		mrp_task_manager_rebuild (manager);
	}

	task_manager_do_forward_pass (manager, NULL);
	task_manager_do_backward_pass (manager);

	priv->needs_recalc = FALSE;
	priv->in_recalc = FALSE;
}

static void
task_manager_task_duration_notify_cb (MrpTask        *task,
				      GParamSpec     *spec,
				      MrpTaskManager *manager)
{
	mrp_task_manager_recalc (manager, TRUE);
}

#ifdef WITH_SIMPLE_PRIORITY_SCHEDULING
static void
task_manager_task_priority_notify_cb (MrpTask        *task,
									  GParamSpec     *spec,
									  MrpTaskManager *manager)
{
	mrp_task_manager_recalc (manager, TRUE);
}
#endif
static void
task_manager_task_constraint_notify_cb (MrpTask        *task,
					GParamSpec     *spec,
					MrpTaskManager *manager)
{
	mrp_task_manager_recalc (manager, TRUE);
}

static void
task_manager_project_start_notify_cb (MrpProject     *project,
				      GParamSpec     *spec,
				      MrpTaskManager *manager)
{
	mrp_task_manager_recalc (manager, TRUE);
}

static void
task_manager_task_relation_notify_cb (MrpRelation    *relation,
				      GParamSpec     *spec,
				      MrpTaskManager *manager)
{
	mrp_task_manager_recalc (manager, TRUE);
}

static void
task_manager_assignment_units_notify_cb (MrpAssignment  *assignment,
					 GParamSpec     *spec,
					 MrpTaskManager *manager)
{
	mrp_task_invalidate_cost (mrp_assignment_get_task (assignment));
	mrp_task_manager_recalc (manager, TRUE);
}

static void
task_manager_task_relation_added_cb (MrpTask        *task,
				     MrpRelation    *relation,
				     MrpTaskManager *manager)
{
	/* We get a signal on both the predecessor and the successor, it's
	 * enough with one rebuild.
	 */
	if (task == mrp_relation_get_predecessor (relation)) {
		return;
	}

	g_signal_connect_object (relation,
				 "notify",
				 G_CALLBACK (task_manager_task_relation_notify_cb),
				 manager, 0);

	manager->priv->needs_rebuild = TRUE;
	mrp_task_manager_recalc (manager, FALSE);
}

static void
task_manager_task_relation_removed_cb (MrpTask        *task,
				       MrpRelation    *relation,
				       MrpTaskManager *manager)
{
	/* We get a signal on both the predecessor and the successor, it's
	 * enough with one rebuild.
	 */
	if (task == mrp_relation_get_predecessor (relation)) {
		return;
	}

	g_signal_handlers_disconnect_by_func (relation,
					      task_manager_task_relation_notify_cb,
					      manager);

	manager->priv->needs_rebuild = TRUE;
	mrp_task_manager_recalc (manager, FALSE);
}

static void
task_manager_task_assignment_added_cb (MrpTask        *task,
				       MrpAssignment  *assignment,
				       MrpTaskManager *manager)
{
	g_signal_connect_object (assignment, "notify::units",
				 G_CALLBACK (task_manager_assignment_units_notify_cb),
				 manager, 0);

	mrp_task_invalidate_cost (task);
	manager->priv->needs_rebuild = TRUE;
	mrp_task_manager_recalc (manager, FALSE);
}

static void
task_manager_task_assignment_removed_cb (MrpTask        *task,
					 MrpAssignment  *assignment,
					 MrpTaskManager *manager)
{
	g_signal_handlers_disconnect_by_func (assignment,
					      task_manager_assignment_units_notify_cb,
					      manager);

	mrp_task_invalidate_cost (task);
	manager->priv->needs_rebuild = TRUE;
	mrp_task_manager_recalc (manager, FALSE);
}

static gboolean
check_predecessor_traverse (MrpTaskManager *manager,
			    MrpTask        *task,
			    MrpTask        *end,
			    gint            length)
{
	MrpTaskGraphNode *node;
	GList            *l;

	if (length > 1 && task == end) {
		return FALSE;
	}

	/* Avoid endless loop. */
	if (imrp_task_get_visited (task)) {
		return TRUE;
	}

	imrp_task_set_visited (task, TRUE);

	node = imrp_task_get_graph_node (task);
	for (l = node->next; l; l = l->next) {
		if (!check_predecessor_traverse (manager, l->data, end, length + 1)) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
check_move_traverse_recursive (MrpTaskManager *manager,
			       MrpTask *task)
{
	MrpTask          *child;
	gboolean          retval = TRUE;

	child = mrp_task_get_first_child (task);
	while (retval && child) {
		retval = check_predecessor_traverse (manager, child, child, 1);

		if (retval && mrp_task_get_n_children (child) > 0) {
			retval = check_move_traverse_recursive (manager, child);
		}

		child = mrp_task_get_next_sibling (child);
	}

	return retval;
}

static gboolean
check_move_traverse (MrpTaskManager *manager,
		     MrpTask        *task)
{
	return check_predecessor_traverse (manager, task, task, 1) &&
		check_move_traverse_recursive (manager, task);
}

gboolean
mrp_task_manager_check_predecessor (MrpTaskManager  *manager,
				    MrpTask         *task,
				    MrpTask         *predecessor,
				    GError         **error)
{
	gboolean retval;

	g_return_val_if_fail (MRP_IS_TASK_MANAGER (manager), FALSE);
	g_return_val_if_fail (MRP_IS_TASK (task), FALSE);
	g_return_val_if_fail (MRP_IS_TASK (predecessor), FALSE);

	if (manager->priv->needs_rebuild) {
		mrp_task_manager_rebuild (manager);
	}

	/* Add the predecessor to check. */
	add_predecessor_to_dependency_graph (manager, task, predecessor);

	if (0) {
		g_print ("--->\n");
		dump_all_task_nodes (manager);
		g_print ("<---\n");
	}

	mrp_task_manager_traverse (manager,
				   manager->priv->root,
				   task_manager_unset_visited_func,
				   NULL);

	retval = check_predecessor_traverse (manager, predecessor, predecessor, 1);

	/* Remove the predecessor again. */
	remove_predecessor_from_dependency_graph (manager, task, predecessor);

	if (!retval) {
		g_set_error (error,
			     MRP_ERROR,
			     MRP_ERROR_TASK_RELATION_FAILED,
			     _("Cannot add a predecessor, because it would result in a loop."));
		return FALSE;
	}

	return TRUE;
}

gboolean
mrp_task_manager_check_move (MrpTaskManager  *manager,
			     MrpTask         *task,
			     MrpTask         *parent,
			     GError         **error)
{
	gboolean retval;

	g_return_val_if_fail (MRP_IS_TASK_MANAGER (manager), FALSE);
	g_return_val_if_fail (MRP_IS_TASK (task), FALSE);
	g_return_val_if_fail (MRP_IS_TASK (parent), FALSE);

	/* Remove the task from the old parent and add it to its new parent. */
	remove_task_from_dependency_graph (manager, task, mrp_task_get_parent (task));
	add_task_to_dependency_graph (manager, task, parent);

	if (0) {
		g_print ("--->\n");
		dump_all_task_nodes (manager);
		g_print ("<---\n");
	}

	mrp_task_manager_traverse (manager,
				   manager->priv->root,
				   task_manager_unset_visited_func,
				   NULL);

	retval = check_move_traverse (manager, task);

	/* Put the task back again. */
	remove_task_from_dependency_graph (manager, task, parent);
	add_task_to_dependency_graph (manager, task, mrp_task_get_parent (task));

	if (!retval) {
		g_set_error (error,
			     MRP_ERROR,
			     MRP_ERROR_TASK_MOVE_FAILED,
			     _("Cannot move the task, because it would result in a loop."));
		return FALSE;
	}

	return retval;
}

static gint
task_manager_get_work_for_calendar (MrpTaskManager *manager,
				    MrpCalendar    *calendar,
				    mrptime         start,
				    mrptime         finish)
{
	mrptime             t;
	mrptime             t1, t2;
	gint                work;
	MrpDay             *day;
	GList              *ivals, *l;
	MrpInterval        *ival;

	work = 0;

	/* Loop through the intervals of the calendar and add up the work, until
	 * the finish time is hit.
	 */
	t = mrp_time_align_day (start);

	while (t < finish) {
		day = mrp_calendar_get_day (calendar, t, TRUE);
		ivals = mrp_calendar_day_get_intervals (calendar, day, TRUE);

		for (l = ivals; l; l = l->next) {
			ival = l->data;

			mrp_interval_get_absolute (ival, t, &t1, &t2);

			/* Skip intervals that are before the task. */
			if (t2 < start) {
				continue;
			}

			/* Stop if the interval starts after the task. */
			if (t1 >= finish) {
				break;
			}

			/* Don't add time outside the task. */
			t1 = MAX (t1, start);
			t2 = MIN (t2, finish);

			work += t2 - t1;
		}

		t += 24*60*60;
	}

	return work;
}

static gint
task_manager_get_work_for_task_with_assignments (MrpTaskManager *manager,
									   MrpTask        *task,
									   mrptime         start,
									   mrptime         finish)
{
	mrptime             t;
	mrptime             t1, t2;
	gint                work, delta;
	GList              *ivals, *l;
	MrpUnitsInterval   *ival;

	work = 0;

	/* Loop through the intervals of the calendar and add up the work, until
	 * the finish time is hit.
	 */
	t = mrp_time_align_day (start);


	while (t < finish) {
		ivals = task_manager_get_task_units_intervals (manager, task, t);

		/* If we don't get anywhere in 100 days, then the calendar must
		 * be broken, so we abort the scheduling of this task. It's not
		 * the best solution but fixes the issue for now.
		 */
		if (work == 0 && t - start > (60*60*24*100)) {
			break;
		}

		if (!ivals) {
			/* Holidays for all. */
			t += 60*60*24;
			continue;
		}

		for (l = ivals; l; l = l->next) {
			ival = l->data;

			t1 = t + ival->start;
			t2 = t + ival->end;
			/* Skip any intervals before the task starts. */
			if (t2 < start) {
				continue;
			}

			/* Don't add time before the start of the task. */
			t1 = MAX (t1, start);
			if (t1 == t2) {
				continue;
			}

			/* Resize the too long interval */
			t2 = MIN (t2, finish);
			if (t1 >= t2) {
				break;
			}

			delta = floor (0.5 + (double) ival->units * (t2 - t1) / 100.0);

			work += delta;
		}

		g_list_free (ivals);


		t += 24*60*60;
	}

	return (work);
}

/* Calculate the work needed to achieve the specified start and finish, with the
 * allocated resources' calendars in consideration.
 */
gint
mrp_task_manager_calculate_task_work (MrpTaskManager *manager,
				      MrpTask        *task,
				      mrptime         start,
				      mrptime         finish)
{
	MrpTaskManagerPriv *priv;
	gint                work = 0;
	GList              *assignments;
	MrpCalendar        *calendar;

	priv = manager->priv;

	if (task == priv->root) {
		return 0;
	}

	if (start == -1) {
		/* FIXME: why did we use task_manager_calculate_task_start
		 * (manager, task) here?? Shouldn't be needed...
		 */
		start = mrp_task_get_start (task);
	}

	if (finish <= start) {
		return 0;
	}

	/* Loop through the intervals of the assigned resources' calendars (or
	 * the project calendar if no resources are assigned), and add up the
	 * work.
	 */

	assignments = mrp_task_get_assignments (task);
	if (assignments) {
		work = task_manager_get_work_for_task_with_assignments (manager,
													  task,
													  start,
													  finish);
	}
	else {
		calendar = mrp_project_get_calendar (priv->project);

		work = task_manager_get_work_for_calendar (manager,
							   calendar,
							   start,
							   finish);
	}

	return work;
}

/* Calculate summary duration using the project calendar,
   ignoring assignments' calendars
 */
gint
mrp_task_manager_calculate_summary_duration (MrpTaskManager *manager,
				      MrpTask        *task,
				      mrptime         start,
				      mrptime         finish)
{
	MrpTaskManagerPriv *priv;
	mrptime             t;
	mrptime             t1, t2;
	gint                duration = 0;
	MrpCalendar        *calendar;
	MrpDay             *day;
	GList              *ivals, *l;
	MrpInterval        *ival;

	priv = manager->priv;

	if (task == priv->root) {
		return 0;
	}

	if (start == -1) {
		start = mrp_task_get_start (task);
	}

	if (finish <= start) {
		return 0;
	}

	calendar = mrp_project_get_calendar (priv->project);

	duration = 0;

	/* Loop through the intervals of the calendar and add up the working time, until
	 * the finish time is hit.
	 */
	t = mrp_time_align_day (start);

	while (t < finish) {
		day = mrp_calendar_get_day (calendar, t, TRUE);
		ivals = mrp_calendar_day_get_intervals (calendar, day, TRUE);

		for (l = ivals; l; l = l->next) {
			ival = l->data;

			mrp_interval_get_absolute (ival, t, &t1, &t2);

			/* Skip intervals that are before the task. */
			if (t2 < start) {
				continue;
			}

			/* Stop if the interval starts after the task. */
			if (t1 >= finish) {
				break;
			}

			/* Don't add time outside the task. */
			t1 = MAX (t1, start);
			t2 = MIN (t2, finish);

			duration += t2 - t1;
		}

		t += 24*60*60;
	}

	return duration;
}

