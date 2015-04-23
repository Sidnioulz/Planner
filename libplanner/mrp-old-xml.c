/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004-2005 Imendio AB
 * Copyright (C) 2002-2003 CodeFactory AB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
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
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include "mrp-private.h"
#include "mrp-error.h"
#include <glib/gi18n.h>
#include "mrp-task.h"
#include "mrp-resource.h"
#include "mrp-group.h"
#include "mrp-relation.h"
#include "mrp-old-xml.h"

typedef struct {
        xmlDocPtr       doc;

	gint            version;

	MrpProject     *project;

	MrpTask        *root_task;
	GList          *resources;
	GList          *groups;
	GList          *assignments;

	mrptime         project_start;

	gint            project_calendar_id;

	MrpGroup       *default_group;

	gint            last_id;

	GHashTable     *task_hash;
	GHashTable     *resource_hash;
	GHashTable     *group_hash;
	GHashTable     *day_hash;
	GHashTable     *calendar_hash;
	GList          *delayed_relations;
} MrpParser;

typedef struct {
	gint            predecessor_id;
	gint            successor_id;
	gint            lag;
	MrpRelationType type;
} DelayedRelation;

typedef struct {
	xmlNodePtr node;
	gint       id;
} NodeEntry;

static gchar           *old_xml_get_string            (xmlNodePtr   node,
						       const char  *name);
static gint             old_xml_get_int               (xmlNodePtr   node,
						       const char  *name);
static gint             old_xml_get_int_with_default  (xmlNodePtr   node,
						       const char  *name,
						       gint         def);
static mrptime          old_xml_get_date              (xmlNodePtr   node,
						       const char  *name);
static gfloat           old_xml_get_float             (xmlNodePtr   node,
						       const char  *name);
static MrpTaskType      old_xml_get_task_type         (xmlNodePtr   node,
						       const char  *name);
static MrpTaskSched     old_xml_get_task_sched        (xmlNodePtr   node,
						       const char  *name);
static xmlNodePtr       old_xml_search_child          (xmlNodePtr   node,
						       const gchar *name);
static MrpPropertyType
old_xml_property_type_from_string                     (const gchar *str);
static void
old_xml_set_property_from_node                        (MrpProject  *project,
                                                       MrpObject   *object,
                                                       xmlNodePtr   node);

static void
old_xml_read_predecessor (MrpParser  *parser,
			  gint        task_id,
			  xmlNodePtr  tree)
{
	gint             predecessor_id;
	DelayedRelation *relation;
	gchar           *str;
	MrpRelationType  type;

	if (strcmp (tree->name, "predecessor")){
		/*g_warning ("Got %s, expected 'predecessor'.", tree->name);*/
		return;
	}

	predecessor_id = old_xml_get_int (tree, "predecessor-id");
	if (predecessor_id == 0) {
		g_warning ("Invalid predecessor read.");
		return;
	}

	str = old_xml_get_string (tree, "type");
	if (!strcmp (str, "FS")) {
		type = MRP_RELATION_FS;
	}
	else if (!strcmp (str, "FF")) {
		type = MRP_RELATION_FF;
	}
	else if (!strcmp (str, "SS")) {
		type = MRP_RELATION_SS;
	}
	else if (!strcmp (str, "SF")) {
		type = MRP_RELATION_SF;
	}
	else {
		g_warning ("Invalid dependency type.");
		g_free (str);
		return;
	}

	g_free (str);

	relation = g_new0 (DelayedRelation, 1);
	relation->successor_id = task_id;
	relation->predecessor_id = predecessor_id;
	relation->type = type;
	relation->lag = old_xml_get_int (tree, "lag");

	parser->delayed_relations = g_list_prepend (parser->delayed_relations,
						    relation);
}

static gboolean
old_xml_read_constraint (xmlNodePtr node, MrpConstraint *constraint)
{
	gchar *str;

	str = old_xml_get_string (node, "type");

	if (str == NULL) {
		g_warning ("Invalid constraint read.");
		return FALSE;
	}

	if (!strcmp (str, "must-start-on")) {
		constraint->type = MRP_CONSTRAINT_MSO;
	} else if (!strcmp (str, "start-no-earlier-than")) {
		constraint->type = MRP_CONSTRAINT_SNET;
	} else if (!strcmp (str, "finish-no-later-than")) {
		constraint->type = MRP_CONSTRAINT_FNLT;
	} else {
		g_warning ("Cant handle constraint '%s'", str);
		g_free (str);

		return FALSE;
	}

	constraint->time = old_xml_get_date (node, "time");

	g_free (str);

	return TRUE;
}

static void
old_xml_read_custom_properties (MrpParser  *parser,
				xmlNodePtr  node,
				MrpObject  *object)
{
	xmlNodePtr child;

	for (child = node->children; child; child = child->next) {
		if (strcmp (child->name, "property")) {
			continue;
		}

		old_xml_set_property_from_node (parser->project,
						object,
						child);
	}
}

