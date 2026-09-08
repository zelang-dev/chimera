/*
 * htmltags.h
 *
 * libhtml - HTML->X renderer
 * Copyright (c) 1994-1997, John Kilburg <john@cs.unlv.edu>
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

typedef enum
{
  MARKUP_NONE    = 0x00000000,
  MARKUP_EMPTY   = 0x00000001,
  MARKUP_CLAMP   = 0x00000002,
  MARKUP_FAKE    = 0x00000004,
  MARKUP_SPACER  = 0x00000008,
  MARKUP_TWOPASS = 0x00000010
} HTMLMarkupFlags;

struct HTMLTagP
{
  char *name;
  HTMLTagID id;

  /* rendering functions */
  HTMLTagRenderProc b, e;
  HTMLTagDataProc d;
  HTMLTagAddBoxProc a;
  HTMLTagWidthProc w;

  /* flags for whatever seems convienent to make a flag */
  HTMLMarkupFlags mflags;

  /* markup handling functions */
  HTMLTagAcceptProc m;
  HTMLTagInsertProc p;
  HTMLTagClampProc c;
};

#define HTMLTestM(a, b)     (((a)->mflags & (b)) != 0)
#define HTMLClearM(a, b)    ((a)->mflags &= ~(b))
#define HTMLSetM(a, b)      ((a)->mflags |= (b))

static struct HTMLTagP tlist[] =
{
  /* misc.c */
  { "h1", TAG_H1, HTMLHxBegin, HTMLHxEnd, HTMLFillData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "h2", TAG_H2, HTMLHxBegin, HTMLHxEnd, HTMLFillData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "h3", TAG_H3, HTMLHxBegin, HTMLHxEnd, HTMLFillData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "h4", TAG_H4, HTMLHxBegin, HTMLHxEnd, HTMLFillData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "h5", TAG_H5, HTMLHxBegin, HTMLHxEnd, HTMLFillData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "h6", TAG_H6, HTMLHxBegin, HTMLHxEnd, HTMLFillData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "plaintext", TAG_PLAINTEXT, HTMLPlainBegin, HTMLPlainEnd,
	HTMLPlainData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "listing", TAG_LISTING, HTMLPlainBegin, HTMLPlainEnd,
	HTMLPlainData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "pre", TAG_PRE, HTMLPreBegin, HTMLPreEnd, HTMLPreData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "xmp", TAG_XMP, HTMLPlainBegin, HTMLPlainEnd,
	HTMLPlainData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, NULL, NULL },
  { "center", TAG_CENTER, HTMLCenterBegin, HTMLCenterEnd,
	HTMLFillData, NULL, NULL,
	MARKUP_NONE,
	NULL, NULL, NULL },
  { "div", TAG_DIV, HTMLDivBegin, HTMLDivEnd, HTMLFillData, NULL, NULL,
	MARKUP_NONE,
	NULL, NULL, NULL },
  { "blockquote", TAG_BLOCKQUOTE, HTMLBQBegin, HTMLBQEnd,
	HTMLFillData, NULL, NULL,
	MARKUP_CLAMP,
	NULL, NULL, NULL },
  { "p", TAG_P, HTMLParaBegin, HTMLParaEnd, HTMLFillData, NULL, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	NULL, HTMLParaInsert, NULL },
  { "address", TAG_ADDRESS, HTMLAddressBegin, HTMLAddressEnd,
	HTMLFillData, NULL, NULL,
	MARKUP_NONE, NULL, NULL, NULL },
  { "hr", TAG_HR, HTMLHorizontalRule, NULL, NULL, NULL, NULL,
	MARKUP_EMPTY,
	NULL, NULL, NULL },
  { "b", TAG_B, HTMLFontBegin, NULL, HTMLFillData, NULL, NULL,
	MARKUP_NONE,
	NULL, NULL, NULL },
  { "em", TAG_EM, HTMLFontBegin, NULL, HTMLFillData, NULL, NULL,
	MARKUP_NONE,
	NULL, NULL, NULL },
  { "strong", TAG_STRONG, HTMLFontBegin, NULL, HTMLFillData, NULL, NULL,
	MARKUP_NONE, NULL, NULL, NULL },
  { "tt", TAG_TT, HTMLFontBegin, NULL, HTMLFillData, NULL, NULL,
	MARKUP_NONE,
	NULL, NULL, NULL },
  { "cite", TAG_CITE, HTMLFontBegin, NULL, HTMLFillData, NULL, NULL,
	MARKUP_NONE,
	NULL, NULL, NULL },
  { "i", TAG_I, HTMLFontBegin, NULL, HTMLFillData, NULL, NULL,
	MARKUP_NONE,
	NULL, NULL, NULL },
  { "kbd", TAG_KBD, HTMLFontBegin, NULL, HTMLFillData, NULL, NULL,
	MARKUP_NONE,
	NULL, NULL, NULL },
  { "a", TAG_A, HTMLAnchorBegin, NULL, HTMLFillData, NULL, NULL,
	MARKUP_NONE,
	NULL, HTMLAnchorInsert, NULL },
  { "br", TAG_BR, HTMLBreak, NULL, NULL, NULL, NULL,
	MARKUP_EMPTY,
	NULL, NULL, NULL },

