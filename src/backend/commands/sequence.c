/*-------------------------------------------------------------------------
 *
 * sequence.c--
 *    PostgreSQL sequences support code.
 *
 *-------------------------------------------------------------------------
 */
#include <stdio.h>
#include <string.h>

#include <postgres.h>

#include <storage/bufmgr.h>
#include <storage/bufpage.h>
#include <storage/lmgr.h>
#include <access/heapam.h>
#include <nodes/parsenodes.h>
#include <commands/creatinh.h>
#include <commands/sequence.h>
#include <utils/builtins.h>

#define SEQ_MAGIC     0x1717

#define SEQ_MAXVALUE	((int4)0x7FFFFFFF)
#define SEQ_MINVALUE	-(SEQ_MAXVALUE)

bool ItsSequenceCreation = false;

typedef struct FormData_pg_sequence {
	NameData	sequence_name;
	int4		last_value;
	int4		increment_by;
	int4		max_value;
	int4		min_value;
	int4		cache_value;
	char		is_cycled;
	char		is_called;
} FormData_pg_sequence;

typedef FormData_pg_sequence   *SequenceTupleForm;

typedef struct sequence_magic {
	uint32 magic;
} sequence_magic;

typedef struct SeqTableData {
	char			*name;
	Oid			relid;
	Relation		rel;
	int4			cached;
	int4			last;
	int4			increment;
	struct SeqTableData	*next;
} SeqTableData;

typedef SeqTableData *SeqTable;

static SeqTable seqtab = NULL;

static SeqTable init_sequence (char *caller, char *name);
static SequenceTupleForm read_info (char * caller, SeqTable elm, Buffer * buf);
static void init_params (CreateSeqStmt *seq, SequenceTupleForm new);
static int get_param (DefElem *def);

/*
 * DefineSequence --
 *		Creates a new sequence relation
 */
void
DefineSequence (CreateSeqStmt *seq)
{
    FormData_pg_sequence new;
    CreateStmt *stmt = makeNode (CreateStmt);
    ColumnDef  *coldef;
    TypeName   *typnam;
    Relation rel;
    Buffer buf;
    PageHeader page;
    sequence_magic *sm;
    HeapTuple tuple;
    TupleDesc tupDesc;
    Datum value[SEQ_COL_LASTCOL];
    char null[SEQ_COL_LASTCOL];
    int i;

    /* Check and set values */
    init_params (seq, &new);

    /*
     * Create relation (and fill null[] & value[])
     */
    stmt->tableElts = NIL;
    for (i = SEQ_COL_FIRSTCOL; i <= SEQ_COL_LASTCOL; i++)
    {
    	typnam = makeNode(TypeName);
    	typnam->setof = FALSE;
    	typnam->arrayBounds = NULL;
    	coldef = makeNode(ColumnDef);
    	coldef->typename = typnam;
    	null[i-1] = ' ';

	switch (i)
	{
	    case SEQ_COL_NAME:
    		typnam->name = "name";
    		coldef->colname = "sequence_name";
	    	value[i-1] = PointerGetDatum (seq->seqname);
    		break;
	    case SEQ_COL_LASTVAL:
    		typnam->name = "int4";
    		coldef->colname = "last_value";
	    	value[i-1] = Int32GetDatum (new.last_value);
    		break;
	    case SEQ_COL_INCBY:
    		typnam->name = "int4";
    		coldef->colname = "increment_by";
	    	value[i-1] = Int32GetDatum (new.increment_by);
    		break;
	    case SEQ_COL_MAXVALUE:
    		typnam->name = "int4";
    		coldef->colname = "max_value";
	    	value[i-1] = Int32GetDatum (new.max_value);
    		break;
	    case SEQ_COL_MINVALUE:
    		typnam->name = "int4";
    		coldef->colname = "min_value";
	    	value[i-1] = Int32GetDatum (new.min_value);
    		break;
	    case SEQ_COL_CACHE:
    		typnam->name = "int4";
    		coldef->colname = "cache_value";
	    	value[i-1] = Int32GetDatum (new.cache_value);
    		break;
	    case SEQ_COL_CYCLE:
    		typnam->name = "char";
    		coldef->colname = "is_cycled";
	    	value[i-1] = CharGetDatum (new.is_cycled);
    		break;
	    case SEQ_COL_CALLED:
    		typnam->name = "char";
    		coldef->colname = "is_called";
	    	value[i-1] = CharGetDatum ('f');
    		break;
    	}
    	stmt->tableElts = lappend (stmt->tableElts, coldef);
    }
    
    stmt->relname = seq->seqname;
    stmt->archiveLoc = -1;		/* default */
    stmt->archiveType = ARCH_NONE;
    stmt->inhRelnames = NIL;
    
    ItsSequenceCreation = true;		/* hack */

    DefineRelation (stmt);

    /* Xact abort calls CloseSequences, which turns ItsSequenceCreation off */
    ItsSequenceCreation = false;	/* hack */

    rel = heap_openr (seq->seqname);
    Assert ( RelationIsValid (rel) );

    RelationSetLockForWrite (rel);

    tupDesc = RelationGetTupleDescriptor(rel);
    
    Assert ( RelationGetNumberOfBlocks (rel) == 0 );
    buf = ReadBuffer (rel, P_NEW);

    if ( !BufferIsValid (buf) )
    	elog (WARN, "DefineSequence: ReadBuffer failed");

    page = (PageHeader) BufferGetPage (buf);

    PageInit((Page)page, BufferGetPageSize(buf), sizeof(sequence_magic));
    sm = (sequence_magic *) PageGetSpecialPointer (page);
    sm->magic = SEQ_MAGIC;

    /* Now - form & insert sequence tuple */
    tuple = heap_formtuple (tupDesc, value, null);
    heap_insert (rel, tuple);

    if ( WriteBuffer (buf) == STATUS_ERROR )
    	elog (WARN, "DefineSequence: WriteBuffer failed");

    RelationUnsetLockForWrite (rel);
    heap_close (rel);
    
    return;

}


