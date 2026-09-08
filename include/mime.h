/*
 * mime.h
 *
 * Copyright (c) 1997, John Kilburg <john@cs.unlv.edu>
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

#ifndef __MIME_H__
#define __MIME_H__ 1

typedef struct MIMEHeaderP *MIMEHeader;

MIMEHeader MIMECreateHeader _ArgProto((void));
int MIMEParseBuffer _ArgProto((MIMEHeader, char *, size_t));
void MIMEDestroyHeader _ArgProto((MIMEHeader));
int MIMEGetField _ArgProto((MIMEHeader, char *, char **));
void MIMEAddField _ArgProto((MIMEHeader, char *, char *));
void MIMEAddLine _ArgProto((MIMEHeader, char *));
int MIMEGetHeaderEnd _ArgProto((MIMEHeader, size_t *));
int MIMEFindData _ArgProto((MIMEHeader, char *, size_t, size_t *));
int MIMEWriteHeader _ArgProto((MIMEHeader, FILE *));

#endif

