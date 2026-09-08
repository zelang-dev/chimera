/*
 * table.c
 *
 * libhtml - HTML->X renderer
 *
 * Copyright (c) 1995-1997, John Kilburg <john@cs.unlv.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "port_before.h"

#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "port_after.h"

#include "html.h"

typedef struct
{
  HTMLBox box;
  unsigned int width;
  int rowspan, colspan;
} HData;

typedef struct
{
  bool original;
  int rowspan, colspan;
  HData *ds;
  int rowspan2, colspan2;
  HData *ds2;
} HCell;

typedef struct
{
  GList datas;
  int colcount;
  HCell *ca;
} HRow;

typedef struct
{
  MemPool mp;
  unsigned int border;                  /* table border width */
  HTMLBox box;                          /* table box */
  HTMLBox dbox;                         /* current data box */
  MLElement p;                          /* table tag element */

  GList grid;                           /* grid of rows and data */

  GList klist;                          /* list of kids */

  int rowcount, colcount;               /* nearly always greater than real */

  int pass;

  unsigned int *cwidth, *rheight;       /* column widths, row heights */
  unsigned int *swidth;                 /* scaled columns widths */

  unsigned int max_width;               /* width available for table */
  unsigned int inf_width;               /* width with unbounded table size */
} HTable;

/*
 * Private functions
 */
static void DestroyTable _ArgProto((HTMLInfo, HTMLBox));
static void SetupTable _ArgProto((HTMLInfo, HTMLBox));
static void RenderTable _ArgProto((HTMLInfo, HTMLBox, Region));
static void TablePosition1 _ArgProto((HTable *));
static void TablePosition2 _ArgProto((HTable *));
static void FillSpans _ArgProto((HTable *));

/*
 * SetupTable
 *
 * Called when the position of the table is known.  Now set the location
 * of the child boxes and arrange to have their setup function called.
 */
static void
SetupTable(li, box)
HTMLInfo li;
HTMLBox box;
{
  int i, j;
  HRow *rs;
  HData *ds;
  int bx, by;
  HTable *ts = (HTable *)box->closure;

  bx = 0;
  by = 0;

  for (j = 0, rs = (HRow *)GListGetHead(ts->grid); rs != NULL;
       rs = (HRow *)GListGetNext(ts->grid), j++)
  {
    if (rs->ca == NULL) continue;

    for (i = 0; i < ts->colcount; i++)
    {
      if (rs->ca[i].ds != NULL && rs->ca[i].original)
      {
	ds = rs->ca[i].ds;
	ds->box->x = bx + box->x;
	ds->box->y = by + box->y;
	HTMLSetupBox(li, ds->box);

	bx += ts->cwidth[i];
      }
      else
      {
	bx += ts->cwidth[i];
      }
    }
    bx = 0;
    by += ts->rheight[j];
  }

  return;
}

/*
 * RenderTable
 *
 * Called when the table has been exposed.  Since the child boxes
 * will handle their own refresh only need to refresh the border if
 * there is one.
 */
static void
RenderTable(li, box, r)
HTMLInfo li;
HTMLBox box;
Region r;
{
  HTable *ts = (HTable *)box->closure;
  HTMLBox c;

  for (c = (HTMLBox)GListGetHead(ts->klist); c != NULL;
       c = (HTMLBox)GListGetNext(ts->klist))
  {
    HTMLRenderBox(li, r, c);
  }

  return;
}

/*
 * DestroyTable
 */
static void
DestroyTable(li, box)
HTMLInfo li;
HTMLBox box;
{
  HTable *ts = (HTable *)box->closure;
  HTMLBox c;

  while ((c = (HTMLBox)GListPop(ts->klist)) != NULL)
  {
    HTMLDestroyBox(li, c);
  }

  return;
}


/*
 * HTMLTableAddBox
 */
void
HTMLTableAddBox(li, env, box)
HTMLInfo li;
HTMLEnv env;
HTMLBox box;
{
  return;
}

/*
 * HTMLTDAddBox
 */
void
HTMLTDAddBox(li, env, box)
HTMLInfo li;
HTMLEnv env;
HTMLBox box;
{
  HTable *ts = (HTable *)env->closure;
  HTMLLayoutBox(li, ts->dbox, box);
  return;
}

/*
 * HTMLTDBegin
 *
 * Called when a <td> tag is encountered
 */