int4
nextval (struct varlena * seqin)
{
    char *seqname = textout(seqin);
    SeqTable elm;
    Buffer buf;
    SequenceTupleForm seq;
    ItemPointerData iptr;
    int4 incby, maxv, minv, cache;
    int4 result, next, rescnt = 0;

    /* open and WIntentLock sequence */
    elm = init_sequence ("nextval", seqname);
    pfree (seqname);
    
    if ( elm->last != elm->cached )		/* some numbers were cached */
    {
    	elm->last += elm->increment;
    	return (elm->last);
    }

    seq = read_info ("nextval", elm, &buf);	/* lock page and read tuple */
    
    next = result = seq->last_value;
    incby = seq->increment_by;
    maxv = seq->max_value;
    minv = seq->min_value;
    cache = seq->cache_value;

    if ( seq->is_called != 't' )
    	rescnt++;			/* last_value if not called */

    while ( rescnt < cache )		/* try to fetch cache numbers */
    {
    	/* 
    	 * Check MAXVALUE for ascending sequences
    	 * and MINVALUE for descending sequences
    	 */
    	if ( incby > 0 )		/* ascending sequence */
    	{
    	    if ( ( maxv >= 0 && next > maxv - incby)  ||
    			( maxv < 0 && next + incby > maxv ) )
    	    {
    	    	if ( rescnt > 0 )
    	    	    break;		/* stop caching */
    	    	if ( seq->is_cycled != 't' )
    	    	    elog (WARN, "%s.nextval: got MAXVALUE (%d)", 
    						elm->name, maxv);
	    	next = minv;
	    }
	    else
    		next += incby;
	}
	else				/* descending sequence */
	{
	    if ( ( minv < 0 && next < minv - incby ) ||
	    		( minv >= 0 && next + incby < minv ) )
    	    {
    	    	if ( rescnt > 0 )
    	    	    break;		/* stop caching */
    	    	if ( seq->is_cycled != 't' )
    	    	    elog (WARN, "%s.nextval: got MINVALUE (%d)", 
    						elm->name, minv);
	    	next = maxv;
	    }
	    else
    		next += incby;
    	}
    	rescnt++;			/* got result */
    	if ( rescnt == 1 )		/* if it's first one - */
    	    result = next;		/* it's what to return */
    }

    /* save info in local cache */
    elm->last = result;			/* last returned number */
    elm->cached = next;			/* last cached number */

    /* save info in sequence relation */
    seq->last_value = next;		/* last fetched number */
    seq->is_called = 't';
    
    if ( WriteBuffer (buf) == STATUS_ERROR )
    	elog (WARN, "%s.nextval: WriteBuffer failed", elm->name);

    ItemPointerSet(&iptr, 0, FirstOffsetNumber);
    RelationUnsetSingleWLockPage (elm->rel, &iptr);

    return (result);
    
}


