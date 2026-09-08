/*
 * html.h
 *
 * libhtml - HTML->X renderer
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
#ifndef __HTML_H__
#define __HTML_H__ 1

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include "common.h"
#include "Chimera.h"
#include "ChimeraRender.h"
#include "ChimeraGUI.h"
#include "ml.h"
#include "css.h"

/* proposed HTML3.2 tags and some internal tags */
typedef enum
{
  /* internal tags */
  TAG_DOCUMENT,

  /* implemented? */
  TAG_BR,
  TAG_ADDRESS,
  TAG_DIV,
  TAG_CENTER,
  TAG_A,
  TAG_MAP,
  TAG_AREA,
  TAG_IMG,
  TAG_HR,
  TAG_P,
  TAG_PRE,
  TAG_XMP,
  TAG_LISTING,
  TAG_PLAINTEXT,
  TAG_BLOCKQUOTE,
  TAG_DL,
  TAG_DD,
  TAG_DT,
  TAG_OL,
  TAG_UL,
  TAG_DIR,
  TAG_MENU,
  TAG_LI,
  TAG_FORM,
  TAG_INPUT,
  TAG_SELECT,
  TAG_OPTION,
  TAG_TEXTAREA,
  TAG_TABLE,
  TAG_TR,
  TAG_TH,
  TAG_TD,
  TAG_TITLE,
  TAG_BASE,
  TAG_SCRIPT,
  TAG_H1,
  TAG_H2,
  TAG_H3,
  TAG_H4,
  TAG_H5,
  TAG_H6,
  TAG_TT,
  TAG_I,
  TAG_B,
  TAG_STRIKE,
  TAG_BIG,
  TAG_SMALL,
  TAG_SUB,
  TAG_SUP,
  TAG_EM,
  TAG_STRING,
  TAG_DFN,
  TAG_CODE,
  TAG_SAMP,
  TAG_KBD,
  TAG_VAR,
  TAG_CITE,
  TAG_STRONG,

  /* do not implement */
  TAG_HTML,
  TAG_BODY,
  TAG_HEAD,

  /* weird stuff? */
  TAG_IFRAME,
  TAG_FRAME,
  TAG_FRAMESET,
  TAG_NOFRAMES,

  /* unimplemented */
  TAG_LINK,
  TAG_APPLET,
  TAG_META,
  TAG_STYLE,
  TAG_CAPTION,
  TAG_FONT,
  TAG_PARAM,
  TAG_ISINDEX
} HTMLTagID;

typedef struct HTMLBoxP       *HTMLBox;
typedef struct HTMLInfoP      *HTMLInfo;
typedef struct HTMLClassP     *HTMLClass;
typedef struct HTMLObjectP    *HTMLObject;
typedef struct HTMLEnvP       *HTMLEnv;
typedef struct HTMLFontP      *HTMLFont;
typedef struct HTMLFontListP  *HTMLFontList;
typedef struct HTMLAnchorP    *HTMLAnchor;
typedef struct HTMLTagP       *HTMLTag;
typedef struct HTMLMapP       *HTMLMap;
typedef struct HTMLAreaP      *HTMLArea;
typedef struct HTMLStateP     *HTMLState;

/*
 * Box callbacks
 */
typedef void (*HTMLSetupProc) _ArgProto((HTMLInfo, HTMLBox));
typedef void (*HTMLDestroyProc) _ArgProto((HTMLInfo, HTMLBox));
typedef void (*HTMLRenderProc) _ArgProto((HTMLInfo, HTMLBox, Region));
typedef void (*HTMLLayoutProc) _ArgProto((HTMLInfo, HTMLBox, HTMLBox));
typedef unsigned int (*HTMLWidthProc) _ArgProto((HTMLInfo, HTMLBox));

/*
 * Tag callbacks
 */