void
HTMLTDBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTable *ts;
  HRow *rs;
  HData *ds;

  ts = (HTable *)env->penv->closure;
  env->closure = ts;

  if (ts->pass == 0)
  {
    rs = (HRow *)GListGetTail(ts->grid);
    myassert(rs != NULL, "Could not find row data (1).");

    ds = (HData *)MPCGet(ts->mp, sizeof(HData));
    if (p == NULL)
    {
      ds->colspan = 1;
      ds->rowspan = 1;
    }
    else
    {
      if ((ds->colspan = MLAttributeToInt(p, "colspan")) < 1) ds->colspan = 1;
      if ((ds->rowspan = MLAttributeToInt(p, "rowspan")) < 1) ds->rowspan = 1;
    }

    GListAddTail(rs->datas, ds);

    rs->colcount += ds->colspan;
    if (rs->colcount > ts->colcount) ts->colcount = rs->colcount;
    /* this is more than the real count but that's ok. */
    ts->rowcount += ds->rowspan;

    ts->dbox = ds->box = HTMLCreateFlowBox(li, env, li->tableCellInfinity);
  }
  else
  {
    rs = (HRow *)GListGetCurrent(ts->grid);
    myassert(rs != NULL, "Could not find row data (2).");

    ds = (HData *)GListGetCurrent(rs->datas);
    myassert(ds != NULL, "Could not find cell data (2).");

    ts->dbox = ds->box = HTMLCreateFlowBox(li, env, ds->width);
  }

  GListAddHead(ts->klist, ds->box);

  return;
}

/*
 * HTMLTDEnd
 *
 * Called when a <td> tag is encountered
 */
void
HTMLTDEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTable *ts = (HTable *)env->closure;
  HRow *rs;

  HTMLFinishFlowBox(li, ts->dbox);
  if (ts->pass > 0)
  {
    rs = (HRow *)GListGetCurrent(ts->grid);
    GListGetNext(rs->datas);
  }
  return;
}

/*
 * HTMLTRBegin
 */
void
HTMLTRBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTable *ts;
  HRow *rs;

  ts = (HTable *)env->penv->closure;
  env->closure = ts;

  if (ts->pass == 0)
  {
    rs = (HRow *)MPCGet(ts->mp, sizeof(HRow));
    GListAddTail(ts->grid, rs);
    rs->datas = GListCreateX(ts->mp);
  }
  else
  {
    rs = (HRow *)GListGetCurrent(ts->grid);
    GListGetHead(rs->datas);
  }

  return;
}

/*
 * HTMLTREnd
 */
void
HTMLTREnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTable *ts;

  ts = (HTable *)env->penv->closure;
  env->closure = ts;
  if (ts->pass > 0) GListGetNext(ts->grid);

  return;
}

/*
 * HTMLTableBegin
 *
 * Called when <table> is encountered.  Has to deal with two passes.
 */
void
HTMLTableBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLBox box;
  HTable *ts;
  char *value, *cp;
  int x;
  int border;
  unsigned int width, maxwidth;

  maxwidth = width = HTMLGetMaxWidth(li, env->penv);

  env->ff = FLOW_LEFT_JUSTIFY;
  env->anchor = NULL;

  if (env->closure == NULL)
  {
    ts = (HTable *)MPCGet(li->mp, sizeof(HTable));
    ts->mp = li->mp;
    ts->p = p;
    env->closure = ts;

    if ((border = MLAttributeToInt(p, "border")) < 0) border = 0;
    ts->border = border;
    ts->grid = GListCreateX(ts->mp);
    ts->klist = GListCreateX(li->mp);

    if ((value = MLFindAttribute(p, "width")) != NULL)
    {
      if (isdigit(value[0]))
      {
	x = atoi(value);
	for (cp = value; ; cp++)
	{
	  if (!isdigit(*cp)) break;
	}
	if (*cp != '\0')
	{
	  if (*cp == '%')
	  {
	    if (x >= 10) width = width * x / 100;
	  }
	  else if (x >= 100) width = x;
	}
	else if (x >= 100) width = x;
      }
    }
    if (width > 0) ts->max_width = width;  
    else ts->max_width = 0;
  }
  else
  {
    ts = (HTable *)env->closure;
    GListGetHead(ts->grid);
    GListClear(ts->klist);
  }
  ts->pass = env->pass;

  /*
   * Create the table box
   */
  ts->box = box = HTMLCreateBox(li, env);
  box->setup = SetupTable;
  box->render = RenderTable;
  box->destroy = DestroyTable;
  box->closure = ts;

  return;
}