int4
currval (struct varlena * seqin)
{
    char *seqname = textout(seqin);
    SeqTable elm;
    int4 result;

    /* open and WIntentLock sequence */
    elm = init_sequence ("currval", seqname);
    pfree (seqname);
    
    if ( elm->increment == 0 )	/* nextval/read_info were not called */
    {
    	elog (WARN, "%s.currval is not yet defined in this session", elm->name);
    }
    
    result = elm->last;
    
    return (result);
    
}

static SequenceTupleForm
read_info (char * caller, SeqTable elm, Buffer * buf)
{
    ItemPointerData iptr;
    PageHeader page;
    ItemId lp;
    HeapTuple tuple;
    sequence_magic *sm;
    SequenceTupleForm seq;

    ItemPointerSet(&iptr, 0, FirstOffsetNumber);
    RelationSetSingleWLockPage (elm->rel, &iptr);

    if ( RelationGetNumberOfBlocks (elm->rel) != 1 )
    	elog (WARN, "%s.%s: invalid number of blocks in sequence", 
    			elm->name, caller);
    
    *buf = ReadBuffer (elm->rel, 0);
    if ( !BufferIsValid (*buf) )
    	elog (WARN, "%s.%s: ReadBuffer failed", elm->name, caller);

    page = (PageHeader) BufferGetPage (*buf);
    sm = (sequence_magic *) PageGetSpecialPointer (page);
    
    if ( sm->magic != SEQ_MAGIC )
    	elog (WARN, "%s.%s: bad magic (%08X)", elm->name, caller, sm->magic);

    lp = PageGetItemId (page, FirstOffsetNumber);
    Assert (ItemIdIsUsed (lp));
    tuple = (HeapTuple) PageGetItem ((Page) page, lp);
    
    seq = (SequenceTupleForm) GETSTRUCT(tuple);
    
    elm->increment = seq->increment_by;

    return (seq);

}


static SeqTable
init_sequence (char * caller, char * name)
{
    SeqTable elm, priv = (SeqTable) NULL;
    SeqTable temp;
        
    for (elm = seqtab; elm != (SeqTable) NULL; )
    {
    	if ( strcmp (elm->name, name) == 0 )
    	    break;
    	priv = elm;
    	elm = elm->next;
    }
    
    if ( elm == (SeqTable) NULL )			/* not found */
    {
    	temp = (SeqTable) malloc (sizeof(SeqTableData));
    	temp->name = malloc (strlen(name) + 1);
    	strcpy (temp->name, name);
    	temp->rel = (Relation) NULL;
    	temp->cached = temp->last = temp->increment = 0;
    	temp->next = (SeqTable) NULL;
    }
    else						/* found */
    {
    	if ( elm->rel != (Relation) NULL)		/* already opened */
    	    return (elm);
    	temp = elm;
    }
    	
    temp->rel = heap_openr (name);

    if ( ! RelationIsValid (temp->rel) )
    	elog (WARN, "%s.%s: sequence does not exist", name, caller);

    RelationSetWIntentLock (temp->rel);
    
    if ( temp->rel->rd_rel->relkind != RELKIND_SEQUENCE )
    	elog (WARN, "%s.%s: %s is not sequence !", name, caller, name);

    if ( elm != (SeqTable) NULL )	/* we opened sequence from our */
    {					/* SeqTable - check relid ! */
    	if ( RelationGetRelationId (elm->rel) != elm->relid )
    	{
    	    elog (NOTICE, "%s.%s: sequence was re-created",
    	    	name, caller, name);
    	    elm->cached = elm->last = elm->increment = 0;
    	    elm->relid = RelationGetRelationId (elm->rel);
    	}
    }
    else
    {
    	elm = temp;
    	elm->relid = RelationGetRelationId (elm->rel);
    	if ( seqtab == (SeqTable) NULL )
    	    seqtab = elm;
    	else
    	    priv->next = elm;
    }
    	
    return (elm);
    
}


