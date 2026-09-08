/* Modified by John */

/***********************************************************

Copyright (c) 1987, 1988, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* NOTE: THIS IS NOT A WIDGET!  Rather, this is an interface to a widget.
   It implements policy, and gives a (hopefully) easier-to-use interface
   than just directly making your own form. */


#include <X11/IntrinsicP.h>
#include <X11/Xos.h>
#include <X11/StringDefs.h>
#include <X11/Xmu/Misc.h>

#include <X11/Xaw/XawInit.h>
#include <X11/Xaw/Command.h>	
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Cardinals.h>

#include "TextField.h"

#include "AuthDialogP.h"

/*
 * After we have set the string in the value widget we set the
 * string to a magic value.  So that when a SetValues request is made
 * on the mydialog value we will notice it, and reset the string.
 */
#define MAGIC_VALUE ((char *) 3)

#define streq(a,b) (strcmp( (a), (b) ) == 0)

static XtResource resources[] = {
  { XtNlabel, XtCLabel, XtRString, sizeof(String),
       XtOffsetOf(AuthDialogRec, authdialog.label), XtRString, NULL },
  { "username", XtCValue, XtRString, sizeof(String),
       XtOffsetOf(AuthDialogRec, authdialog.username), XtRString, NULL },
  { XtNicon, XtCIcon, XtRBitmap, sizeof(Pixmap),
       XtOffsetOf(AuthDialogRec, authdialog.icon), XtRImmediate, 0 },
  { XtNcallback, XtCCallback, XtRCallback, sizeof(XtPointer), 
       XtOffsetOf(AuthDialogRec, authdialog.callbacks),XtRCallback, NULL },
};

static void AuthInitialize(), AuthConstraintInitialize();
static void CreateAuthDialogFields();
static void UsernameActivate();
static void PasswordActivate();

static Boolean SetValues();

AuthDialogClassRec authdialogClassRec = {
  { /* core_class fields */
    /* superclass         */    (WidgetClass) &formClassRec,
    /* class_name         */    "AuthDialog",
    /* widget_size        */    sizeof(AuthDialogRec),
    /* class_initialize   */    XawInitializeWidgetSet,
    /* class_part init    */    NULL,
    /* class_inited       */    FALSE,
    /* initialize         */    AuthInitialize,
    /* initialize_hook    */    NULL,
    /* realize            */    XtInheritRealize,
    /* actions            */    NULL,
    /* num_actions        */    0,
    /* resources          */    resources,
    /* num_resources      */    XtNumber(resources),
    /* xrm_class          */    NULLQUARK,
    /* compress_motion    */    TRUE,
    /* compress_exposure  */    TRUE,
    /* compress_enterleave*/    TRUE,
    /* visible_interest   */    FALSE,
    /* destroy            */    NULL,
    /* resize             */    XtInheritResize,
    /* expose             */    XtInheritExpose,
    /* set_values         */    SetValues,
    /* set_values_hook    */    NULL,
    /* set_values_almost  */    XtInheritSetValuesAlmost,
    /* get_values_hook    */    NULL,
    /* accept_focus       */    NULL,
    /* version            */    XtVersion,
    /* callback_private   */    NULL,
    /* tm_table           */    NULL,
    /* query_geometry     */	XtInheritQueryGeometry,
    /* display_accelerator*/	XtInheritDisplayAccelerator,
    /* extension          */	NULL
  },
  { /* composite_class fields */
    /* geometry_manager   */   XtInheritGeometryManager,
    /* change_managed     */   XtInheritChangeManaged,
    /* insert_child       */   XtInheritInsertChild,
    /* delete_child       */   XtInheritDeleteChild,
    /* extension          */   NULL
  },
  { /* constraint_class fields */
    /* subresourses       */   NULL,
    /* subresource_count  */   0,
    /* constraint_size    */   sizeof(AuthDialogConstraintsRec),
    /* initialize         */   AuthConstraintInitialize,
    /* destroy            */   NULL,
    /* set_values         */   NULL,
    /* extension          */   NULL
  },
  { /* form_class fields */
    /* layout             */   XtInheritLayout
  },
  { /* authdialog_class fields */
    /* empty              */   0
  }
};

WidgetClass authdialogWidgetClass = (WidgetClass)&authdialogClassRec;

