/*-------------------------------------------------------------------------
 *
 * pg_dump_sort.c
 *	  Sort the items of a dump into a safe order for dumping
 *
 *
 * Portions Copyright (c) 1996-2003, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  $PostgreSQL$
 *
 *-------------------------------------------------------------------------
 */
#include "pg_dump.h"
#include "pg_backup_archiver.h"


static char *modulename = gettext_noop("sorter");

/*
 * Sort priority for object types.  Objects are sorted by priority,
 * and within an equal priority level by OID.  (This is a relatively
 * crude hack to provide semi-reasonable behavior for old databases
 * without full dependency info.)
 */
static const int objectTypePriority[] =
{
	1,				/* DO_NAMESPACE */
	2,				/* DO_TYPE */
	2,				/* DO_FUNC */
	2,				/* DO_AGG */
	3,				/* DO_OPERATOR */
	4,				/* DO_OPCLASS */
	5,				/* DO_CONVERSION */
	6,				/* DO_TABLE */
	7,				/* DO_ATTRDEF */
	10,				/* DO_INDEX */
	11,				/* DO_RULE */
	12,				/* DO_TRIGGER */
	9,				/* DO_CONSTRAINT */
	13,				/* DO_FK_CONSTRAINT */
	2,				/* DO_PROCLANG */
	2,				/* DO_CAST */
	8				/* DO_TABLE_DATA */
};


static int	DOTypeCompare(const void *p1, const void *p2);
static bool TopoSort(DumpableObject **objs,
					 int numObjs,
					 DumpableObject **ordering,
					 int *nOrdering);
static bool findLoop(DumpableObject *obj,
					 int depth,
					 DumpableObject **ordering,
					 int *nOrdering);
static void repairDependencyLoop(DumpableObject **loop,
								 int nLoop);
static void describeDumpableObject(DumpableObject *obj,
								   char *buf, int bufsize);


/*
 * Sort the given objects into a type/OID-based ordering
 *
 * Normally this is just the starting point for the dependency-based
 * ordering.
 */
void
sortDumpableObjectsByType(DumpableObject **objs, int numObjs)
{
	if (numObjs > 1)
		qsort((void *) objs, numObjs, sizeof(DumpableObject *), DOTypeCompare);
}

static int
DOTypeCompare(const void *p1, const void *p2)
{
	DumpableObject *obj1 = *(DumpableObject **) p1;
	DumpableObject *obj2 = *(DumpableObject **) p2;
	int			cmpval;

	cmpval = objectTypePriority[obj1->objType] -
		objectTypePriority[obj2->objType];

	if (cmpval != 0)
		return cmpval;

	return oidcmp(obj1->catId.oid, obj2->catId.oid);
}


/*
 * Sort the given objects into a safe dump order using dependency
 * information (to the extent we have it available).
 */
void
sortDumpableObjects(DumpableObject **objs, int numObjs)
{
	DumpableObject  **ordering;
	int			nOrdering;

	ordering = (DumpableObject **) malloc(numObjs * sizeof(DumpableObject *));
	if (ordering == NULL)
		exit_horribly(NULL, modulename, "out of memory\n");

	while (!TopoSort(objs, numObjs, ordering, &nOrdering))
		repairDependencyLoop(ordering, nOrdering);

	memcpy(objs, ordering, numObjs * sizeof(DumpableObject *));

	free(ordering);
}

/*
 * TopoSort -- topological sort of a dump list
 *
 * Generate a re-ordering of the dump list that satisfies all the dependency
 * constraints shown in the dump list.  (Each such constraint is a fact of a
 * partial ordering.)  Minimize rearrangement of the list not needed to
 * achieve the partial ordering.
 *
 * This is a lot simpler and slower than, for example, the topological sort
 * algorithm shown in Knuth's Volume 1.  However, Knuth's method doesn't
 * try to minimize the damage to the existing order.
 *
 * Returns TRUE if able to build an ordering that satisfies all the
 * constraints, FALSE if not (there are contradictory constraints).
 *
 * On success (TRUE result), ordering[] is filled with an array of
 * DumpableObject pointers, of length equal to the input list length.
 *
 * On failure (FALSE result), ordering[] is filled with an array of
 * DumpableObject pointers of length *nOrdering, representing a circular set
 * of dependency constraints.  (If there is more than one cycle in the given
 * constraints, one is picked at random to return.)
 *
 * The caller is responsible for allocating sufficient space at *ordering.
 */