static void
old_xml_read_task (MrpParser *parser, xmlNodePtr tree, MrpTask *parent)
{
	xmlNodePtr     tasks, predecessor;
	xmlNodePtr     child;
	gchar          *name;
	gint           id;
	mrptime        start = 0, end = 0;
	MrpTask       *task;
	MrpConstraint  constraint;
	guint          percent_complete = 0;
	gint          priority = 0;
	gchar         *note;
	gint           duration, work;
	gboolean       got_constraint = FALSE;
	MrpTaskType    type;
	MrpTaskSched   sched;

	if (strcmp (tree->name, "task")){
		/*g_warning ("Got %s, expected 'task'.", tree->name);*/
		return;
	}

	name = old_xml_get_string (tree, "name");
	note = old_xml_get_string (tree, "note");
	id = old_xml_get_int (tree, "id");
	percent_complete = old_xml_get_int (tree, "percent-complete");
	priority = old_xml_get_int (tree, "priority");
	type = old_xml_get_task_type (tree, "type");
	sched = old_xml_get_task_sched (tree, "scheduling");

	if (parser->version == 1) {
		start = old_xml_get_date (tree, "start");
		end = old_xml_get_date (tree, "end");
		duration = end - start;

		if (parser->project_start == -1) {
			parser->project_start = start;
		} else {
			parser->project_start = MIN (parser->project_start, start);
		}

		constraint.type = MRP_CONSTRAINT_MSO;
		constraint.time = start;
		got_constraint = TRUE;

		task = g_object_new (MRP_TYPE_TASK,
				     "project", parser->project,
				     "name", name,
				     "duration", duration,
				     "percent_complete", percent_complete,
				     "priority", priority,
				     "note", note,
				     NULL);
	} else {
		/* Use work if available, otherwise use duration. */

		work = old_xml_get_int_with_default (tree, "work", -1);
		duration = old_xml_get_int_with_default (tree, "duration", -1);

		if (work == -1 && duration == -1) {
			g_warning ("The file is not correct, no work and no duration.");
			work = 8*60*60;
			duration = 8*60*60;
		}

		if (work == -1) {
			work = duration;
		}
		if (duration == -1) {
			duration = work;
		}

		if (type == MRP_TASK_TYPE_MILESTONE) {
			work = 0;
			duration = 0;
		}

		task = g_object_new (MRP_TYPE_TASK,
				     "project", parser->project,
				     "name", name,
				     "sched", sched,
				     "type", type,
				     "work", work,
				     "duration", duration,
				     "percent_complete", percent_complete,
				     "priority", priority,
				     "note", note,
				     NULL);
	}

	g_free (name);
	g_free (note);

	/* Note: We should use mrp_project_insert_task here instead of the
	 * private imrp_task_insert_child, and not set the "project" property
	 * above. But then the project will emit signals to the views etc,
	 * maybe we should have freeze/thaw on the project or something.
	 */
	imrp_task_insert_child (parent, -1, task);

	/* Treat duration from old files as work. */
	if (parser->version == 1) {
		gint work;

		work = mrp_project_calculate_task_work (parser->project,
							task,
							start, end);

		g_object_set (task,
			      "work", work,
			      NULL);
	}

	g_hash_table_insert (parser->task_hash, GINT_TO_POINTER (id), task);

	for (child = tree->children; child; child = child->next) {
			if (!strcmp (child->name, "properties")) {
				old_xml_read_custom_properties (parser, child, MRP_OBJECT (task));
			}
		}

	for (tasks = tree->children; tasks; tasks = tasks->next) {
		if (!strcmp (tasks->name, "task")) {
			/* Silently correct old milestones with children to
			 * normal tasks.
			 */
			if (type == MRP_TASK_TYPE_MILESTONE) {
				type = MRP_TASK_TYPE_NORMAL;
				g_object_set (task, "type", type, NULL);
			}

			old_xml_read_task (parser, tasks, task);
		}
		else if (!strcmp (tasks->name, "predecessors")) {
			for (predecessor = tasks->children; predecessor; predecessor = predecessor->next) {
				old_xml_read_predecessor (parser, id, predecessor);
			}
		}
		else if (!strcmp (tasks->name, "constraint")) {
			got_constraint = old_xml_read_constraint (tasks, &constraint);
		}
	}

	if (got_constraint) {
		g_object_set (task,
			      "constraint", &constraint,
			      NULL);
	}
}