typedef bool (*HTMLTagClampProc) _ArgProto((HTMLInfo, HTMLEnv));
typedef void (*HTMLTagRenderProc) _ArgProto((HTMLInfo, HTMLEnv, MLElement));
typedef bool (*HTMLTagAcceptProc) _ArgProto((HTMLInfo, HTMLObject));

typedef enum
{
  HTMLInsertEmpty,
  HTMLInsertReject,
  HTMLInsertOK
} HTMLInsertStatus;

typedef HTMLInsertStatus (*HTMLTagInsertProc) _ArgProto((HTMLInfo,
							 HTMLEnv, MLElement));
typedef void (*HTMLTagDataProc) _ArgProto((HTMLInfo, HTMLEnv, MLElement));
typedef void (*HTMLTagAddBoxProc) _ArgProto((HTMLInfo, HTMLEnv, HTMLBox));
typedef unsigned int (*HTMLTagWidthProc) _ArgProto((HTMLInfo, HTMLEnv));

typedef enum
{
  FLOW_LEFT_JUSTIFY,
  FLOW_CENTER_JUSTIFY,
  FLOW_RIGHT_JUSTIFY
} HTMLFlowAlign;

/*
 * Environment
 */
typedef enum
{
  HTML_ENV,
  HTML_ELEMENT,
  HTML_TAG,
  HTML_BEGINTAG,
  HTML_ENDTAG
} HTMLObjectType;

struct HTMLObjectP
{
  HTMLObjectType type;
  union
  {
    MLElement p;
    HTMLEnv env;
  } o;
};

struct HTMLEnvP
{
  /* Keep track of html structure */
  HTMLEnv           penv;                  /* parent environment */
  HTMLTag           tag;                   /* tag that defines environment */
  GList             olist;                 /* object list */
  GList             blist;                 /* boxed object list */
  GList             slist;                 /* static object list */
  int               pass;

  /* Keep track of HTML rendering */
  bool              visited;               /* used by boxify functions */
  MLElement         anchor;                /* anchor to use in environment */
  HTMLFont          fi;                    /* font to use in environment */
  HTMLFlowAlign     ff;                    /* alignment in flow box */ 
  void              *closure;              /* environment specific data */
};

typedef enum
{
  BOX_NONE                = 0x00000000,    /* no flags set */
  BOX_SPACE               = 0x00000001,    /* horizontal space */
  BOX_LINEBREAK           = 0x00000002,    /* line break, duh. */
  BOX_FLOAT_LEFT          = 0x00000008,    /* float to the left edge */
  BOX_FLOAT_RIGHT         = 0x00000010,    /* float to the right edge */
  BOX_VCENTER             = 0x00000020,    /* vertically center in parent */
  BOX_PUSHED_LEFT         = 0x00000040,    /* box pushed left by floater */
  BOX_PUSHED_RIGHT        = 0x00000080,    /* box pushed right by floater */
  BOX_SELECT              = 0x00000100,    /* anchor box selected */
  BOX_TOPLEVEL            = 0x00000200,    /* toplevel box ? */
  BOX_CLEAR_LEFT          = 0x00000400,    /* clear past left aligned */
  BOX_CLEAR_RIGHT         = 0x00000800     /* clear past right aligned */
} HTMLBoxFlags;

#define HTMLTestB(a, b)     (((a)->bflags & (b)) != 0)
#define HTMLClearB(a, b)    ((a)->bflags &= ~(b))
#define HTMLSetB(a, b)      ((a)->bflags |= (b))

/* This is still too big */
struct HTMLBoxP
{
  HTMLEnv      env;                 /* */
  int          baseline;            /* level of alignment */
  int          x, y;                /* upper-left coordinates */
  unsigned int width, height;       /* external dimensions */
  HTMLBoxFlags bflags;              /* misc. worthless flags */

  /* rendering functions */
  HTMLRenderProc render;            /* called when output needed */
  HTMLDestroyProc destroy;          /* called when destroyed */
  HTMLSetupProc  setup;             /* called by parent box */
  HTMLLayoutProc layout;            /* child box layout */
  HTMLWidthProc maxwidth;           /* called to return maximum width */
  void         *closure;            /* state for functions */
};