static bool
TopoSort(DumpableObject **objs,
		 int numObjs,
		 DumpableObject **ordering,		/* output argument */
		 int *nOrdering)				/* output argument */
{
	DumpId		maxDumpId = getMaxDumpId();
	bool		result = true;
	DumpableObject  **topoItems;
	DumpableObject   *obj;
	int		   *beforeConstraints;
	int			i,
				j,
				k,
				last;

	/* First, create work array with the dump items in their current order */
	topoItems = (DumpableObject **) malloc(numObjs * sizeof(DumpableObject *));
	if (topoItems == NULL)
		exit_horribly(NULL, modulename, "out of memory\n");
	memcpy(topoItems, objs, numObjs * sizeof(DumpableObject *));

	*nOrdering = numObjs;	/* for success return */

	/*
	 * Scan the constraints, and for each item in the array, generate a
	 * count of the number of constraints that say it must be before
	 * something else. The count for the item with dumpId j is
	 * stored in beforeConstraints[j].
	 */
	beforeConstraints = (int *) malloc((maxDumpId + 1) * sizeof(int));
	if (beforeConstraints == NULL)
		exit_horribly(NULL, modulename, "out of memory\n");
	memset(beforeConstraints, 0, (maxDumpId + 1) * sizeof(int));
	for (i = 0; i < numObjs; i++)
	{
		obj = topoItems[i];
		for (j = 0; j < obj->nDeps; j++)
		{
			k = obj->dependencies[j];
			if (k <= 0 || k > maxDumpId)
				exit_horribly(NULL, modulename, "invalid dependency %d\n", k);
			beforeConstraints[k]++;
		}
	}

	/*--------------------
	 * Now scan the topoItems array backwards.	At each step, output the
	 * last item that has no remaining before-constraints, and decrease
	 * the beforeConstraints count of each of the items it was constrained
	 * against.
	 * i = index of ordering[] entry we want to output this time
	 * j = search index for topoItems[]
	 * k = temp for scanning constraint list for item j
	 * last = last non-null index in topoItems (avoid redundant searches)
	 *--------------------
	 */
	last = numObjs - 1;
	for (i = numObjs; --i >= 0;)
	{
		/* Find next candidate to output */
		while (topoItems[last] == NULL)
			last--;
		for (j = last; j >= 0; j--)
		{
			obj = topoItems[j];
			if (obj != NULL && beforeConstraints[obj->dumpId] == 0)
				break;
		}
		/* If no available candidate, topological sort fails */
		if (j < 0)
		{
			result = false;
			break;
		}
		/* Output candidate, and mark it done by zeroing topoItems[] entry */
		ordering[i] = obj = topoItems[j];
		topoItems[j] = NULL;
		/* Update beforeConstraints counts of its predecessors */
		for (k = 0; k < obj->nDeps; k++)
			beforeConstraints[obj->dependencies[k]]--;
	}

	/*
	 * If we failed, report one of the circular constraint sets
	 */
	if (!result)
	{
		for (j = last; j >= 0; j--)
		{
			ordering[0] = obj = topoItems[j];
			if (obj && findLoop(obj, 1, ordering, nOrdering))
				break;
		}
		if (j < 0)
			exit_horribly(NULL, modulename,
						  "could not find dependency loop\n");
	}

	/* Done */
	free(topoItems);
	free(beforeConstraints);

	return result;
}