/*
 * HTMLTableEnd
 *
 * Called when </table> is encountered.  Has to deal with two passes.
 */
void
HTMLTableEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTable *ts = (HTable *)env->closure;

  if (ts->colcount == 0) return;

  if (ts->pass == 0)
  {
    /*
     * Do first pass size calculations.
     */
    FillSpans(ts);
    TablePosition1(ts);

    /*
     * Now destroy the table box and its children since we need to rebuild
     * from scratch.
     */
    HTMLDestroyBox(li, ts->box);
    ts->box = NULL;
  }
  else
  {
    TablePosition2(ts); 

    HTMLEnvAddBox(li, env->penv, ts->box);
  }

  return;
}

/*
 * FillSpans 
 */
static void
FillSpans(ts)
HTable *ts;
{
  HRow *rs;
  HData *ds;
  int i, j, k;
  HCell *pca;

  ts->colcount *= 2;

  /*
   * Allocate an array for each row to keep track of the data grid.
   */
  for (k = 0, rs = (HRow *)GListGetHead(ts->grid); rs != NULL;
       rs = (HRow *)GListGetNext(ts->grid), k++)
  {
    rs->ca = (HCell *)MPCGet(ts->mp, sizeof(HCell) * ts->colcount);
  }
  /*
   * Its possible rowspan has pushed things down a bit so add rows as
   * necessary.  Overkill is OK.
   */
  for (; k < ts->rowcount; k++)
  {
    rs = (HRow *)MPCGet(ts->mp, sizeof(HRow));
    GListAddTail(ts->grid, rs);
    rs->datas = GListCreateX(ts->mp);
    rs->ca = (HCell *)MPCGet(ts->mp, sizeof(HCell) * ts->colcount);
  }

  /*
   * Put the data into the grids according to rowspan and colspan.
   */
  pca = NULL;
  for (rs = (HRow *)GListGetHead(ts->grid); rs != NULL;
       rs = (HRow *)GListGetNext(ts->grid))
  {
    /*
     * If there is a previous row then look at the HCell's above and
     * see if they need to be extended.  We know the HCell above needs to
     * be extended down if rowspan > 1.  Move rowspan - 1 down to the
     * current row.  Also, move the colspan
     * value down (its not used, though) and the HData pointer.
     */
    if (pca != NULL)
    {
      for (i = 0; i < ts->colcount; i++)
      {
	if (pca[i].ds != NULL && pca[i].rowspan > 1)
	{
	  myassert(rs->ca[i].ds == NULL, "Cell visitation unexpected!");
	  rs->ca[i].rowspan = pca[i].rowspan - 1;
	  rs->ca[i].colspan = pca[i].colspan;
	  rs->ca[i].ds = pca[i].ds;
	}
	if (pca[i].ds2 != NULL && pca[i].rowspan2 > 1)
	{
	  myassert(rs->ca[i].ds == NULL, "Cell visitation unexpected! (2)");
	  rs->ca[i].rowspan = pca[i].rowspan2 - 1;
	  rs->ca[i].colspan = pca[i].colspan2;
	  rs->ca[i].ds = pca[i].ds2;
	}
      }
    }
    
    /*
     * Now extend HData to the right depending on the colspan.
     */
    for (ds = (HData *)GListGetHead(rs->datas); ds != NULL;
	 ds = (HData *)GListGetNext(rs->datas))
    {
      /*
       * Scan to the right and look for the first empty HCell.  This needs
       * to be done because an HData above could have spanned down to fill
       * in the beginning of the row.
       *
       * This has to be done for each data section because a row that
       * was extended down may have used up space in the middle of the
       * row.
       */
      for (i = 0; i < ts->colcount; i++)
      {
        if (rs->ca[i].ds == NULL) break;
      }
      myassert(ts->colcount != i, "Table column count confused.");
      
      rs->ca[i].original = true;
      for (j = 0; j < ds->colspan; j++, i++)
      {
	if (rs->ca[i].ds == NULL)
	{
	  rs->ca[i].ds = ds;
	  rs->ca[i].colspan = ds->colspan - j;
	  rs->ca[i].rowspan = ds->rowspan;
	}
	else
	{
	  rs->ca[i].ds2 = ds;
	  rs->ca[i].colspan2 = ds->colspan - j;
	  rs->ca[i].rowspan2 = ds->rowspan;
	}
      }
    }

    pca = rs->ca;
  }

/*
 * Test code.  Print out patterns to see if the internal representation
 * looks reasonable.
 */
/*
  for (rs = (HRow *)GListGetHead(ts->grid); rs != NULL;
       rs = (HRow *)GListGetNext(ts->grid))
  {
    for (i = 0; i < ts->colcount; i++)
    {
      if (rs->ca[i].ds2 != NULL) printf ("o");
      else if (rs->ca[i].ds != NULL && rs->ca[i].original)
      {
	printf ("%c", (char )((int)('A') + i));
      }
      else if (rs->ca[i].ds != NULL) printf ("%c", (char )((int)('a') + i));
      else printf ("x");
    }
    printf("\n");
  }
*/

  return;
}

