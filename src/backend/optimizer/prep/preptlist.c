/*-------------------------------------------------------------------------
 *
 * preptlist.c
 *	  Routines to preprocess the parse tree target list
 *
 * Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $Header$
 *
 *-------------------------------------------------------------------------
 */
#include <string.h>
#include "postgres.h"





#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/prep.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static List *expand_targetlist(List *tlist, Oid relid, int command_type,
				  Index result_relation);
static List *replace_matching_resname(List *new_tlist,
						 List *old_tlist);
static List *new_relation_targetlist(Oid relid, Index rt_index,
						NodeTag node_type);


/*
 * preprocess_targetlist
 *	  Driver for preprocessing the parse tree targetlist.
 *
 *	  1. Deal with appends and replaces by filling missing attributes
 *		 in the target list.
 *	  2. Reset operator OIDs to the appropriate regproc ids.
 *
 *	  Returns the new targetlist.
 */
List *
preprocess_targetlist(List *tlist,
					  int command_type,
					  Index result_relation,
					  List *range_table)
{
	List	   *expanded_tlist = NIL;
	Oid			relid = InvalidOid;
	List	   *t_list = NIL;
	List	   *temp = NIL;

	if (result_relation >= 1 && command_type != CMD_SELECT)
		relid = getrelid(result_relation, range_table);

	/*
	 * for heap_formtuple to work, the targetlist must match the exact
	 * order of the attributes. We also need to fill in the missing
	 * attributes here.							-ay 10/94
	 */
	expanded_tlist = expand_targetlist(tlist, relid, command_type, result_relation);

	/* XXX should the fix-opids be this early?? */
	/* was mapCAR  */
	foreach(temp, expanded_tlist)
	{
		TargetEntry *tle = lfirst(temp);

		if (tle->expr)
			fix_opid(tle->expr);
	}
	t_list = copyObject(expanded_tlist);

	/* ------------------
	 *	for "replace" or "delete" queries, add ctid of the result
	 *	relation into the target list so that the ctid can get
	 *	propogate through the execution and in the end ExecReplace()
	 *	will find the right tuple to replace or delete.  This
	 *	extra field will be removed in ExecReplace().
	 *	For convinient, we append this extra field to the end of
	 *	the target list.
	 * ------------------
	 */
	if (command_type == CMD_UPDATE || command_type == CMD_DELETE)
	{
		TargetEntry *ctid;
		Resdom	   *resdom;
		Var		   *var;

		resdom = makeResdom(length(t_list) + 1,
							TIDOID,
							-1,
							"ctid",
							0,
							0,
							true);

		var = makeVar(result_relation, -1, TIDOID, -1, 0, result_relation, -1);

		ctid = makeTargetEntry(resdom, (Node *) var);
		t_list = lappend(t_list, ctid);
	}

	return t_list;
}

/*****************************************************************************
 *
 *		TARGETLIST EXPANSION
 *
 *****************************************************************************/

/*
 * expand_targetlist
 *	  Given a target list as generated by the parser and a result relation,
 *	  add targetlist entries for the attributes which have not been used.
 *
 *	  XXX This code is only supposed to work with unnested relations.
 *
 *	  'tlist' is the original target list
 *	  'relid' is the relid of the result relation
 *	  'command' is the update command
 *
 * Returns the expanded target list, sorted in resno order.
 */
static List *
expand_targetlist(List *tlist,
				  Oid relid,
				  int command_type,
				  Index result_relation)
{
	NodeTag		node_type = T_Invalid;

	switch (command_type)
	{
		case CMD_INSERT:
			node_type = (NodeTag) T_Const;
			break;
		case CMD_UPDATE:
			node_type = (NodeTag) T_Var;
			break;
	}

	if (node_type != T_Invalid)
	{
		List	   *ntlist = new_relation_targetlist(relid,
													 result_relation,
													 node_type);

		return replace_matching_resname(ntlist, tlist);
	}
	else
		return tlist;

}