static void
old_xml_read_resource (MrpParser *parser, xmlNodePtr tree)
{
	xmlNodePtr   child;
	gint         id;
	gint         type;
	gchar       *name, *short_name, *email;
	gchar       *note;
	gint         gid;
	gint         units;
	gfloat       std_rate; /*, ovt_rate;*/
	gint         calendar_id;
	MrpResource *resource;
	MrpGroup    *group;
	MrpCalendar *calendar;

	if (strcmp (tree->name, "resource")){
		/*g_warning ("Got %s, expected 'resource'.", tree->name);*/
		return;
	}

	id          = old_xml_get_int (tree, "id");
	name        = old_xml_get_string (tree, "name");
	short_name  = old_xml_get_string (tree, "short-name");
	gid         = old_xml_get_int (tree, "group");
	type        = old_xml_get_int (tree, "type");
	units       = old_xml_get_int (tree, "units");
	std_rate    = old_xml_get_float (tree, "std-rate");
	/*ovt_rate = old_xml_get_float (tree, "ovt-rate");*/
	email       = old_xml_get_string (tree, "email");
	calendar_id = old_xml_get_int (tree, "calendar");
        note        = old_xml_get_string (tree, "note");

	if (short_name == NULL) {
		short_name = g_strdup ("");
	}

	if (email == NULL) {
		email = g_strdup ("");
	}

	if (note == NULL) {
		note = g_strdup ("");
	}

	group = g_hash_table_lookup (parser->group_hash, GINT_TO_POINTER (gid));
	calendar = g_hash_table_lookup (parser->calendar_hash, GINT_TO_POINTER (calendar_id));

	resource = g_object_new (MRP_TYPE_RESOURCE,
				 "name", name,
				 "short_name", short_name,
				 "type", type,
				 "group", group,
				 "units", units,
				 "email", email,
				 "calendar", calendar,
				 "note", note,
				 NULL);

	/* These are cost custom properties */
	/* We need first to register the resource in the project */
	mrp_project_add_resource (parser->project, resource);

	mrp_object_set (MRP_OBJECT (resource),
			"cost", std_rate,
			/*"cost_overtime", ovt_rate,*/
			NULL);

	for (child = tree->children; child; child = child->next) {
		if (!strcmp (child->name, "properties")) {
			old_xml_read_custom_properties (parser, child, MRP_OBJECT (resource));
		}
	}

	g_hash_table_insert (parser->resource_hash,
			     GINT_TO_POINTER (id), resource);

	parser->resources = g_list_prepend (parser->resources, resource);

	g_free (name);
	g_free (email);
	g_free (short_name);
	g_free (note);
}

static void
old_xml_read_group (MrpParser *parser, xmlNodePtr tree)
{
	gint      id;
	gchar    *name;
	gchar    *mgr_name, *mgr_phone, *mgr_email;
	MrpGroup *group;

	if (strcmp (tree->name, "group")){
		/*g_warning ("Got %s, expected 'group'.", tree->name);*/
		return;
	}

	id = old_xml_get_int (tree, "id");

	name = old_xml_get_string (tree, "name");
	mgr_name  = old_xml_get_string (tree, "admin-name");
	mgr_phone = old_xml_get_string (tree, "admin-phone");
	mgr_email = old_xml_get_string (tree, "admin-email");

	group = g_object_new (MRP_TYPE_GROUP,
			      "name", name,
			      "manager_name", mgr_name,
			      "manager_phone", mgr_phone,
			      "manager_email", mgr_email,
			      NULL);

	g_free (name);
	g_free (mgr_name);
	g_free (mgr_phone);
	g_free (mgr_email);

	g_hash_table_insert (parser->group_hash, GINT_TO_POINTER (id), group);

	parser->groups = g_list_prepend (parser->groups, group);
}

static void
old_xml_read_assignment (MrpParser *parser, xmlNodePtr tree)
{
	gint   task_id, resource_id, assigned_units;
	MrpAssignment *assignment;
	MrpTask       *task;
	MrpResource   *resource;

	if (strcmp (tree->name, "allocation")){
		/*g_warning ("Got %s, expected 'allocation'.", tree->name);*/
		return;
	}

	task_id = old_xml_get_int (tree, "task-id");
	resource_id = old_xml_get_int (tree, "resource-id");
	assigned_units = old_xml_get_int_with_default (tree, "units",100);

	task = g_hash_table_lookup (parser->task_hash,
				    GINT_TO_POINTER (task_id));
	resource = g_hash_table_lookup (parser->resource_hash,
					GINT_TO_POINTER (resource_id));

	if (!task) {
		g_warning ("Corrupt file? Task %d not found in hash.", task_id);
		goto fail;
	}

	if (!resource) {
		g_warning ("Corrupt file? Resource %d not found in hash.", resource_id);
		goto fail;
	}

	assignment = g_object_new (MRP_TYPE_ASSIGNMENT,
				   "task", task,
				   "resource", resource,
				   "units", assigned_units,
				   NULL);

	parser->assignments = g_list_prepend (parser->assignments, assignment);

 fail:
	; /* Avoid gcc warning. */
}

static void
old_xml_read_day_type (MrpParser *parser, xmlNodePtr tree)
{
	MrpDay  *day;
	gint     id;
	xmlChar *name, *desc;

	id = old_xml_get_int (tree, "id");
	if (id == MRP_DAY_WORK || id == MRP_DAY_NONWORK || id == MRP_DAY_USE_BASE) {
		return;
	}

	if (strcmp (tree->name, "day-type") != 0){
		return;
	}

	name = xmlGetProp (tree, "name");
	if (!name) {
		return;
	}

	desc = xmlGetProp (tree, "description");
	if (!desc) {
		xmlFree (name);
		return;
	}

	day = mrp_day_add (parser->project, name, desc);

	xmlFree (name);
	xmlFree (desc);

	g_hash_table_insert (parser->day_hash, GINT_TO_POINTER (id), day);
}