/*
 * TablePosition1
 */
static void
TablePosition1(ts)
HTable *ts;
{
  int i, j;
  HRow *rs;
  HData *ds;
  unsigned int twidth, width;

  /*
   * Allocate arrays to hold the column widths and row heights.
   *
   * cwidth  = column width
   * swidth  = scaled column width
   * rheight = row height
   */
  ts->cwidth = (unsigned int *)MPCGet(ts->mp, sizeof(unsigned int) *
				      ts->colcount);
  ts->swidth = (unsigned int *)MPCGet(ts->mp, sizeof(unsigned int) *
				      ts->colcount);
  ts->rheight = (unsigned int *)MPCGet(ts->mp, sizeof(unsigned int) *
				       ts->rowcount);

  /*
   * Figure out the unrestrained widths of the columns.  Needed a little
   * later to determine the width of the table.
   */
  twidth = 0;
  for (rs = (HRow *)GListGetHead(ts->grid); rs != NULL;
       rs = (HRow *)GListGetNext(ts->grid))
  {
    for (i = 0; i < ts->colcount; i++)
    {
      if (rs->ca[i].ds != NULL)
      {
	ds = rs->ca[i].ds;
	width = ds->box->width / ds->colspan;
	if (width > ts->cwidth[i]) ts->cwidth[i] = width;
      }
      if (rs->ca[i].ds2 != NULL)
      {
	ds = rs->ca[i].ds2;
	width = ds->box->width / ds->colspan;
	if (width > ts->cwidth[i]) ts->cwidth[i] = width;
      }
    }
  }

  /*
   * Determine the width of the unrestrained table.
   */
  twidth = 0;
  for (i = 0; i < ts->colcount; i++)
  {
    twidth += ts->cwidth[i];
  }

  if (twidth > 0 && twidth > ts->max_width)
  {
    /*
     * Now scale the columns to fit in the space available attempting to keep
     * things in proportion.
     */
    for (i = 0; i < ts->colcount; i++)
    {
      ts->swidth[i] = ts->max_width * ts->cwidth[i] / twidth;
    }
  }
  else
  {
    /*
     * No size problems so just copy the widths as-is.
     */
    for (i = 0; i < ts->colcount; i++)
    {
      ts->swidth[i] = ts->cwidth[i];
    }
  }

  /*
   * Finally, figure out the widths of individual cells.
   */
  for (rs = (HRow *)GListGetHead(ts->grid); rs != NULL;
       rs = (HRow *)GListGetNext(ts->grid))
  {
    for (i = 0; i < ts->colcount; i++)
    {
      if (rs->ca[i].ds != NULL && rs->ca[i].original)
      {
	/*
	 * Add in the widths of all the columns for a column spanning
	 * cell.
	 */
	ds = rs->ca[i].ds;
	for (j = 0; j + i < ts->colcount && j < ds->colspan; j++)
	{
	  ds->width += ts->swidth[i + j];
	}
      }
    }
  }

  return;
}

/*
 * TablePosition2
 */