  /* inline.c */
  { "img", TAG_IMG, HTMLImg, NULL, NULL, NULL, NULL,
	MARKUP_EMPTY,
	NULL, HTMLImgInsert, NULL },

  /* list.c */
  { "ul", TAG_UL, HTMLULBegin, HTMLListEnd, NULL, HTMLListAddBox, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	HTMLListAccept, NULL, NULL },
  { "dir", TAG_UL, HTMLULBegin, HTMLListEnd, NULL, HTMLListAddBox, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	HTMLListAccept, NULL, NULL },
  { "menu", TAG_UL, HTMLULBegin, HTMLListEnd, NULL, HTMLListAddBox, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	HTMLListAccept, NULL, NULL },
  { "ol", TAG_OL, HTMLOLBegin, HTMLListEnd, NULL, HTMLListAddBox, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	HTMLListAccept, NULL, NULL },
  { "dl", TAG_DL, HTMLDLBegin, HTMLListEnd, NULL, HTMLListAddBox, NULL,
	MARKUP_CLAMP | MARKUP_SPACER,
	HTMLDLAccept, NULL, NULL },
  { "li", TAG_LI, HTMLLIBegin, HTMLItemEnd, HTMLFillData, HTMLItemAddBox,
	HTMLItemWidth,
	MARKUP_CLAMP,
	NULL, HTMLLIInsert, NULL },
  { "dt", TAG_DT, HTMLDTBegin, HTMLItemEnd, HTMLFillData, HTMLItemAddBox,
	HTMLItemWidth,
	MARKUP_CLAMP,
	NULL, HTMLDTInsert, NULL },
  { "dd", TAG_DD, HTMLDDBegin, HTMLItemEnd, HTMLFillData,
	HTMLItemAddBox, HTMLItemWidth,
	MARKUP_CLAMP,
	NULL, HTMLDDInsert, NULL },

  /* head.c */
  { "title", TAG_TITLE, NULL, HTMLTitleEnd, NULL, NULL, NULL,
	MARKUP_CLAMP,
	NULL, NULL, NULL },
  { "style", TAG_STYLE, NULL, NULL, NULL, NULL, NULL,
	MARKUP_CLAMP,
	NULL, NULL, NULL },
  { "script", TAG_SCRIPT, NULL, NULL, NULL, NULL, NULL,
	MARKUP_CLAMP,
	NULL, NULL, NULL },
  { "base", TAG_BASE, HTMLBase, NULL, NULL, NULL, NULL,
	MARKUP_EMPTY,
	NULL, NULL, NULL },
  { "meta", TAG_META, HTMLMeta, NULL, NULL, NULL, NULL,
	MARKUP_EMPTY,
	NULL, NULL, NULL },