static void
old_xml_read_default_day (MrpParser   *parser,
			  xmlNodePtr   node,
			  MrpCalendar *calendar,
			  gint         day_id,
			  const gchar *day_name)
{
	gint    id;
	MrpDay *day;

	id = old_xml_get_int (node, day_name);
	day = g_hash_table_lookup (parser->day_hash,
				   GINT_TO_POINTER (id));
	mrp_calendar_set_default_days (calendar, day_id, day, -1);
}

static void
old_xml_read_overridden_day_type (MrpParser   *parser,
				  MrpCalendar *calendar,
				  xmlNodePtr   day)
{
	gint        id;
	MrpDay     *mrp_day;
	GList      *intervals = NULL;
	xmlNodePtr  child;

	if (strcmp (day->name, "overridden-day-type") != 0){
		return;
	}

	id = old_xml_get_int (day, "id");
	mrp_day = g_hash_table_lookup (parser->day_hash, GINT_TO_POINTER (id));

	for (child = day->children; child; child = child->next) {
		if (strcmp (child->name, "interval") == 0) {
			MrpInterval *interval;
			gchar       *str;
			gint         hour, min;
			mrptime      start, end;

			str = old_xml_get_string (child, "start");
			if (sscanf (str, "%02d%02d", &hour, &min) != 2) {
				g_free (str);
				continue;
			}
			g_free (str);
			start = hour * 60 * 60 + min * 60;

			str = old_xml_get_string (child, "end");
			if (sscanf (str, "%02d%02d", &hour, &min) != 2) {
				g_free (str);
				continue;
			}
			g_free (str);
			end = hour * 60 * 60 + min * 60;

			interval = mrp_interval_new (start, end);
			intervals = g_list_append (intervals, interval);
		}
	}

	mrp_calendar_day_set_intervals (calendar, mrp_day, intervals);

	g_list_foreach (intervals, (GFunc) mrp_interval_unref, NULL);
	g_list_free (intervals);
}

static void
old_xml_read_overridden_day (MrpParser   *parser,
			     MrpCalendar *calendar,
			     xmlNodePtr   day)
{
	gint     id;
	mrptime  date;
	xmlChar *xml_str;
	MrpDay  *mrp_day;
	gint     y, m, d;

	if (strcmp (day->name, "day") != 0){
		return;
	}

	xml_str = xmlGetProp (day, "type");
	if (!xml_str) {
		return;
	}
	if (strcmp (xml_str, "day-type") != 0) {
		xmlFree (xml_str);
		return;
	}
	xmlFree (xml_str);

	id = old_xml_get_int (day, "id");

	mrp_day = g_hash_table_lookup (parser->day_hash, GINT_TO_POINTER (id));

	xml_str = xmlGetProp (day, "date");
	if (!xml_str) {
		return;
	}

	if (sscanf (xml_str, "%04d%02d%02d", &y, &m, &d) == 3) {
		date = mrp_time_compose (y, m, d, 0, 0, 0);
		mrp_calendar_set_days (calendar, date, mrp_day, (mrptime) -1);
	} else {
		g_warning ("Invalid time format for overridden day.");
	}

	xmlFree (xml_str);
}

static void
old_xml_read_calendar (MrpParser *parser, MrpCalendar *parent, xmlNodePtr tree)
{
	MrpCalendar *calendar = NULL;
	xmlChar     *name;
	xmlNodePtr   child;
	gint         id;

	if (strcmp (tree->name, "calendar") != 0){
		return;
	}

	name = xmlGetProp (tree, "name");
	if (!name) {
		return;
	}

	if (parent) {
		calendar = mrp_calendar_derive (name, parent);
	} else {
		calendar = mrp_calendar_new (name, parser->project);
	}

 	xmlFree (name);

	id = old_xml_get_int (tree, "id");
	g_hash_table_insert (parser->calendar_hash,
			     GINT_TO_POINTER (id),
			     calendar);

	for (child = tree->children; child; child = child->next) {
		if (strcmp (child->name, "calendar") == 0) {
			old_xml_read_calendar (parser, calendar, child);
		}
		else if (strcmp (child->name, "default-week") == 0) {
			old_xml_read_default_day (parser, child, calendar,
						  MRP_CALENDAR_DAY_MON, "mon");
			old_xml_read_default_day (parser, child, calendar,
						  MRP_CALENDAR_DAY_TUE, "tue");
			old_xml_read_default_day (parser, child, calendar,
						  MRP_CALENDAR_DAY_WED, "wed");
			old_xml_read_default_day (parser, child, calendar,
						  MRP_CALENDAR_DAY_THU, "thu");
			old_xml_read_default_day (parser, child, calendar,
						  MRP_CALENDAR_DAY_FRI, "fri");
			old_xml_read_default_day (parser, child, calendar,
						  MRP_CALENDAR_DAY_SAT, "sat");
			old_xml_read_default_day (parser, child, calendar,
						  MRP_CALENDAR_DAY_SUN, "sun");
		}
		else if (strcmp (child->name, "overridden-day-types") == 0) {
			xmlNodePtr day;

			for (day = child->children; day; day = day->next) {
				old_xml_read_overridden_day_type (parser,
								  calendar,
								  day);
			}
		}
		else if (strcmp (child->name, "days") == 0) {
			xmlNodePtr day;

			for (day = child->children; day; day = day->next) {
				old_xml_read_overridden_day (parser,
							     calendar,
							     day);
			}
		}
	}
}