/* ARGSUSED */
static void AuthInitialize(request, new, args, num_args)
Widget request, new;
ArgList args;
Cardinal *num_args;
{
    AuthDialogWidget dw = (AuthDialogWidget)new;
    Arg arglist[9];
    Cardinal arg_cnt = 0;

    XtSetArg(arglist[arg_cnt], XtNborderWidth, 0); arg_cnt++;
    XtSetArg(arglist[arg_cnt], XtNleft, XtChainLeft); arg_cnt++;

    if (dw->authdialog.icon != (Pixmap)0) {
	XtSetArg(arglist[arg_cnt], XtNbitmap, dw->authdialog.icon); arg_cnt++;
	XtSetArg(arglist[arg_cnt], XtNright, XtChainLeft); arg_cnt++;
	dw->authdialog.iconW =
	    XtCreateManagedWidget( "icon", labelWidgetClass,
				   new, arglist, arg_cnt );
	arg_cnt = 2;
	XtSetArg(arglist[arg_cnt], XtNfromHoriz, dw->authdialog.iconW);
	arg_cnt++;
    } else dw->authdialog.iconW = (Widget)NULL;

    XtSetArg(arglist[arg_cnt], XtNlabel, dw->authdialog.label); arg_cnt++;
    XtSetArg(arglist[arg_cnt], XtNright, XtChainRight); arg_cnt++;

    dw->authdialog.labelW = XtCreateManagedWidget( "label", labelWidgetClass,
						  new, arglist, arg_cnt);

    if (dw->authdialog.iconW != (Widget)NULL &&
	(dw->authdialog.labelW->core.height <
	 dw->authdialog.iconW->core.height)) {
	XtSetArg( arglist[0], XtNheight, dw->authdialog.iconW->core.height );
	XtSetValues( dw->authdialog.labelW, arglist, ONE );
    }
    CreateAuthDialogFields( (Widget) dw);
}

/* ARGSUSED */
static void AuthConstraintInitialize(request, new, args, num_args)
Widget request, new;
ArgList args;
Cardinal *num_args;
{
    AuthDialogWidget dw = (AuthDialogWidget)new->core.parent;
    AuthDialogConstraints constraint =
	(AuthDialogConstraints)new->core.constraints;

    if (!XtIsSubclass(new, commandWidgetClass))	/* if not a button */
	return;					/* then just use defaults */

    constraint->form.left = constraint->form.right = XtChainLeft;
    constraint->form.vert_base = dw->authdialog.passwordW;

    if (dw->composite.num_children > 1) {
	WidgetList children = dw->composite.children;
	Widget *childP;
        for (childP = children + dw->composite.num_children - 1;
	     childP >= children; childP-- ) {
	    if (*childP == dw->authdialog.labelW ||
		*childP == dw->authdialog.passwordW)
	        break;
	    if (XtIsManaged(*childP) &&
		XtIsSubclass(*childP, commandWidgetClass)) {
	        constraint->form.horiz_base = *childP;
		break;
	    }
	}
    }
}

#define ICON 0
#define LABEL 1
#define NUM_CHECKS 2

/* ARGSUSED */
static Boolean SetValues(current, request, new, in_args, in_num_args)
Widget current, request, new;
ArgList in_args;
Cardinal *in_num_args;
{
    AuthDialogWidget w = (AuthDialogWidget)new;
    AuthDialogWidget old = (AuthDialogWidget)current;
    Arg args[5];
    Cardinal num_args;
    int i;
    Boolean checks[NUM_CHECKS];

    for (i = 0; i < NUM_CHECKS; i++)
	checks[i] = FALSE;

    for (i = 0; i < *in_num_args; i++) {
	if (streq(XtNicon, in_args[i].name))
	    checks[ICON] = TRUE;
	if (streq(XtNlabel, in_args[i].name))
	    checks[LABEL] = TRUE;
    }

    if (checks[ICON]) {
	if (w->authdialog.icon != (Pixmap)0) {
	    XtSetArg( args[0], XtNbitmap, w->authdialog.icon );
	    if (old->authdialog.iconW != (Widget)NULL) {
		XtSetValues( old->authdialog.iconW, args, ONE );
	    } else {
		XtSetArg( args[1], XtNborderWidth, 0);
		XtSetArg( args[2], XtNleft, XtChainLeft);
		XtSetArg( args[3], XtNright, XtChainLeft);
		w->authdialog.iconW =
		    XtCreateWidget( "icon", labelWidgetClass,
				    new, args, FOUR );
		((AuthDialogConstraints)
		 w->authdialog.labelW->core.constraints)->
		     form.horiz_base = w->authdialog.iconW;
		XtManageChild(w->authdialog.iconW);
	    }
	} else if (old->authdialog.icon != (Pixmap)0) {
	    ((AuthDialogConstraints)
	     w->authdialog.labelW->core.constraints)->
		 form.horiz_base = (Widget)NULL;
	    XtDestroyWidget(old->authdialog.iconW);
	    w->authdialog.iconW = (Widget)NULL;
	}
    }

    if ( checks[LABEL] ) {
        num_args = 0;
        XtSetArg( args[num_args], XtNlabel, w->authdialog.label ); num_args++;
	if (w->authdialog.iconW != (Widget)NULL &&
	    (w->authdialog.labelW->core.height <=
	     w->authdialog.iconW->core.height)) {
	  XtSetArg(args[num_args], XtNheight,
		   w->authdialog.iconW->core.height);
	    num_args++;
	}
	XtSetValues( w->authdialog.labelW, args, num_args );
    }

    if ( w->authdialog.username != old->authdialog.username ) {
      TextFieldSetString(w->authdialog.usernameW, w->authdialog.username);
      w->authdialog.username = MAGIC_VALUE;
    }

    return False;
}