/*
 * CloseSequences --
 *		is calling by xact mgr at commit/abort.
 */
void
CloseSequences (void)
{
    SeqTable elm;
    Relation rel;
        
    ItsSequenceCreation = false;
    
    for (elm = seqtab; elm != (SeqTable) NULL; )
    {
	if ( elm->rel != (Relation) NULL )	/* opened in current xact */
	{
	    rel = elm->rel;
    	    elm->rel = (Relation) NULL;
    	    RelationUnsetWIntentLock (rel);
    	    heap_close (rel);
    	}
    	elm = elm->next;
    }

    return;
    
}


static void 
init_params (CreateSeqStmt *seq, SequenceTupleForm new)
{
    DefElem *last_value = NULL;
    DefElem *increment_by = NULL;
    DefElem *max_value = NULL;
    DefElem *min_value = NULL;
    DefElem *cache_value = NULL;
    List *option;
    
    new->is_cycled = 'f';
    foreach (option, seq->options)
    {
    	DefElem *defel = (DefElem *)lfirst(option);
    	
    	if ( !strcasecmp(defel->defname, "increment") )
    	    increment_by = defel;
    	else if ( !strcasecmp(defel->defname, "start") )
    	    last_value = defel;
    	else if ( !strcasecmp(defel->defname, "maxvalue") )
    	    max_value = defel;
    	else if ( !strcasecmp(defel->defname, "minvalue") )
    	    min_value = defel;
    	else if ( !strcasecmp(defel->defname, "cache") )
    	    cache_value = defel;
    	else if ( !strcasecmp(defel->defname, "cycle") )
    	{
    	    if ( defel->arg != (Node*) NULL )
    	    	elog (WARN, "DefineSequence: CYCLE ??");
    	    new->is_cycled = 't';
    	}
    	else
    	    elog (WARN, "DefineSequence: option \"%s\" not recognized",
    	    			defel->defname);
    }

    if ( increment_by == (DefElem*) NULL )	/* INCREMENT BY */
    	new->increment_by = 1;
    else if ( ( new->increment_by = get_param (increment_by) ) == 0 )
    	elog (WARN, "DefineSequence: can't INCREMENT by 0");

    if ( max_value == (DefElem*) NULL )		/* MAXVALUE */
    	if ( new->increment_by > 0 )
	    new->max_value = SEQ_MAXVALUE;	/* ascending seq */
	else
	    new->max_value = -1;			/* descending seq */
    else
    	new->max_value = get_param (max_value);

    if ( min_value == (DefElem*) NULL )		/* MINVALUE */
    	if ( new->increment_by > 0 )
	    new->min_value = 1;			/* ascending seq */
	else
	    new->min_value = SEQ_MINVALUE;	/* descending seq */
    else
    	new->min_value = get_param (min_value);
    
    if ( new->min_value >= new->max_value )
    	elog (WARN, "DefineSequence: MINVALUE (%d) can't be >= MAXVALUE (%d)", 
    		new->min_value, new->max_value);

    if ( last_value == (DefElem*) NULL )	/* START WITH */
	if ( new->increment_by > 0 )
	    new->last_value = new->min_value;	/* ascending seq */
	else
	    new->last_value = new->max_value;	/* descending seq */
    else
    	new->last_value = get_param (last_value);

    if ( new->last_value < new->min_value )
    	elog (WARN, "DefineSequence: START value (%d) can't be < MINVALUE (%d)", 
    		new->last_value, new->min_value);
    if ( new->last_value > new->max_value )
    	elog (WARN, "DefineSequence: START value (%d) can't be > MAXVALUE (%d)", 
    		new->last_value, new->max_value);

    if ( cache_value == (DefElem*) NULL )	/* CACHE */
    	new->cache_value = 1;
    else if ( ( new->cache_value = get_param (cache_value) ) <= 0 )
    	elog (WARN, "DefineSequence: CACHE (%d) can't be <= 0", 
    			new->cache_value);

}


static int
get_param (DefElem *def)
{
    if ( def->arg == (Node*) NULL )
    	elog (WARN, "DefineSequence: \"%s\" value unspecified", def->defname);

    if ( nodeTag (def->arg) == T_Integer )
    	return (intVal (def->arg));
    
    elog (WARN, "DefineSequence: \"%s\" is to be integer", def->defname);
    return (-1);
}
