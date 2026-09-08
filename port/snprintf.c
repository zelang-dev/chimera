/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1995 Patrick Powell, San Diego State University
 *     papowell@sdsu.edu
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: snprintf.c
 * PURPOSE: LPRng version of printf - absolutely bombproof (hopefully!)
 **************************************************************************/

/*
 *
 *
 * Removed the 'plp' from function names and may have made other
 * modifications so if it looks mangled its my fault.
 *
 * -john
 *
 *
 */
#include <stdio.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#define HAVE_STDARGS    /* let's hope that works everywhere (mj) */
#define VA_LOCAL_DECL   va_list ap;
#define VA_START(f)     va_start(ap, f)
#define VA_SHIFT(v,t)  ;       /* no-op for ANSI */
#define VA_END          va_end(ap)
#else
#ifdef HAVE_VARARGS_H
#include <varargs.h>
#undef HAVE_STDARGS
#define VA_LOCAL_DECL   va_list ap;
#define VA_START(f)     va_start(ap)          /* f is ignored! */
#define VA_SHIFT(v,t) v = va_arg(ap,t)
#define VA_END                va_end(ap)
#else
XX ** NO VARARGS ** XX
#endif
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "port_after.h"

/*
 * dopr(): poor man's version of doprintf
 */

static void dopr( char *buffer, const char *format, va_list args );
static void fmtstr(  char *value, int ljust, int len, int zpad, int precision );
static void fmtnum(  long value, int base, int dosign,
	int ljust, int len, int zpad, int precision );
static void fmtdouble( int fmt, double value,
	int ljust, int len, int zpad, int precision );
static void dostr( char * );
static char *output;
static void dopr_outch( int c );
static char *end;

/**************************************************************
 * Original:
 * Patrick Powell Tue Apr 11 09:48:21 PDT 1995
 * A bombproof version of doprnt (dopr) included.
 * Sigh.  This sort of thing is always nasty do deal with.  Note that
 * the version here does not include floating point...
 *
 * plp_snprintf() is used instead of sprintf() as it does limit checks
 * for string length.  This covers a nasty loophole.
 *
 * The other functions are there to prevent NULL pointers from
 * causing nast effects.
 **************************************************************/

int vsnprintf(char *str, size_t count, const char *fmt, va_list args)
{
	str[0] = 0;
	end = str+count-1;
	dopr( str, fmt, args );
	if( count>0 ){
		end[0] = 0;
	}
	return(strlen(str));
}

/* VARARGS3 */
#ifdef HAVE_STDARGS
int snprintf (char *str,size_t count,const char *fmt,...)
#else
int snprintf (va_alist) va_dcl
#endif
{
#ifndef HAVE_STDARGS
    char *str;
	size_t count;
    char *fmt;
#endif
    VA_LOCAL_DECL

    VA_START (fmt);
    VA_SHIFT (str, char *);
    VA_SHIFT (count, size_t );
    VA_SHIFT (fmt, char *);
    (void) vsnprintf ( str, count, fmt, ap);
    VA_END;
	return( strlen( str ) );
}

static void dopr( char *buffer, const char *format, va_list args )
{
	int ch;
	long value;
	int longflag = 0;
	char *strvalue;
	int ljust;
	int len;
	int zpad;
	int precision;
	int set_precision;
	double dval;

	output = buffer;
	while( (ch = *format++) ){
		switch( ch ){
		case '%':
			ljust = len = zpad = 0;
			precision = -1; set_precision = 0;
		nextch: 
			ch = *format++;
			switch( ch ){
			case 0:
				dostr( "**end of format**" );
				return;
			case '-': ljust = 1; goto nextch;
			case '.': set_precision = 1; precision = 0; goto nextch;
			case '*': len = va_arg( args, int ); goto nextch;
			case '0': /* set zero padding if len not set */
				if(len==0 && set_precision == 0 ) zpad = '0';
			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
				if( set_precision ){
					precision = precision*10 + ch - '0';
				} else {
					len = len*10 + ch - '0';
				}
				goto nextch;
			case 'l': longflag = 1; goto nextch;
			case 'u': case 'U':
				/*fmtnum(value,base,dosign,ljust,len, zpad, precision) */
				if( longflag ){
					value = va_arg( args, long );
				} else {
					value = va_arg( args, int );
				}
				fmtnum( value, 10,0, ljust, len, zpad, precision ); break;
			case 'o': case 'O':
				/*fmtnum(value,base,dosign,ljust,len, zpad, precision) */
				if( longflag ){
					value = va_arg( args, long );
				} else {
					value = va_arg( args, int );
				}
				fmtnum( value, 8,0, ljust, len, zpad, precision ); break;
			case 'd': case 'D':
				if( longflag ){
					value = va_arg( args, long );
				} else {
					value = va_arg( args, int );
				}
				fmtnum( value, 10,1, ljust, len, zpad, precision ); break;
			case 'x':
				if( longflag ){
					value = va_arg( args, long );
				} else {
					value = va_arg( args, int );
				}
				fmtnum( value, 16,0, ljust, len, zpad, precision ); break;
			case 'X':
				if( longflag ){
					value = va_arg( args, long );
				} else {
					value = va_arg( args, int );
				}
				fmtnum( value,-16,0, ljust, len, zpad, precision ); break;
			case 's':
				strvalue = va_arg( args, char *);
				fmtstr( strvalue,ljust,len, zpad, precision ); break;
			case 'c':
				ch = va_arg( args, int );
				{ char b[2];
					b[0] = ch;
					b[1] = 0;
					fmtstr( b,ljust,len, zpad, precision ); break;
				}
				break;
			case 'f': case 'g':
				dval = va_arg( args, double );
				fmtdouble( ch, dval,ljust,len, zpad, precision ); break;
			case '%': dopr_outch( ch ); continue;
			default:
				dostr(  "???????" );
			}
			longflag = 0;
			break;
		default:
			dopr_outch( ch );
			break;
		}
	}
	*output = 0;
}