struct HTMLAnchorP
{
  HTMLBox box;
  char *name;
  MLElement p;
};

struct HTMLStateP
{
  int x, y;
};

struct HTMLClassP
{
  MemPool     mp;
  bool        font_setup_done;
  Display     *dpy;
  XFontStruct *defaultFont;
  HTMLFontList prop;
  HTMLFontList fixed;
  GList       oldstates;
  GList       fonts;
  GList       contexts;
  CSSContext  css;
};

struct HTMLInfoP
{
  /* X */
  Widget      widget;
  Display     *dpy;
  Window      win;
  GC          gc;
  Pixel       anchor_color;
  Pixel       anchor_select_color;
  Pixel       fg, bg;
  
  /* www lib */
  ChimeraContext  wc;
  ChimeraResources cres;
  ChimeraGUI      wd;
  ChimeraSink     wp;
  ChimeraTask     wt;
  ChimeraRender   wn;
  GList       sinks;
  GList       loads;

  /* HTML */
  CSSContext  css;
  bool        reload;
  unsigned int maxwidth, maxheight;
  MemPool     mp;
  MLState     hs;
  HTMLClass   lc;
  HTMLFont    cfi;
  HTMLBox     firstbox;
  HTMLState   ps;

  HTMLEnv     searchenv;
  HTMLObject  searchobj;

  HTMLEnv     topenv;               /* document environment */
  GList       envstack;             /* tag hierarchy */
  GList       selectors;            /* CSS selector list */
  GList       oldselectors;         /* old CSS selector list */
  GList       endstack;             /* pending end tags */   

  GList       alist;                /* anchor box list */
  ChimeraTimeOut  sto;              /* select timeout */
  HTMLAnchor  sa;                   /* selected anchor */

  int         delayed;
  char        *url;                 /* original URL */
  char        *burl;                /* base URL */
  char        *title;               /* title text */
  HTMLAnchor  over;
  GList       maps;
  bool        cancel;
  bool	      formatted;	    /* inside PRE tags - djhjr */
  bool        noframeset;
  bool        framesonly;
  GList       framesets;            /* list of frames */

  /* configurable goodies */
  int         textLineSpace;
  bool        printHTMLErrors;
  int         tmargin, bmargin;     /* top, bottom margins */
  int         rmargin, lmargin;     /* right, left margins */
  unsigned int inlineTimeOut;
  unsigned int selectTimeOut;       /* select timeout time */
  unsigned int tableCellInfinity;   /* infinite width for table cell */
  bool        flowDebug;
  bool        constraintDebug;
  bool        printTags;            /* print tag and data hierarchy */
};

typedef enum
{
  ATTRIB_UNKNOWN,
  ATTRIB_LEFT,
  ATTRIB_RIGHT,
  ATTRIB_TOP,
  ATTRIB_BOTTOM,
  ATTRIB_CENTER,
  ATTRIB_MIDDLE,
  ATTRIB_ALL
} HTMLAttribID;

/*
 * inline.c
 */
typedef struct
{
  MLElement p;
  void      *closure;
} HTMLInlineInfo;

typedef struct HTMLInlineP *HTMLInline;

HTMLInline HTMLCreateInline _ArgProto((HTMLInfo, HTMLEnv, char *,
				       HTMLInlineInfo *,
				       ChimeraRenderHooks *, void *));
HTMLBox HTMLInlineToBox _ArgProto((HTMLInline));
void HTMLInlineDestroy _ArgProto((HTMLInline));

/*
 * text.c
 */
HTMLBox HTMLCreateTextBox _ArgProto((HTMLInfo, HTMLEnv, char *, size_t));
char *HTMLGetEnvText _ArgProto((MemPool, HTMLEnv));
void HTMLStringSpacify _ArgProto((char *, size_t));
void HTMLAddLineBreak _ArgProto((HTMLInfo, HTMLEnv));
void HTMLAddBlankLine _ArgProto((HTMLInfo, HTMLEnv));