/*
 * Recursively search for a circular dependency loop
 */
static bool
findLoop(DumpableObject *obj,
		 int depth,
		 DumpableObject **ordering,		/* output argument */
		 int *nOrdering)			/* output argument */
{
	DumpId		startPoint = ordering[0]->dumpId;
	int			j;
	int			k;

	/* See if we've found a loop back to the starting point */
	for (j = 0; j < obj->nDeps; j++)
	{
		if (obj->dependencies[j] == startPoint)
		{
			*nOrdering = depth;
			return true;
		}
	}
	/* Try each outgoing branch */
	for (j = 0; j < obj->nDeps; j++)
	{
		DumpableObject *nextobj = findObjectByDumpId(obj->dependencies[j]);

		if (!nextobj)
			continue;			/* ignore dependencies on undumped objects */
		for (k = 0; k < depth; k++)
		{
			if (ordering[k] == nextobj)
				break;
		}
		if (k < depth)
			continue;			/* ignore loops not including start point */
		ordering[depth] = nextobj;
		if (findLoop(nextobj,
					 depth + 1,
					 ordering,
					 nOrdering))
			return true;
	}

	return false;
}

/*
 * A user-defined datatype will have a dependency loop with each of its
 * I/O functions (since those have the datatype as input or output).
 * We want the dump ordering to be the input function, then any other
 * I/O functions, then the datatype.  So we break the circularity in
 * favor of the functions, and add a dependency from any non-input
 * function to the input function.
 */
static void
repairTypeFuncLoop(DumpableObject *typeobj, DumpableObject *funcobj)
{
	TypeInfo   *typeInfo = (TypeInfo *) typeobj;
	FuncInfo   *inputFuncInfo;

	/* remove function's dependency on type */
	removeObjectDependency(funcobj, typeobj->dumpId);

	/* if this isn't the input function, make it depend on same */
	if (funcobj->catId.oid == typeInfo->typinput)
		return;					/* it is the input function */
	inputFuncInfo = findFuncByOid(typeInfo->typinput);
	if (inputFuncInfo == NULL)
		return;
	addObjectDependency(funcobj, inputFuncInfo->dobj.dumpId);
	/*
	 * Make sure the input function's dependency on type gets removed too;
	 * if it hasn't been done yet, we'd end up with loops involving the
	 * type and two or more functions, which repairDependencyLoop() is not
	 * smart enough to handle.
	 */
	removeObjectDependency(&inputFuncInfo->dobj, typeobj->dumpId);
}

/*
 * Because we force a view to depend on its ON SELECT rule, while there
 * will be an implicit dependency in the other direction, we need to break
 * the loop.  We can always do this by removing the implicit dependency.
 */
static void
repairViewRuleLoop(DumpableObject *viewobj,
				   DumpableObject *ruleobj)
{
	/* remove rule's dependency on view */
	removeObjectDependency(ruleobj, viewobj->dumpId);
}

/*
 * Because we make tables depend on their CHECK constraints, while there
 * will be an automatic dependency in the other direction, we need to break
 * the loop.  If there are no other objects in the loop then we can remove
 * the automatic dependency and leave the CHECK constraint non-separate.
 */
static void
repairTableConstraintLoop(DumpableObject *tableobj,
						  DumpableObject *constraintobj)
{
	/* remove constraint's dependency on table */
	removeObjectDependency(constraintobj, tableobj->dumpId);
}

/*
 * However, if there are other objects in the loop, we must break the loop
 * by making the CHECK constraint a separately-dumped object.
 *
 * Because findLoop() finds shorter cycles before longer ones, it's likely
 * that we will have previously fired repairTableConstraintLoop() and
 * removed the constraint's dependency on the table.  Put it back to ensure
 * the constraint won't be emitted before the table...
 */