static List *
replace_matching_resname(List *new_tlist, List *old_tlist)
{
	List	   *temp,
			   *i;
	List	   *t_list = NIL;

	foreach(i, new_tlist)
	{
		TargetEntry *new_tle = (TargetEntry *) lfirst(i);
		TargetEntry *matching_old_tl = NULL;

		foreach(temp, old_tlist)
		{
			TargetEntry *old_tle = (TargetEntry *) lfirst(temp);

			old_tle = lfirst(temp);
			if (!strcmp(old_tle->resdom->resname,
						new_tle->resdom->resname))
			{
				matching_old_tl = old_tle;
				break;
			}
		}

		if (matching_old_tl)
		{
			matching_old_tl->resdom->resno = new_tle->resdom->resno;
			t_list = lappend(t_list, matching_old_tl);
		}
		else
			t_list = lappend(t_list, new_tle);
	}

	/*
	 * It is possible that 'old_tlist' has some negative attributes (i.e.
	 * negative resnos). This only happens if this is a replace/append
	 * command and we explicitly specify a system attribute. Of course
	 * this is not a very good idea if this is a user query, but on the
	 * other hand the rule manager uses this mechanism to replace rule
	 * locks.
	 *
	 * So, copy all these entries to the end of the target list and set their
	 * 'resjunk' value to true to show that these are special attributes
	 * and have to be treated specially by the executor!
	 */
	foreach(temp, old_tlist)
	{
		TargetEntry *old_tle,
				   *new_tl;
		Resdom	   *newresno;

		old_tle = lfirst(temp);
		if (old_tle->resdom->resno < 0)
		{
			newresno = (Resdom *) copyObject((Node *) old_tle->resdom);
			newresno->resno = length(t_list) + 1;
			newresno->resjunk = true;
			new_tl = makeTargetEntry(newresno, old_tle->expr);
			t_list = lappend(t_list, new_tl);
		}

		/*
		 * Also it is possible that the parser or rewriter added some junk
		 * attributes to hold GROUP BY expressions which are not part of
		 * the result attributes. We can simply identify them by looking
		 * at the resgroupref in the TLE's resdom, which is a unique
		 * number telling which TLE belongs to which GroupClause.
		 */
		if (old_tle->resdom->resgroupref > 0)
		{
			bool		already_there = FALSE;
			TargetEntry *new_tle;
			Resdom	   *newresno;

			/*
			 * Check if the tle is already in the new list
			 */
			foreach(i, t_list)
			{
				new_tle = (TargetEntry *) lfirst(i);

				if (new_tle->resdom->resgroupref ==
					old_tle->resdom->resgroupref)
				{
					already_there = TRUE;
					break;
				}

			}

			/*
			 * If not, add it and make sure it is now a junk attribute
			 */
			if (!already_there)
			{
				newresno = (Resdom *) copyObject((Node *) old_tle->resdom);
				newresno->resno = length(t_list) + 1;
				newresno->resjunk = true;
				new_tl = makeTargetEntry(newresno, old_tle->expr);
				t_list = lappend(t_list, new_tl);
			}
		}
	}

	return t_list;
}

/*
 * new_relation_targetlist
 *	  Generate a targetlist for the relation with relation OID 'relid'
 *	  and rangetable index 'rt_index'.
 *
 *	  Returns the new targetlist.
 */
static List *
new_relation_targetlist(Oid relid, Index rt_index, NodeTag node_type)
{
	List	   *t_list = NIL;
	int			natts = get_relnatts(relid);
	AttrNumber	attno;

	for (attno = 1; attno <= natts; attno++)
	{
		HeapTuple	tp;
		Form_pg_attribute att_tup;
		char	   *attname;
		Oid			atttype;
		int32		atttypmod;
		bool		attisset;

		tp = SearchSysCacheTuple(ATTNUM,
								 ObjectIdGetDatum(relid),
								 UInt16GetDatum(attno),
								 0, 0);
		if (! HeapTupleIsValid(tp))
		{
			elog(ERROR, "new_relation_targetlist: no attribute tuple %u %d",
				 relid, attno);
		}
		att_tup = (Form_pg_attribute) GETSTRUCT(tp);
		attname = pstrdup(att_tup->attname.data);
		atttype = att_tup->atttypid;
		atttypmod = att_tup->atttypmod;
		attisset = att_tup->attisset;

		switch (node_type)
		{
			case T_Const:		/* INSERT command */
				{
					struct varlena *typedefault = get_typdefault(atttype);
					int			typlen;
					Const	   *temp_const;
					TargetEntry *temp_tle;

					if (typedefault == NULL)
						typlen = 0;
					else
					{
						/*
						 * Since this is an append or replace, the size of
						 * any set attribute is the size of the OID used to
						 * represent it.
						 */
						if (attisset)
							typlen = get_typlen(OIDOID);
						else
							typlen = get_typlen(atttype);
					}

					temp_const = makeConst(atttype,
										   typlen,
										   (Datum) typedefault,
										   (typedefault == NULL),
										   /* XXX ? */
										   false,
										   false, /* not a set */
										   false);

					temp_tle = makeTargetEntry(makeResdom(attno,
														  atttype,
														  -1,
														  attname,
														  0,
														  (Oid) 0,
														  false),
											   (Node *) temp_const);
					t_list = lappend(t_list, temp_tle);
					break;
				}
			case T_Var:			/* UPDATE command */
				{
					Var		   *temp_var;
					TargetEntry *temp_tle;

					temp_var = makeVar(rt_index, attno, atttype, atttypmod,
									   0, rt_index, attno);

					temp_tle = makeTargetEntry(makeResdom(attno,
														  atttype,
														  atttypmod,
														  attname,
														  0,
														  (Oid) 0,
														  false),
											   (Node *) temp_var);
					t_list = lappend(t_list, temp_tle);
					break;
				}
			default:			/* do nothing */
				break;
		}
	}

	return t_list;
}