/*
 * layout.c
 */
HTMLBox HTMLCreateBox _ArgProto((HTMLInfo, HTMLEnv));
void HTMLLayoutBox _ArgProto((HTMLInfo, HTMLBox, HTMLBox));
void HTMLSetupBox _ArgProto((HTMLInfo, HTMLBox));
void HTMLRenderBox _ArgProto((HTMLInfo, Region, HTMLBox));
void HTMLDestroyBox _ArgProto((HTMLInfo, HTMLBox));
unsigned HTMLGetBoxWidth _ArgProto((HTMLInfo, HTMLBox));

/*
 * html.c
 */
HTMLAttribID HTMLAttributeToID _ArgProto((MLElement, char *));
HTMLTag HTMLGetTag _ArgProto((MLElement));
HTMLTag HTMLTagIDToTag _ArgProto((HTMLTagID));
bool HTMLHandleTag _ArgProto((HTMLInfo, HTMLTag, MLElement));
HTMLEnv HTMLFindEnv _ArgProto((HTMLInfo, HTMLTagID));
void HTMLDelayLayout _ArgProto((HTMLInfo));
void HTMLContinueLayout _ArgProto((HTMLInfo));
HTMLTagID HTMLTagToID _ArgProto((HTMLTag));
void HTMLHandler _ArgProto((void *, MLElement));
HTMLEnv HTMLPopEnv _ArgProto((HTMLInfo, HTMLTagID));
void HTMLStartEnv _ArgProto((HTMLInfo, HTMLTagID, MLElement));
void HTMLEndEnv _ArgProto((HTMLInfo, HTMLTagID));
unsigned int HTMLGetMaxWidth _ArgProto((HTMLInfo, HTMLEnv));
void HTMLEnvAddBox _ArgProto((HTMLInfo, HTMLEnv, HTMLBox));
void HTMLStart _ArgProto((HTMLInfo));
void HTMLFinish _ArgProto((HTMLInfo));
HTMLEnv HTMLGetIDEnv _ArgProto((HTMLEnv, HTMLTagID));

/*
 * font.c
 */
XFontStruct *HTMLGetFont _ArgProto((HTMLInfo, HTMLEnv));
void HTMLSetupFonts _ArgProto((HTMLInfo));
void HTMLFreeFonts _ArgProto((HTMLClass));
void HTMLAddFontSlant _ArgProto((HTMLFont));
void HTMLAddFontWeight _ArgProto((HTMLFont));
void HTMLAddFontScale _ArgProto((HTMLFont));
void HTMLSetFontScale _ArgProto((HTMLFont, int));
HTMLFont HTMLDupFont _ArgProto((HTMLInfo, HTMLFont));
void HTMLSetFontFixed _ArgProto((HTMLFont));
void HTMLSetFontProp _ArgProto((HTMLFont));

/*
 * load.c
 */
int HTMLLoadAnchor _ArgProto((HTMLInfo, HTMLAnchor, int, int, char *, bool));
void HTMLPrintAnchor _ArgProto((HTMLInfo, HTMLAnchor, int, int, bool));
char *HTMLMakeURL _ArgProto((HTMLInfo, MemPool, char *));
int HTMLLoadURL _ArgProto((HTMLInfo, char *, char *, char *));
void HTMLPrintURL _ArgProto((HTMLInfo, char *));
void HTMLFindName _ArgProto((HTMLInfo, char *));
void HTMLAddAnchor _ArgProto((HTMLInfo, HTMLBox, char *, MLElement));

/*
 * module.c
 */
void HTMLSetFinalPosition _ArgProto((HTMLInfo));

/*
 * flow.c
 */
HTMLBox HTMLCreateFlowBox _ArgProto((HTMLInfo, HTMLEnv, unsigned int));
void HTMLFinishFlowBox _ArgProto((HTMLInfo, HTMLBox));