  /* form.c */
  { "input", TAG_INPUT, HTMLInput, NULL, NULL, NULL, NULL,
	MARKUP_EMPTY,
	NULL, NULL, NULL },
  { "select", TAG_SELECT, HTMLSelectBegin, HTMLSelectEnd, NULL, NULL, NULL,
        MARKUP_CLAMP,
	NULL, NULL, NULL },
  { "option", TAG_OPTION, NULL, HTMLOptionEnd, NULL, NULL, NULL,
	MARKUP_NONE,
	NULL, NULL, NULL },
  { "textarea", TAG_TEXTAREA, NULL, HTMLTextareaEnd, NULL, NULL, NULL,
        MARKUP_CLAMP,
	NULL, NULL, NULL },
  { "form", TAG_FORM, HTMLFormBegin, HTMLFormEnd, HTMLFillData, NULL, NULL,
	MARKUP_CLAMP, NULL, NULL, NULL },

  /* table.c */
  { "table", TAG_TABLE, HTMLTableBegin, HTMLTableEnd, NULL, HTMLTableAddBox,
	NULL,
	MARKUP_CLAMP | MARKUP_SPACER | MARKUP_TWOPASS,
	HTMLTableAccept, NULL, NULL },
  { "tr", TAG_TR, HTMLTRBegin, HTMLTREnd, NULL, NULL, NULL,
	MARKUP_CLAMP,
	HTMLTRAccept, HTMLTRInsert, HTMLTRClamp },
  { "th", TAG_TH, HTMLTDBegin, HTMLTDEnd, HTMLFillData, HTMLTDAddBox,
	HTMLTDWidth,
	MARKUP_CLAMP,
	NULL, HTMLTDInsert, HTMLTDClamp },
  { "td", TAG_TD, HTMLTDBegin, HTMLTDEnd, HTMLFillData, HTMLTDAddBox,
	HTMLTDWidth,
	MARKUP_CLAMP,
	NULL, HTMLTDInsert, HTMLTDClamp },

  /* map.c */
  { "map", TAG_MAP, HTMLMapBegin, HTMLMapEnd, NULL, NULL, NULL,
        MARKUP_CLAMP,
	HTMLMapAccept, NULL, NULL },
  { "area", TAG_AREA, HTMLAreaBegin, NULL, NULL, NULL, NULL,
	MARKUP_EMPTY,
	NULL, HTMLAreaInsert, NULL },

  /* frame.c */
  { "iframe", TAG_IFRAME, HTMLIFrame, NULL, NULL, NULL, NULL,
	MARKUP_CLAMP,
	NULL, NULL, NULL },
  { "frame", TAG_FRAME, HTMLFrame, NULL, NULL, NULL, NULL,
	MARKUP_EMPTY,
	NULL, HTMLFrameInsert, NULL },
  { "frameset", TAG_FRAMESET, HTMLFrameSetBegin, HTMLFrameSetEnd,
	NULL, NULL, NULL,
	MARKUP_CLAMP,
	HTMLFrameSetAccept, HTMLFrameSetInsert, NULL },
  { "noframes", TAG_NOFRAMES, HTMLNoFramesBegin, HTMLNoFramesEnd,
	NULL, NULL, NULL,
	MARKUP_CLAMP,
	NULL, NULL, NULL },

  /* Internal tags */
  { "xxx-document", TAG_DOCUMENT, HTMLDocumentBegin, HTMLDocumentEnd, 
	HTMLFillData, HTMLDocumentAddBox, HTMLDocumentWidth,
	MARKUP_FAKE, NULL, NULL, NULL }
};

static int tlist_len = sizeof(tlist) / sizeof(tlist[0]);
