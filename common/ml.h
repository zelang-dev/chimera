/*
 * ml.h
 *
 * Copyright (c) 1996-1997, John Kilburg <john@cs.unlv.edu>
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
typedef struct MLElementP *MLElement;
typedef struct MLAttributeP *MLAttribute;
typedef struct MLStateP *MLState;

typedef enum
{
  ML_BEGINTAG,
  ML_ENDTAG,
  ML_DATA,
  ML_CONTROLTAG,
  ML_EOF
} MLElementType;

/*
 * Public functions
 */

/*
 * Functions for providing raw data to the markup-language code.
 */
typedef void (*MLElementHandler) _ArgProto((void *, MLElement));

MLState MLInit _ArgProto((MLElementHandler, void *));
void MLAddData _ArgProto((MLState, char *, size_t));
void MLEndData _ArgProto((MLState, char *, size_t));
void MLDestroy _ArgProto((MLState));

/*
 * Functions for extracting information about elements created
 */
char *MLFindAttribute _ArgProto((MLElement, char *));
char *MLTagName _ArgProto((MLElement));
void MLGetText _ArgProto((MLElement, char **, size_t *));
MLElementType MLGetType(MLElement);
int MLAttributeToInt _ArgProto((MLElement, char *));

/*
 * Random stuff
 */
MLElement MLCreateTag _ArgProto((MLState, char *, size_t));
