/*
 * css.h
 *
 * Copyright (c) 1998, John Kilburg <john@cs.unlv.edu>
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

typedef struct CSSPropertyP *CSSProperty;
typedef struct CSSSelectorP *CSSSelector;
typedef struct CSSRuleP *CSSRule;
typedef struct CSSContextP *CSSContext;

typedef void (*CSSProc) _ArgProto((void *));

void CSSDestroyContext _ArgProto((CSSContext));

CSSContext CSSParseBuffer _ArgProto((ChimeraContext,
				     char *, size_t, CSSProc, void *));

CSSSelector CSSCreateSelector _ArgProto((MemPool));

void CSSSetSelector _ArgProto((CSSSelector,
			       char *, char *, char *, char *));

char *CSSFindProperty _ArgProto((CSSContext, GList, char *));

void CSSPrintSelectorList _ArgProto((GList));

void CSSPrint _ArgProto((CSSContext));