static void
repairTableConstraintMultiLoop(DumpableObject *tableobj,
							   DumpableObject *constraintobj)
{
	/* remove table's dependency on constraint */
	removeObjectDependency(tableobj, constraintobj->dumpId);
	/* mark constraint as needing its own dump */
	((ConstraintInfo *) constraintobj)->separate = true;
	/* put back constraint's dependency on table */
	addObjectDependency(constraintobj, tableobj->dumpId);
}

/*
 * Attribute defaults behave exactly the same as CHECK constraints...
 */
static void
repairTableAttrDefLoop(DumpableObject *tableobj,
					   DumpableObject *attrdefobj)
{
	/* remove attrdef's dependency on table */
	removeObjectDependency(attrdefobj, tableobj->dumpId);
}

static void
repairTableAttrDefMultiLoop(DumpableObject *tableobj,
							DumpableObject *attrdefobj)
{
	/* remove table's dependency on attrdef */
	removeObjectDependency(tableobj, attrdefobj->dumpId);
	/* mark attrdef as needing its own dump */
	((AttrDefInfo *) attrdefobj)->separate = true;
	/* put back attrdef's dependency on table */
	addObjectDependency(attrdefobj, tableobj->dumpId);
}

/*
 * CHECK constraints on domains work just like those on tables ...
 */
static void
repairDomainConstraintLoop(DumpableObject *domainobj,
						   DumpableObject *constraintobj)
{
	/* remove constraint's dependency on domain */
	removeObjectDependency(constraintobj, domainobj->dumpId);
}

static void
repairDomainConstraintMultiLoop(DumpableObject *domainobj,
								DumpableObject *constraintobj)
{
	/* remove domain's dependency on constraint */
	removeObjectDependency(domainobj, constraintobj->dumpId);
	/* mark constraint as needing its own dump */
	((ConstraintInfo *) constraintobj)->separate = true;
	/* put back constraint's dependency on domain */
	addObjectDependency(constraintobj, domainobj->dumpId);
}

/*
 * Fix a dependency loop, or die trying ...
 *
 * This routine is mainly concerned with reducing the multiple ways that
 * a loop might appear to common cases, which it passes off to the
 * "fixer" routines above.
 */
