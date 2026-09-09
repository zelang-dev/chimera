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
#include <X11/Xaw/Cardinals.h>

#include "TextField.h"

#include "MyDialogP.h"

static char defaultTranslations[] =
"<Key>Return:	MyPressReturn() \n\
 Ctrl<Key>m:	MyPressReturn() \n";

static XtResource resources[] = {
  { XtNlabel, XtCLabel, XtRString, sizeof(String),
       XtOffsetOf(MyDialogRec, mydialog.label), XtRString, NULL },
  {XtNcallback, XtCCallback, XtRCallback, sizeof(XtPointer), 
     XtOffsetOf(MyDialogRec, mydialog.callbacks),XtRCallback,(XtPointer)NULL},
};

static void Initialize(), ConstraintInitialize(), MyPressReturn();
static Boolean SetValues();

static XtActionsRec actionsList[] =
{
  { "MyPressReturn", MyPressReturn }
};

MyDialogClassRec mydialogClassRec = {
  { /* core_class fields */
    /* superclass         */    (WidgetClass) &formClassRec,
    /* class_name         */    "MyDialog",
    /* widget_size        */    sizeof(MyDialogRec),
    /* class_initialize   */    XawInitializeWidgetSet,
    /* class_part init    */    NULL,
    /* class_inited       */    FALSE,
    /* initialize         */    Initialize,
    /* initialize_hook    */    NULL,
    /* realize            */    XtInheritRealize,
    /* actions            */    actionsList,
    /* num_actions        */    XtNumber(actionsList),
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
    /* constraint_size    */   sizeof(MyDialogConstraintsRec),
    /* initialize         */   ConstraintInitialize,
    /* destroy            */   NULL,
    /* set_values         */   NULL,
    /* extension          */   NULL
  },
  { /* form_class fields */
    /* layout             */   XtInheritLayout
  },
  { /* mydialog_class fields */
    /* empty              */   0
  }
};

WidgetClass mydialogWidgetClass = (WidgetClass)&mydialogClassRec;

/* ARGSUSED */
static void Initialize(request, new, args, num_args)
Widget request, new;
ArgList args;
Cardinal *num_args;
{
    MyDialogWidget dw = (MyDialogWidget)new;

    dw->mydialog.messageW =
	XtVaCreateManagedWidget("mydialog_message",
				textfieldWidgetClass,
				new,
				XtNstring, dw->mydialog.label,
				XtNdisplayCaret, False,
				XtNeditable, False,
				XtNborderWidth, 0,
				XtNleft, XtChainLeft,
				XtNright, XtChainRight,
				NULL);


    dw->mydialog.valueW = XtVaCreateWidget("mydialog_value",
					   textfieldWidgetClass,
					   new,
					   XtNfromVert, dw->mydialog.messageW,
					   XtNleft, XtChainLeft,
					   XtNright, XtChainRight,
					   NULL);

    /*
     * if the value widget is being added after buttons,
     * then the buttons need new layout constraints.
     */
    if (dw->composite.num_children > 1) {
      WidgetList children = dw->composite.children;
      Widget *childP;

      for (childP = children + dw->composite.num_children - 1;
	   childP >= children; childP-- ) {
	if (*childP == dw->mydialog.messageW ||
	    *childP == dw->mydialog.messageW)
	    continue;
	if (XtIsManaged(*childP) &&
	    XtIsSubclass(*childP, commandWidgetClass)) {
	  ((MyDialogConstraints)(*childP)->core.constraints)->
	      form.vert_base = dw->mydialog.messageW;
	}
      }
    }
    
    XtOverrideTranslations(dw->mydialog.valueW,
                           XtParseTranslationTable(defaultTranslations));
    
    XtManageChild(dw->mydialog.valueW);
    
    XtSetKeyboardFocus(new, dw->mydialog.valueW);
  }

/* ARGSUSED */
static void ConstraintInitialize(request, new, args, num_args)
Widget request, new;
ArgList args;
Cardinal *num_args;
{
    MyDialogWidget dw = (MyDialogWidget)new->core.parent;
    MyDialogConstraints constraint = (MyDialogConstraints)new->core.constraints;

    if (!XtIsSubclass(new, commandWidgetClass))	/* if not a button */
	return;					/* then just use defaults */

    constraint->form.left = constraint->form.right = XtChainLeft;
    constraint->form.vert_base = dw->mydialog.valueW;

    if (dw->composite.num_children > 1) {
	WidgetList children = dw->composite.children;
	Widget *childP;
        for (childP = children + dw->composite.num_children - 1;
	     childP >= children; childP-- ) {
	    if (*childP == dw->mydialog.messageW ||
		*childP == dw->mydialog.valueW)
	        break;
	    if (XtIsManaged(*childP) &&
		XtIsSubclass(*childP, commandWidgetClass)) {
	        constraint->form.horiz_base = *childP;
		break;
	    }
	}
    }
}

/* ARGSUSED */
static Boolean
SetValues(current, request, new, in_args, in_num_args)
Widget current, request, new;
ArgList in_args;
Cardinal *in_num_args;
{
    MyDialogWidget w = (MyDialogWidget)new;
    Boolean label_change = False;
    int i;

    for (i = 0; i < *in_num_args; i++) {
	if (strcmp(XtNlabel, in_args[i].name) == 0) label_change = True;
    }

    if (label_change) {
        TextFieldSetString(w->mydialog.messageW, w->mydialog.label);
    }

    return False;
}

Widget
#if NeedFunctionPrototypes
MyDialogAddButton(Widget mydialog, _Xconst char* name, XtCallbackProc function,
		   XtPointer param)
#else
MyDialogAddButton(mydialog, name, function, param)
Widget mydialog;
String name;
XtCallbackProc function;
XtPointer param;
#endif
{
/*
 * Correct Constraints are all set in ConstraintInitialize().
 */
    Widget button;

    button = XtVaCreateManagedWidget(name,
				     commandWidgetClass, mydialog, NULL);

    if (function != NULL)	/* don't add NULL callback func. */
        XtAddCallback(button, XtNcallback, function, param);
    return(button);
}


void
#if NeedFunctionPrototypes
MyDialogSetMessage(Widget w, char *message)
#else
MyDialogSetMessage(w, message)
Widget w;
char *message;
#endif
{
    TextFieldSetString(((MyDialogWidget)w)->mydialog.messageW, message);
    return;
}

void
#if NeedFunctionPrototypes
MyDialogSetValue(Widget w, char *value)
#else
MyDialogSetValue(w, value)
Widget w;
char *value;
#endif
{
    TextFieldSetString(((MyDialogWidget)w)->mydialog.valueW, value);
    return;
}

char *
#if NeedFunctionPrototypes
MyDialogGetValue(Widget w)
#else
MyDialogGetValue(w)
Widget w;
#endif
{
    return(TextFieldGetString(((MyDialogWidget)w)->mydialog.valueW));
}

/*
 * MyPressReturn
 */
static void
MyPressReturn(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
  MyDialogWidget dw;

  dw = (MyDialogWidget)XtParent(w);
  XtCallCallbackList(XtParent(w), dw->mydialog.callbacks, (XtPointer) NULL);

  return;
}