/*	Function Name: CreateAuthDialogFields
 *	Description: Creates the authdialog widgets username and password
 *                   widgets.
 *	Arguments: w - the authdialog widget.
 *	Returns: none.
 *
 *	must be called only when w->authdialog.value is non-nil.
 */

static void
CreateAuthDialogFields(w)
Widget w;
{
    AuthDialogWidget dw = (AuthDialogWidget) w;    

    dw->authdialog.usernameW =
	XtVaCreateWidget("username",
			 textfieldWidgetClass, w,
			 XtNstring, dw->authdialog.username,
			 XtNfromVert, dw->authdialog.labelW,
			 XtNleft, XtChainLeft,
			 XtNright, XtChainRight,
			 NULL);
    XtAddCallback(dw->authdialog.usernameW, XtNactivateCallback,
		  UsernameActivate, dw);

    dw->authdialog.passwordW =
	XtVaCreateWidget("password",
			 textfieldWidgetClass, w,
			 XtNecho, False,
			 XtNfromVert, dw->authdialog.usernameW,
			 XtNleft, XtChainLeft,
			 XtNright, XtChainRight,
			 NULL);
    XtAddCallback(dw->authdialog.passwordW, XtNactivateCallback,
		  PasswordActivate, dw);

    /* if the value widget is being added after buttons,
     * then the buttons need new layout constraints.
     */
    if (dw->composite.num_children > 1) {
	WidgetList children = dw->composite.children;
	Widget *childP;
        for (childP = children + dw->composite.num_children - 1;
	     childP >= children; childP-- ) {
	    if (*childP == dw->authdialog.labelW ||
		*childP == dw->authdialog.usernameW ||
		*childP == dw->authdialog.passwordW)
		continue;
	    if (XtIsManaged(*childP) &&
		XtIsSubclass(*childP, commandWidgetClass)) {
	        ((AuthDialogConstraints)(*childP)->core.constraints)->
		    form.vert_base = dw->authdialog.passwordW;
	    }
	}
    }

    XtManageChild(dw->authdialog.usernameW);
    XtManageChild(dw->authdialog.passwordW);

/* 
 * Value widget gets the keyboard focus.
 */

    XtSetKeyboardFocus(w, dw->authdialog.usernameW);
    dw->authdialog.username = MAGIC_VALUE;
}


void
#if NeedFunctionPrototypes
AuthDialogAddButton(Widget authdialog, _Xconst char* name, XtCallbackProc function,
		   XtPointer param)
#else
AuthDialogAddButton(authdialog, name, function, param)
Widget authdialog;
String name;
XtCallbackProc function;
XtPointer param;
#endif
{
/*
 * Correct Constraints are all set in AuthConstraintInitialize().
 */
    Widget button;

    button = XtCreateManagedWidget( name, commandWidgetClass, authdialog, 
				    (ArgList)NULL, (Cardinal)0 );

    if (function != NULL)	/* don't add NULL callback func. */
        XtAddCallback(button, XtNcallback, function, param);
}


char *
#if NeedFunctionPrototypes
AuthDialogGetUsername(Widget w)
#else
AuthDialogGetUsername(w)
Widget w;
#endif
{
    return(TextFieldGetString(((AuthDialogWidget)w)->authdialog.usernameW));
}

char *
#if NeedFunctionPrototypes
AuthDialogGetPassword(Widget w)
#else
AuthDialogGetPassword(w)
Widget w;
#endif
{
    return(TextFieldGetString(((AuthDialogWidget)w)->authdialog.passwordW));
}

/*
 * UsernameActivate
 */
static void
UsernameActivate(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  AuthDialogWidget dw;

  dw = (AuthDialogWidget)XtParent(w);
  XtSetKeyboardFocus((Widget)dw, dw->authdialog.passwordW);

  return;
}

/*
 * PasswordActivate
 */
static void
PasswordActivate(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  AuthDialogWidget dw;

  dw = (AuthDialogWidget)XtParent(w);
  XtCallCallbackList(XtParent(w), dw->authdialog.callbacks, (XtPointer) NULL);

  return;
}