static void
repairDependencyLoop(DumpableObject **loop,
					 int nLoop)
{
	int			i,
				j;

	/* Datatype and one of its I/O functions */
	if (nLoop == 2 &&
		loop[0]->objType == DO_TYPE &&
		loop[1]->objType == DO_FUNC)
	{
		repairTypeFuncLoop(loop[0], loop[1]);
		return;
	}
	if (nLoop == 2 &&
		loop[1]->objType == DO_TYPE &&
		loop[0]->objType == DO_FUNC)
	{
		repairTypeFuncLoop(loop[1], loop[0]);
		return;
	}

	/* View and its ON SELECT rule */
	if (nLoop == 2 &&
		loop[0]->objType == DO_TABLE &&
		loop[1]->objType == DO_RULE &&
		((RuleInfo *) loop[1])->ev_type == '1' &&
		((RuleInfo *) loop[1])->is_instead)
	{
		repairViewRuleLoop(loop[0], loop[1]);
		return;
	}
	if (nLoop == 2 &&
		loop[1]->objType == DO_TABLE &&
		loop[0]->objType == DO_RULE &&
		((RuleInfo *) loop[0])->ev_type == '1' &&
		((RuleInfo *) loop[0])->is_instead)
	{
		repairViewRuleLoop(loop[1], loop[0]);
		return;
	}

	/* Table and CHECK constraint */
	if (nLoop == 2 &&
		loop[0]->objType == DO_TABLE &&
		loop[1]->objType == DO_CONSTRAINT &&
		((ConstraintInfo *) loop[1])->contype == 'c' &&
		((ConstraintInfo *) loop[1])->contable == (TableInfo *) loop[0])
	{
		repairTableConstraintLoop(loop[0], loop[1]);
		return;
	}
	if (nLoop == 2 &&
		loop[1]->objType == DO_TABLE &&
		loop[0]->objType == DO_CONSTRAINT &&
		((ConstraintInfo *) loop[0])->contype == 'c' &&
		((ConstraintInfo *) loop[0])->contable == (TableInfo *) loop[1])
	{
		repairTableConstraintLoop(loop[1], loop[0]);
		return;
	}

	/* Indirect loop involving table and CHECK constraint */
	if (nLoop > 2)
	{
		for (i = 0; i < nLoop; i++)
		{
			if (loop[i]->objType == DO_TABLE)
			{
				for (j = 0; j < nLoop; j++)
				{
					if (loop[j]->objType == DO_CONSTRAINT &&
						((ConstraintInfo *) loop[j])->contype == 'c' &&
						((ConstraintInfo *) loop[j])->contable == (TableInfo *) loop[i])
					{
						repairTableConstraintMultiLoop(loop[i], loop[j]);
						return;
					}
				}
			}
		}
	}

	/* Table and attribute default */
	if (nLoop == 2 &&
		loop[0]->objType == DO_TABLE &&
		loop[1]->objType == DO_ATTRDEF &&
		((AttrDefInfo *) loop[1])->adtable == (TableInfo *) loop[0])
	{
		repairTableAttrDefLoop(loop[0], loop[1]);
		return;
	}
	if (nLoop == 2 &&
		loop[1]->objType == DO_TABLE &&
		loop[0]->objType == DO_ATTRDEF &&
		((AttrDefInfo *) loop[0])->adtable == (TableInfo *) loop[1])
	{
		repairTableAttrDefLoop(loop[1], loop[0]);
		return;
	}

	/* Indirect loop involving table and attribute default */
	if (nLoop > 2)
	{
		for (i = 0; i < nLoop; i++)
		{
			if (loop[i]->objType == DO_TABLE)
			{
				for (j = 0; j < nLoop; j++)
				{
					if (loop[j]->objType == DO_ATTRDEF &&
						((AttrDefInfo *) loop[j])->adtable == (TableInfo *) loop[i])
					{
						repairTableAttrDefMultiLoop(loop[i], loop[j]);
						return;
					}
				}
			}
		}
	}

	/* Domain and CHECK constraint */
	if (nLoop == 2 &&
		loop[0]->objType == DO_TYPE &&
		loop[1]->objType == DO_CONSTRAINT &&
		((ConstraintInfo *) loop[1])->contype == 'c' &&
		((ConstraintInfo *) loop[1])->condomain == (TypeInfo *) loop[0])
	{
		repairDomainConstraintLoop(loop[0], loop[1]);
		return;
	}
	if (nLoop == 2 &&
		loop[1]->objType == DO_TYPE &&
		loop[0]->objType == DO_CONSTRAINT &&
		((ConstraintInfo *) loop[0])->contype == 'c' &&
		((ConstraintInfo *) loop[0])->condomain == (TypeInfo *) loop[1])
	{
		repairDomainConstraintLoop(loop[1], loop[0]);
		return;
	}

	/* Indirect loop involving domain and CHECK constraint */
	if (nLoop > 2)
	{
		for (i = 0; i < nLoop; i++)
		{
			if (loop[i]->objType == DO_TYPE)
			{
				for (j = 0; j < nLoop; j++)
				{
					if (loop[j]->objType == DO_CONSTRAINT &&
						((ConstraintInfo *) loop[j])->contype == 'c' &&
						((ConstraintInfo *) loop[j])->condomain == (TypeInfo *) loop[i])
					{
						repairDomainConstraintMultiLoop(loop[i], loop[j]);
						return;
					}
				}
			}
		}
	}

	/*
	 * If we can't find a principled way to break the loop, complain and
	 * break it in an arbitrary fashion.
	 */
	write_msg(modulename, "WARNING: could not resolve dependency loop among these items:\n");
	for (i = 0; i < nLoop; i++)
	{
		char	buf[1024];

		describeDumpableObject(loop[i], buf, sizeof(buf));
		write_msg(modulename, "  %s\n", buf);
	}
	removeObjectDependency(loop[0], loop[1]->dumpId);
}