static void
old_xml_read_project_properties (MrpParser *parser)
{
	xmlNodePtr  node;
	gchar      *name;
	gchar      *org;
	gchar      *manager;
	gchar      *phase;

	node = parser->doc->children;

	parser->version = old_xml_get_int_with_default (node,
							"mrproject-version",
							1);

	if (parser->version > 1) {
		parser->project_start = old_xml_get_date (node,
							  "project-start");
	}

	name = old_xml_get_string (node, "name");
	org = old_xml_get_string (node, "company");
	manager = old_xml_get_string (node, "manager");
	phase = old_xml_get_string (node, "phase");

	parser->project_calendar_id = old_xml_get_int_with_default (node, "calendar", 0);

	g_object_set (parser->project,
		      "name", name,
		      "organization", org,
		      "manager", manager,
		      "phase", phase,
		      NULL);

	g_free (name);
	g_free (org);
	g_free (manager);
	g_free (phase);
}

static GArray *
old_xml_read_string_list (xmlNodePtr  node,
			  MrpObject  *object)
{
	GArray      *array;
	GValue       value = { 0 };
	xmlNodePtr   child;
	gchar       *str;

	if (!node->children) {
		return NULL;
	}

	array = g_array_new (TRUE, TRUE, sizeof(GValue *));
	g_array_set_clear_func (array, (GDestroyNotify) g_value_unset);

	g_value_init (&value, G_TYPE_STRING);


	for (child = node->children; child; child = child->next) {
		if (!strcmp (child->name, "list-item")) {
			str = old_xml_get_string (child, "value");

			if (str && str[0]) {
				g_value_set_string (&value, str);
				g_array_append_val (array, value);
			}

			g_free (str);
		}
	}

	g_value_unset (&value);

	return array;
}

/* Note: this is a hack to read phases as custom properties from files created
 * with 0.8pre, 0.8, 0.9pre.
 */
static GList *
old_xml_read_crufty_phases (xmlNodePtr  node)
{
	xmlNodePtr  child;
	gchar      *str;
	GList      *phases = NULL;

	if (!node->children) {
		return NULL;
	}

	for (child = node->children; child; child = child->next) {
		if (!strcmp (child->name, "list-item")) {
			str = old_xml_get_string (child, "value");

			if (str && str[0]) {
				phases = g_list_prepend (phases, str);
			}
		}
	}

	phases = g_list_reverse (phases);

	return phases;
}

static void
old_xml_read_property_specs (MrpParser *parser)
{
	xmlNodePtr       node;
	xmlNodePtr       child;
	gchar           *name;
	gchar           *label;
	gchar           *description;
	gchar           *owner_str;
	gchar           *type_str;
	MrpProperty     *property;
	MrpPropertyType  type;
	GType            owner;

	node = old_xml_search_child (parser->doc->children, "properties");

	if (node == NULL) {
		return;
	}

	for (child = node->children; child; child = child->next) {
		if (strcmp (child->name, "property")) {
			continue;
		}

		name = old_xml_get_string (child, "name");

		if (!strcmp (name, "phases") || !strcmp (name, "phase")) {
			g_free (name);
			continue;
		}

		label       = old_xml_get_string (child, "label");
		description = old_xml_get_string (child, "description");
		owner_str   = old_xml_get_string (child, "owner");
		type_str    = old_xml_get_string (child, "type");

		type = old_xml_property_type_from_string (type_str);

		property = mrp_property_new (name,
					     type,
					     label,
					     description,
					     TRUE);

		/* If we could't create the property, this means that we are
		 * trying to use a name that is already taken.
		 */
		if (!property) {
			g_free (name);
			g_free (type_str);
			g_free (owner_str);
			g_free (label);
			g_free (description);
			continue;
		}

		if (!strcmp (owner_str, "task")) {
			owner = MRP_TYPE_TASK;
		}
		else if (!strcmp (owner_str, "resource")) {
			owner = MRP_TYPE_RESOURCE;
		}
		else if (!strcmp (owner_str, "project")) {
			owner = MRP_TYPE_PROJECT;
		} else {
			g_warning ("Invalid owners %s.", owner_str);
			continue;
		}

		/* Check if the property already exists. I'm not sure this is
		 * the best solution, but a quick solution is needed to get
		 * project phases working now.
		 */
		if (!mrp_project_has_property (parser->project, owner, name)) {
			mrp_project_add_property (parser->project,
						  owner,
						  property,
						  TRUE /* FIXME: user_defined, should
							  be read from the file */);
		}

		g_free (name);
		g_free (type_str);
		g_free (owner_str);
		g_free (label);
		g_free (description);
	}
}

static void
old_xml_read_phases (MrpParser *parser)
{
	xmlNodePtr  node;
	xmlNodePtr  child;
	GList      *phases = NULL;
	gchar      *name;

	node = old_xml_search_child (parser->doc->children, "phases");
	if (node == NULL) {
		return;
	}

	for (child = node->children; child; child = child->next) {
		if (strcmp (child->name, "phase")) {
			continue;
		}

		name = old_xml_get_string (child, "name");
		phases = g_list_prepend (phases, name);
	}

	phases = g_list_reverse (phases);
	g_object_set (parser->project, "phases", phases, NULL);
	mrp_string_list_free (phases);
}