static void
TablePosition2(ts)
HTable *ts;
{
  int i, j;
  unsigned int width, height;
  HRow *rs;
  HData *ds;

  memset(ts->cwidth, 0, ts->colcount * sizeof(unsigned int));
  memset(ts->rheight, 0, ts->rowcount * sizeof(unsigned int));

  /*
   * Get the columns widths and row heights for the restrained table
   * (second pass).
   */
  for (j = 0, rs = (HRow *)GListGetHead(ts->grid); rs != NULL;
       rs = (HRow *)GListGetNext(ts->grid), j++)
  {
    for (i = 0; i < ts->colcount; i++)
    {
      if (rs->ca[i].ds != NULL)
      {
	ds = rs->ca[i].ds;
	width = ds->box->width / ds->colspan;
	if (width > ts->cwidth[i]) ts->cwidth[i] = width;
	
	height = ds->box->height / ds->rowspan;
	if (height > ts->rheight[j]) ts->rheight[j] = height;
      }

      if (rs->ca[i].ds2 != NULL)
      {
	ds = rs->ca[i].ds2;
	width = ds->box->width / ds->colspan;
	if (width > ts->cwidth[i]) ts->cwidth[i] = width;
	
	height = ds->box->height / ds->rowspan;
	if (height > ts->rheight[j]) ts->rheight[j] = height;
      }
    }
  }

  /*
   * Find the final width and height of the table.
   */
  width = 0;
  height = 0;
  for (i = 0; i < ts->colcount; i++)
  {
    width += ts->cwidth[i];
  }
  for (i = 0; i < ts->rowcount; i++)
  {
    height += ts->rheight[i];
  }

  ts->box->width = width;
  ts->box->height = height;

  return;
}

/*
 * HTMLTableAccept
 */
bool
HTMLTableAccept(li, obj)
HTMLInfo li;
HTMLObject obj;
{
  if (obj->type != HTML_ENV) return(false);
  if (HTMLTagToID(obj->o.env->tag) != TAG_TR) return(false);
  return(true);
}

/*
 * HTMLTRAccept
 */
bool
HTMLTRAccept(li, obj)
HTMLInfo li;
HTMLObject obj;
{
  HTMLTagID tagid;

  if (obj->type != HTML_ENV) return(false);
  tagid = HTMLTagToID(obj->o.env->tag);
  if (tagid != TAG_TD && tagid != TAG_TH) return(false);

  return(true);
}

/*
 * HTMLTDInsert
 */
HTMLInsertStatus
HTMLTDInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv c;
  HTMLTagID tid;

  for (c = (HTMLEnv)GListGetHead(li->envstack); c != NULL;
       c = (HTMLEnv)GListGetNext(li->envstack))
  {
    tid = HTMLTagToID(c->tag);
    if (tid == TAG_TABLE || tid == TAG_TR) break;
  }

  if (c == NULL) return(HTMLInsertReject);
  else if (tid == TAG_TABLE) HTMLStartEnv(li, TAG_TR, NULL);
  else HTMLPopEnv(li, TAG_TR);

  return(HTMLInsertOK);
}

/*
 * HTMLTDClamp
 */
bool
HTMLTDClamp(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HTMLEnv c;
  HTMLTagID tid;

  for (c = (HTMLEnv)GListGetHead(li->envstack); c != NULL;
       c = (HTMLEnv)GListGetNext(li->envstack))
  {
    tid = HTMLTagToID(c->tag);
    if (tid == TAG_TABLE || tid == TAG_TD) break;
  }
  if (c == NULL) return(false);
  else if (tid == TAG_TABLE) return(false);

  return(true);
}

/*
 * HTMLTRInsert
 */
HTMLInsertStatus
HTMLTRInsert(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv tenv;

  if ((tenv = HTMLFindEnv(li, TAG_TABLE)) == NULL) return(HTMLInsertReject);

  HTMLPopEnv(li, TAG_TABLE);

  return(HTMLInsertOK);
}

/*
 * HTMLTRClamp
 */
bool
HTMLTRClamp(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HTMLEnv c;
  HTMLTagID tid;

  for (c = (HTMLEnv)GListGetHead(li->envstack); c != NULL;
       c = (HTMLEnv)GListGetNext(li->envstack))
  {
    tid = HTMLTagToID(c->tag);
    if (tid == TAG_TABLE || tid == TAG_TR) break;
  }
  if (c == NULL) return(false);
  else if (tid == TAG_TABLE) return(false);

  return(true);
}

/*
 * HTMLTDWidth
 */
unsigned int
HTMLTDWidth(li, env)
HTMLInfo li;
HTMLEnv env;
{
  HTable *ts = (HTable *)env->closure;
  return(HTMLGetBoxWidth(li, ts->dbox));
}