/*
 * Format '%[-]len[.precision]s'
 * -   = left justify (ljust)
 * len = minimum length
 * precision = numbers of chars in string to use
 */
static void
fmtstr(  char *value, int ljust, int len, int zpad, int precision )
{
	int padlen, strlen, i;	/* amount to pad */

	if( value == 0 ){
		value = "<NULL>";
	}
	if( precision > 0 ){
		strlen = precision;
	} else {
		for( strlen = 0; value[strlen]; ++ strlen ); /* strlen */
	}
	padlen = len - strlen;
	if( padlen < 0 ) padlen = 0;
	if( ljust ) padlen = -padlen;
	while( padlen > 0 ) {
		dopr_outch( ' ' );
		--padlen;
	}
	/* output characters */
	for( i = 0; i < strlen; ++i ) dopr_outch(value[i]);
	while( padlen < 0 ) {
		dopr_outch( ' ' );
		++padlen;
	}
}

static void
fmtnum(  long value, int base, int dosign, int ljust,
	int len, int zpad, int precision )
{
	int signvalue = 0;
	unsigned long uvalue;
	char convert[20];
	int place = 0;
	int padlen = 0;	/* amount to pad */
	int caps = 0;

	/* DEBUGP(("value 0x%x, base %d, dosign %d, ljust %d, len %d, zpad %d\n",
		value, base, dosign, ljust, len, zpad )); */
	uvalue = value;
	if( dosign ){
		if( value < 0 ) {
			signvalue = '-';
			uvalue = -value; 
		}
	}
	if( base < 0 ){
		caps = 1;
		base = -base;
	}
	do{
		convert[place++] =
			(caps? "0123456789ABCDEF":"0123456789abcdef")
			 [uvalue % (unsigned)base  ];
		uvalue = (uvalue / (unsigned)base );
	}while(uvalue);
	convert[place] = 0;
	padlen = len - place;
	if( padlen < 0 ) padlen = 0;
	if( ljust ) padlen = -padlen;
	/* DEBUGP(( "str '%s', place %d, sign %c, padlen %d\n",
		convert,place,signvalue,padlen)); */
	if( zpad && padlen > 0 ){
		if( signvalue ){
			dopr_outch( signvalue );
			--padlen;
			signvalue = 0;
		}
		while( padlen > 0 ){
			dopr_outch( zpad );
			--padlen;
		}
	}
	while( padlen > 0 ) {
		dopr_outch( ' ' );
		--padlen;
	}
	if( signvalue ) dopr_outch( signvalue );
	while( place > 0 ) dopr_outch( convert[--place] );
	while( padlen < 0 ){
		dopr_outch( ' ' );
		++padlen;
	}
}

static void
fmtdouble( int fmt, double value, int ljust, int len, int zpad, int precision )
{
	char convert[128];
	char fmtstr[128];
	int l;

	if( len == 0 ) len = 10;
	if( len > sizeof(convert) - 10 ){
		len = sizeof(convert) - 10;
	}
	if( precision > sizeof(convert) - 10 ){
		precision = sizeof(convert) - 10;
	}
	if( precision > len ) precision = len;
	strcpy( fmtstr, "%" );
	if( ljust ) strcat(fmtstr, "-" );
	if( len ){
		sprintf( fmtstr+strlen(fmtstr), "%d", len );
	}
	if( precision > 0 ){
		sprintf( fmtstr+strlen(fmtstr), ".%d", precision );
	}
	l = strlen( fmtstr );
	fmtstr[l] = fmt;
	fmtstr[l+1] = 0;
	sprintf( convert, fmtstr, value );
	dostr( convert );
}

static void dostr( char *str )
{
	while(*str) dopr_outch(*str++);
}

static void dopr_outch( int c )
{
	if( end == 0 || output < end ){
		*output++ = c;
	}
}