static gboolean
old_xml_read_project (MrpParser *parser)
{
	xmlNodePtr   child;
	xmlNodePtr   tasks, resources, groups, assignments, calendars;
	gint         gid;
	MrpCalendar *calendar;

	old_xml_read_project_properties (parser);

	parser->root_task = mrp_task_new ();

	child = parser->doc->children;

	child = child->children;

	/* Get the first "properties" node, i.e. specs. */
	while (child) {
		if (!strcmp (child->name, "properties")) {
			old_xml_read_property_specs (parser);

			child = child->next;
			break;
		}

		child = child->next;
	}

	/* Get the second "properties" node, i.e. values. */
	while (child) {
		if (!strcmp (child->name, "properties")) {
			old_xml_read_custom_properties (parser,
							child,
							MRP_OBJECT (parser->project));
			break;
		}

		child = child->next;
	}

	old_xml_read_phases (parser);

	/* Load calendars */
	calendars = old_xml_search_child (parser->doc->children, "calendars");
	if (calendars != NULL) {
		xmlNodePtr day;

 		child = old_xml_search_child (calendars, "day-types");

		/* Insert hardcoded days. */
		g_hash_table_insert (parser->day_hash,
				     GINT_TO_POINTER (MRP_DAY_WORK),
				     mrp_day_ref (mrp_day_get_work ()));
		g_hash_table_insert (parser->day_hash,
				     GINT_TO_POINTER (MRP_DAY_NONWORK),
				     mrp_day_ref (mrp_day_get_nonwork ()));
		g_hash_table_insert (parser->day_hash,
				     GINT_TO_POINTER (MRP_DAY_USE_BASE),
				     mrp_day_ref (mrp_day_get_use_base ()));

		if (child != NULL) {
		        /* Read day-types */
        		for (day = child->children; day; day = day->next) {
        			old_xml_read_day_type (parser, day);
        		}
		}

		for (child = calendars->children; child; child = child->next) {
			if (strcmp (child->name, "calendar") == 0) {
				old_xml_read_calendar (parser, NULL, child);
			}
		}
	}

	/* Set project calendar now that we have calendars. */
	if (parser->project_calendar_id) {
		calendar = g_hash_table_lookup (parser->calendar_hash,
						GINT_TO_POINTER (parser->project_calendar_id));

		g_object_set (parser->project, "calendar", calendar, NULL);
	}

	/* Load tasks. */
	child = old_xml_search_child (parser->doc->children, "tasks");
	if (child != NULL) {
		for (tasks = child->children; tasks; tasks = tasks->next) {
			old_xml_read_task (parser, tasks, parser->root_task);
		}
	}

	/* Note: if we read a G1 file, there was no project-start set, so make
	 * sure the one we faked from the earliest task starts on at 00:00.
	 */
	if (parser->version == 1) {
		mrp_time_align_day (parser->project_start);
	}

	/* Load resource groups. */
	child = old_xml_search_child (parser->doc->children,
				      "resource-groups");
	if (child != NULL) {
		for (groups = child->children; groups; groups = groups->next) {
			old_xml_read_group (parser, groups);
		}

		/* Gah, why underscore?! */
		gid = old_xml_get_int (child, "default_group");
		parser->default_group = g_hash_table_lookup (parser->group_hash,
							     GINT_TO_POINTER (gid));
	}

	/* Load resources. */
	child = old_xml_search_child (parser->doc->children, "resources");
	if (child != NULL) {
		for (resources = child->children; resources; resources = resources->next) {
			old_xml_read_resource (parser, resources);
		}
	}
	parser->resources = g_list_reverse (parser->resources);

	/* Load assignments. */
	child = old_xml_search_child (parser->doc->children, "allocations");
	if (child != NULL) {
		for (assignments = child->children; assignments; assignments = assignments->next) {
			old_xml_read_assignment (parser, assignments);
		}
	}

	return TRUE;
}


/***************
 * XML helpers.
 */


/* Get a value for a node either carried as an attibute or as
 * the content of a child.
 *
 * Returns xmlChar, must be xmlFreed.
 */
static gchar *
old_xml_get_value (xmlNodePtr node, const char *name)
{
	gchar      *val;
	xmlNodePtr  child;

	val = (gchar *) xmlGetProp (node, name);

	if (val != NULL) {
		return val;
	}

	child = node->children;

	while (child != NULL) {
		if (!strcmp (child->name, name)) {
		        /*
			 * !!! Inefficient, but ...
			 */
			val = xmlNodeGetContent(child);
			if (val != NULL) {
				return val;
			}
		}
		child = child->next;
	}

	return NULL;
}

/*
 * Get a value for a node either carried as an attibute or as
 * the content of a child.
 *
 * Returns a g_malloc'ed string.  Caller must free.
 */
static gchar *
old_xml_get_string (xmlNodePtr node, const char *name)
{
	char *ret, *val;

	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	val = old_xml_get_value (node, name);
	ret = g_strdup (val);
	xmlFree (val);

	return ret;
}