/*
 * map.c
 */
char *HTMLFindMapURL _ArgProto((HTMLInfo, char *, int, int));

/*
 *
 * Tag/Env handlers
 *
 */

/*
 * form.c
 */
void HTMLFormBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLFormEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLInput _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLOptionEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLSelectBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLSelectEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLTextareaEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));

/*
 * inline.c
 */
void HTMLImg _ArgProto((HTMLInfo, HTMLEnv, MLElement));
HTMLInsertStatus HTMLImgInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));

/*
 * hr.c
 */
void HTMLHorizontalRule _ArgProto((HTMLInfo, HTMLEnv, MLElement));

/*
 * list.c
 */
HTMLInsertStatus HTMLLIInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLLIBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
HTMLInsertStatus HTMLDTInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLDTBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
HTMLInsertStatus HTMLDDInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLDDBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
bool HTMLListAccept _ArgProto((HTMLInfo, HTMLObject));
bool HTMLDLAccept _ArgProto((HTMLInfo, HTMLObject));
void HTMLULBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLListEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLOLBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLDLBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLListAddBox _ArgProto((HTMLInfo, HTMLEnv, HTMLBox));
void HTMLItemAddBox _ArgProto((HTMLInfo, HTMLEnv, HTMLBox));
unsigned int HTMLItemWidth _ArgProto((HTMLInfo, HTMLEnv));
void HTMLItemEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));

/*
 * text.c
 */
void HTMLPreBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLPreEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLPlainBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLPlainEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLFillData _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLPreData _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLPlainData _ArgProto((HTMLInfo, HTMLEnv, MLElement));

/*
 * misc.c
 */
void HTMLHxBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLHxEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLParaBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLParaEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
HTMLInsertStatus HTMLParaInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLBreak _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLAddressBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLAddressEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLAnchorBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
HTMLInsertStatus HTMLAnchorInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLBQBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLBQEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLFontBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLDivBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLDivEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLCenterBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLCenterEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));

/*
 * table.c
 */
bool HTMLTableAccept _ArgProto((HTMLInfo, HTMLObject));
void HTMLTableBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLTableEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
bool HTMLTRAccept _ArgProto((HTMLInfo, HTMLObject));
HTMLInsertStatus HTMLTRInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
bool HTMLTRClamp _ArgProto((HTMLInfo, HTMLEnv));
void HTMLTRBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLTREnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
HTMLInsertStatus HTMLTDInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
bool HTMLTDClamp _ArgProto((HTMLInfo, HTMLEnv));
void HTMLTDBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLTDEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
unsigned int HTMLTDWidth _ArgProto((HTMLInfo, HTMLEnv));
void HTMLTableAddBox _ArgProto((HTMLInfo, HTMLEnv, HTMLBox));
void HTMLTDAddBox _ArgProto((HTMLInfo, HTMLEnv, HTMLBox));

/*
 * head.c
 */
void HTMLBase _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLTitleEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLMeta _ArgProto((HTMLInfo, HTMLEnv, MLElement));

/*
 * map.c
 */
bool HTMLMapAccept _ArgProto((HTMLInfo, HTMLObject));
HTMLInsertStatus HTMLAreaInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLAreaBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLMapBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLMapEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));

/*
 * frame.c
 */
void HTMLIFrame _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLFrame _ArgProto((HTMLInfo, HTMLEnv, MLElement));
HTMLInsertStatus HTMLFrameInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLFrameSetBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLFrameSetEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
bool HTMLFrameSetAccept _ArgProto((HTMLInfo, HTMLObject));
HTMLInsertStatus HTMLFrameSetInsert _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLNoFramesBegin _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLNoFramesEnd _ArgProto((HTMLInfo, HTMLEnv, MLElement));
void HTMLDestroyFrameSets _ArgProto((HTMLInfo));
void HTMLFrameLoad _ArgProto((HTMLInfo, ChimeraRequest *, char*));

#endif