/*
 * Describe a dumpable object usefully for errors
 *
 * This should probably go somewhere else...
 */
static void
describeDumpableObject(DumpableObject *obj, char *buf, int bufsize)
{
	switch (obj->objType)
	{
		case DO_NAMESPACE:
			snprintf(buf, bufsize,
					 "SCHEMA %s  (ID %d OID %u)",
					 ((NamespaceInfo *) obj)->nspname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_TYPE:
			snprintf(buf, bufsize,
					 "TYPE %s  (ID %d OID %u)",
					 ((TypeInfo *) obj)->typname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_FUNC:
			snprintf(buf, bufsize,
					 "FUNCTION %s  (ID %d OID %u)",
					 ((FuncInfo *) obj)->proname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_AGG:
			snprintf(buf, bufsize,
					 "AGGREGATE %s  (ID %d OID %u)",
					 ((AggInfo *) obj)->aggfn.proname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_OPERATOR:
			snprintf(buf, bufsize,
					 "OPERATOR %s  (ID %d OID %u)",
					 ((OprInfo *) obj)->oprname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_OPCLASS:
			snprintf(buf, bufsize,
					 "OPERATOR CLASS %s  (ID %d OID %u)",
					 ((OpclassInfo *) obj)->opcname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_CONVERSION:
			snprintf(buf, bufsize,
					 "CONVERSION %s  (ID %d OID %u)",
					 ((ConvInfo *) obj)->conname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_TABLE:
			snprintf(buf, bufsize,
					 "TABLE %s  (ID %d OID %u)",
					 ((TableInfo *) obj)->relname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_ATTRDEF:
			snprintf(buf, bufsize,
					 "ATTRDEF %s.%s  (ID %d OID %u)",
					 ((AttrDefInfo *) obj)->adtable->relname,
					 ((AttrDefInfo *) obj)->adtable->attnames[((AttrDefInfo *) obj)->adnum - 1],
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_INDEX:
			snprintf(buf, bufsize,
					 "INDEX %s  (ID %d OID %u)",
					 ((IndxInfo *) obj)->indexname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_RULE:
			snprintf(buf, bufsize,
					 "RULE %s  (ID %d OID %u)",
					 ((RuleInfo *) obj)->rulename,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_TRIGGER:
			snprintf(buf, bufsize,
					 "TRIGGER %s  (ID %d OID %u)",
					 ((TriggerInfo *) obj)->tgname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_CONSTRAINT:
			snprintf(buf, bufsize,
					 "CONSTRAINT %s  (ID %d OID %u)",
					 ((ConstraintInfo *) obj)->conname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_FK_CONSTRAINT:
			snprintf(buf, bufsize,
					 "FK CONSTRAINT %s  (ID %d OID %u)",
					 ((ConstraintInfo *) obj)->conname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_PROCLANG:
			snprintf(buf, bufsize,
					 "PROCEDURAL LANGUAGE %s  (ID %d OID %u)",
					 ((ProcLangInfo *) obj)->lanname,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_CAST:
			snprintf(buf, bufsize,
					 "CAST %u to %u  (ID %d OID %u)",
					 ((CastInfo *) obj)->castsource,
					 ((CastInfo *) obj)->casttarget,
					 obj->dumpId, obj->catId.oid);
			return;
		case DO_TABLE_DATA:
			snprintf(buf, bufsize,
					 "TABLE DATA %s  (ID %d OID %u)",
					 ((TableDataInfo *) obj)->tdtable->relname,
					 obj->dumpId, obj->catId.oid);
			return;
	}
	/* shouldn't get here */
	snprintf(buf, bufsize,
			 "object type %d  (ID %d OID %u)",
			 (int) obj->objType,
			 obj->dumpId, obj->catId.oid);
}