static mrptime
old_xml_get_date (xmlNodePtr node, const char *name)
{
	gchar   *val;
	mrptime  t;

	g_return_val_if_fail (node != NULL, MRP_TIME_INVALID);
	g_return_val_if_fail (name != NULL, MRP_TIME_INVALID);

	val = old_xml_get_value (node, name);

	t = mrp_time_from_string (val, NULL);

	xmlFree (val);

	return t;
}

static gint
old_xml_get_int (xmlNodePtr node, const char *name)
{
	gchar *val;
	gint   ret;

	g_return_val_if_fail (node != NULL, 0);
	g_return_val_if_fail (name != NULL, 0);

	val = old_xml_get_value (node, name);
	if (val == NULL) {
		return 0;
	}

	ret = atoi (val);

	xmlFree (val);

	return ret;
}

static gint
old_xml_get_int_with_default (xmlNodePtr node, const char *name, gint def)
{
	gchar *val;
	gint   ret;

	g_return_val_if_fail (node != NULL, def);
	g_return_val_if_fail (name != NULL, def);

	val = old_xml_get_value (node, name);
	if (val == NULL) {
		return def;
	}

	ret = atoi (val);

	xmlFree (val);

	return ret;
}

static gfloat
old_xml_get_float (xmlNodePtr node, const char *name)
{
	gchar *val;
	gfloat ret;

	g_return_val_if_fail (node != NULL, 0);
	g_return_val_if_fail (name != NULL, 0);

	val = old_xml_get_value (node, name);
	if (val == NULL) {
		return 0;
	}

	ret = g_ascii_strtod (val, NULL);

	xmlFree (val);

	return ret;
}

static MrpTaskType
old_xml_get_task_type (xmlNodePtr node, const char *name)
{
	gchar       *val;
	MrpTaskType  type;

	g_return_val_if_fail (node != NULL, 0);
	g_return_val_if_fail (name != NULL, 0);

	val = old_xml_get_value (node, name);
	if (val == NULL) {
		return MRP_TASK_TYPE_NORMAL;
	}

	if (!strcmp (val, "milestone")) {
		type = MRP_TASK_TYPE_MILESTONE;
	}
	else if (!strcmp (val, "normal")) {
		type = MRP_TASK_TYPE_NORMAL;
	} else {
		type = MRP_TASK_TYPE_NORMAL;
	}

	xmlFree (val);

	return type;
}

static MrpTaskSched
old_xml_get_task_sched (xmlNodePtr node, const char *name)
{
	gchar        *val;
	MrpTaskSched  sched;

	g_return_val_if_fail (node != NULL, 0);
	g_return_val_if_fail (name != NULL, 0);

	val = old_xml_get_value (node, name);
	if (val == NULL) {
		return MRP_TASK_SCHED_FIXED_WORK;
	}

	if (!strcmp (val, "fixed-duration")) {
		sched = MRP_TASK_SCHED_FIXED_DURATION;
	}
	else if (!strcmp (val, "fixed-work")) {
		sched = MRP_TASK_SCHED_FIXED_WORK;
	} else {
		sched = MRP_TASK_SCHED_FIXED_WORK;
	}

	xmlFree (val);

	return sched;
}

/* Search a child by name, if needed go down the tree to find it.
 */
static xmlNodePtr
old_xml_search_child (xmlNodePtr node, const gchar *name)
{
	xmlNodePtr ret;
	xmlNodePtr child;

	child = node->children;
	while (child != NULL) {
		if (!strcmp (child->name, name))
			return child;
		child = child->next;
	}
	child = node->children;
	while (child != NULL) {
		ret = old_xml_search_child (child, name);
		if (ret != NULL)
			return ret;
		child = child->next;
	}
	return NULL;
}

static MrpPropertyType
old_xml_property_type_from_string (const gchar *str)
{
	if (!strcmp (str, "int")) {
		return MRP_PROPERTY_TYPE_INT;
	}
	else if (!strcmp (str, "text")) {
		return MRP_PROPERTY_TYPE_STRING;
	}
	else if (!strcmp (str, "text-list")) {
		return MRP_PROPERTY_TYPE_STRING_LIST;
	}
	else if (!strcmp (str, "float")) {
		return MRP_PROPERTY_TYPE_FLOAT;
	}
	else if (!strcmp (str, "date")) {
		return MRP_PROPERTY_TYPE_DATE;
	}
	else if (!strcmp (str, "duration")) {
		return MRP_PROPERTY_TYPE_DURATION;
	}
	else if (!strcmp (str, "cost")) {
		/* FIXME: implement */
	} else {
		g_warning ("Not implemented support for type");
	}

	return MRP_PROPERTY_TYPE_NONE;
}

static void
old_xml_set_property_from_node (MrpProject *project,
				MrpObject  *object,
				xmlNodePtr  node)
{
	MrpProperty     *property;
	MrpPropertyType  type;
	mrptime          date;
	gint             i;
	gfloat           f;
	gchar           *name;
	gchar           *str;
	GArray          *array;
	GList           *phases;

	name  = old_xml_get_string (node, "name");

	if (!strcmp (name, "phases")) {
		phases = old_xml_read_crufty_phases (node);
		g_object_set (project, "phases", phases, NULL);
		mrp_string_list_free (phases);

		g_free (name);
		return;
	}
	else if (!strcmp (name, "phase")) {
		str = old_xml_get_string (node, "value");
		g_object_set (project, "phase", str, NULL);

		g_free (str);
		g_free (name);
		return;
	}

	str = old_xml_get_string (node, "value");

	/* Check if we have the property first. This is needed both to be robust
	 * against broken input.
	 */
	if (!mrp_project_has_property (project,
				       G_OBJECT_TYPE (object),
				       name)) {
		g_free (name);
		g_free (str);
		return;
	}

	property = mrp_project_get_property (project,
					     name,
					     G_OBJECT_TYPE (object));

	type = mrp_property_get_property_type (property);

	switch (type) {
	case MRP_PROPERTY_TYPE_STRING:
		mrp_object_set (object, name, str, NULL);
		break;
	case MRP_PROPERTY_TYPE_STRING_LIST:
		array = old_xml_read_string_list (node, object);
		if (array) {
			mrp_object_set (object, name, array, NULL);
			g_array_free (array, TRUE);
		}
		break;
	case MRP_PROPERTY_TYPE_INT:
		i = atoi (str);
		mrp_object_set (object, name, i, NULL);
		break;
	case MRP_PROPERTY_TYPE_FLOAT:
		f = g_ascii_strtod (str, NULL);
		mrp_object_set (object, name, f, NULL);
		break;
	case MRP_PROPERTY_TYPE_DURATION:
		i = atoi (str);
		mrp_object_set (object, name, i, NULL);
		break;
	case MRP_PROPERTY_TYPE_DATE:
		date = mrp_time_from_string (str, NULL);
		mrp_object_set (object, name, &date, NULL);
		break;
	case MRP_PROPERTY_TYPE_COST:
		/* FIXME: implement. */
		break;
	default:
		g_warning ("Not implemented support for type.");
		break;
	}

	g_free (name);
	g_free (str);
}

static void
old_xml_process_delayed_relations (MrpParser *parser)
{
	GList           *l;
	MrpTask         *task, *predecessor_task;
	DelayedRelation *relation;

	for (l = parser->delayed_relations; l; l = l->next) {
		relation = l->data;

		task = g_hash_table_lookup (parser->task_hash,
					    GINT_TO_POINTER (relation->successor_id));
		g_assert (task != NULL);

		predecessor_task = g_hash_table_lookup (parser->task_hash,
							GINT_TO_POINTER (relation->predecessor_id));
		g_assert (predecessor_task != NULL);

		mrp_task_add_predecessor (task,
					  predecessor_task,
					  relation->type,
					  relation->lag,
					  NULL);

		g_free (relation);
	}
}

gboolean
mrp_old_xml_parse (MrpProject *project, xmlDoc *doc, GError **error)
{
	MrpParser       parser;
	gboolean        success;
        MrpTaskManager *task_manager;
        MrpAssignment  *assignment;
        GList          *node;

	g_return_val_if_fail (MRP_IS_PROJECT (project), FALSE);
	g_return_val_if_fail (doc != NULL, FALSE);

	memset (&parser, 0, sizeof (parser));

	parser.project_start = -1;
	parser.project = g_object_ref (project);

	parser.doc = doc;

	parser.task_hash = g_hash_table_new (NULL, NULL);
	parser.resource_hash = g_hash_table_new (NULL, NULL);
	parser.group_hash = g_hash_table_new (NULL, NULL);
	parser.day_hash = g_hash_table_new_full (NULL, NULL,
						 NULL,
						 (GDestroyNotify) mrp_day_unref);
	parser.calendar_hash = g_hash_table_new (NULL, NULL);

	success = old_xml_read_project (&parser);

	g_hash_table_destroy (parser.resource_hash);
	g_hash_table_destroy (parser.group_hash);
	g_hash_table_destroy (parser.day_hash);
	g_hash_table_destroy (parser.calendar_hash);

	if (!success) {
		return FALSE;
	}

        task_manager = imrp_project_get_task_manager (project);
        mrp_task_manager_set_root (task_manager, parser.root_task);

	parser.project_start = mrp_time_align_day (parser.project_start);

	g_object_set (project,
                      "project-start", parser.project_start,
                      "default-group", parser.default_group,
                      NULL);

	old_xml_process_delayed_relations (&parser);

        g_object_set_data (G_OBJECT (project),
                           "version", GINT_TO_POINTER (parser.version));

        g_hash_table_destroy (parser.task_hash);
	g_list_free (parser.delayed_relations);

       	imrp_project_set_groups (project, parser.groups);

       	for (node = parser.assignments; node; node = node->next) {
		assignment = MRP_ASSIGNMENT (node->data);

		imrp_task_add_assignment (mrp_assignment_get_task (assignment),
					  assignment);
		imrp_resource_add_assignment (mrp_assignment_get_resource (assignment),
					      assignment);
		g_object_unref (assignment);
	}

        g_list_free (parser.assignments);
        g_list_free (parser.resources);

	return TRUE;
}

