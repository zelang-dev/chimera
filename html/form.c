/*
 * form.c
 *
 * libhtml - HTML->X renderer
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
#include "port_before.h"

#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifndef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/AsciiText.h>

#include "port_after.h"

#include "TextField.h"

#include "html.h"

typedef struct InputStateP InputState;
typedef struct FormStateP FormState;
typedef struct RadioStateP RadioState;
typedef struct OptionStateP OptionState;

struct OptionStateP
{
  char   *value;
  bool   selected;
  char   *text;
  Widget sme;
};

typedef enum
{
  INPUT_TEXT,
  INPUT_PASSWORD,
  INPUT_CHECKBOX,
  INPUT_RADIO,
  INPUT_SUBMIT,
  INPUT_RESET,
  INPUT_RANGE,
  INPUT_AUDIO,
  INPUT_FILE,
  INPUT_SCRIBBLE,
  INPUT_HIDDEN,
  INPUT_IMAGE,
  INPUT_TEXTAREA,
  INPUT_SELECT
} InputType;

struct InputStateP
{
  /* generic stuff */
  InputType   id;
  Widget      w;                    /* the widget */
  MLElement   p;                    /* input tag */
  FormState   *fs;
  HTMLBox     box;
  char        *name;

  /* for image input */
  HTMLInline  img;

  /* select stuff */
  bool        multi;                /* multi-select? */
  GList       oplist;
  ChimeraTimeOut iot;
  int         x, y, button;         /* coordinates if image clicked */
};

struct FormStateP
{
  MLElement p;                      /* form tag info */
  GList     rslist;                 /* radio list */
  GList     islist;
  GList     oplist;                 /* building option list */
  HTMLInfo  li;
  char      *action;
};

/*
 *
 * Private function prototypes
 *
 */

static InputState *CreateInputState _ArgProto((FormState *,
					       MLElement, InputType));
static HTMLBox CreateInputBox _ArgProto((HTMLInfo, HTMLEnv, InputState *,
					 unsigned int, unsigned int));

static void DestroyInput _ArgProto((HTMLInfo, HTMLBox));
static void CreateHidden _ArgProto((FormState *, HTMLEnv, MLElement));
static void CreateText _ArgProto((FormState *, HTMLEnv, MLElement, InputType));
static void CreateCheckbox _ArgProto((FormState *, HTMLEnv, MLElement));
static void CreateRadio _ArgProto((FormState *, HTMLEnv, MLElement));
static void CreateImage _ArgProto((FormState *, HTMLEnv, MLElement));
static void CreateCommand _ArgProto((FormState *, HTMLEnv,
				     MLElement, InputType));

static char *NameValueToURLEncoded _ArgProto((MemPool,
					      char **, char **, int));

static void MakeSelectWidget _ArgProto((HTMLInfo, FormState *, HTMLEnv));
static char *GetAsciiText _ArgProto((Widget));
static XFontStruct *GetFont _ArgProto((Widget));
static void SetupInput _ArgProto((HTMLInfo, HTMLBox));

static bool FormImageSelectCallback _ArgProto((void *, int, int, char *));
bool FormImageMotionCallback _ArgProto((void *, int, int));

static void SubmitCallback _ArgProto((Widget, XtPointer, XtPointer));
static void HandleSubmit _ArgProto((HTMLInfo, FormState *, InputState *,
				    char *));

/*
 * Functions
 */ 
static XFontStruct *
GetFont(w)
Widget w;
{
  XFontStruct *font;
  XtVaGetValues(w, XtNfont, &font, NULL);
  return(font);
}

static char *
GetAsciiText(w)
Widget w;
{
  char *s;

  XtVaGetValues(w, XtNstring, &s, NULL);
  return(s);
}

/*
 * NameValueToURLEncoded
 */
static char *
NameValueToURLEncoded(mp, names, values, count)
MemPool mp;
char **names;
char **values;
int count;
{
  int i;
  char *finfo;
  char *sep;
  char *n, *v;
  size_t flen, alen;
  char *format = "%s%s=%s";
  char *nf;

  sep = "";
  finfo = "";
  flen = 0;
  for (i = 0; i < count; i++)
  {
    if (names[i] == NULL) continue;

    n = URLEscape(mp, names[i], 0);
    if (values[i] != NULL) v = URLEscape(mp, values[i], 1);
    else v = "";
      
    alen = flen + strlen(n) + strlen(v) + strlen(sep) + strlen(format) + 1;

    nf = (char *)MPGet(mp, alen);
    snprintf (nf, alen, "%s%s%s=%s", finfo, sep, n, v);
    finfo = nf;

    sep = "&";
    flen = strlen(finfo);
  }

  return(finfo);
}

/*
 * HandleSubmit
 */
static void
HandleSubmit(li, fs, ci, action)
HTMLInfo li;
FormState *fs;
InputState *ci;
char *action;
{
  InputState *xi;
  int i;
  char **names;
  char **values;
  char *n;
  Boolean checked;
  int count;
  OptionState *co;
  ChimeraRequest *wr;

  if (fs->action == NULL) return;

  wr = RequestCreate(li->cres, fs->action, li->burl);

  if ((wr->input_method = MLFindAttribute(fs->p, "method")) != NULL)
  {
    wr->input_method = MPStrDup(wr->mp, wr->input_method);
  }
  wr->input_type = MPStrDup(wr->mp, "application/x-www-form-urlencoded");

  count = 0;
  for (xi = (InputState *)GListGetHead(fs->islist); xi != NULL;
       xi = (InputState *)GListGetNext(fs->islist))
  {
    count += 2;
    if (xi->id == TAG_SELECT)
    {
      for (co = (OptionState *)GListGetHead(xi->oplist); co != NULL;
	   co = (OptionState *)GListGetNext(xi->oplist))
      {
        if (co->selected) count++;
      }
    }
  }

  if (count == 0)
  {
    fprintf (stderr, "SubmitCallback error.\n");
    return;
  }

  names = (char **)MPGet(wr->mp, sizeof(char **) * count);
  values = (char **)MPGet(wr->mp, sizeof(char **) * count);

  i = 0;
  for (xi = (InputState *)GListGetHead(fs->islist); xi != NULL;
       xi = (InputState *)GListGetNext(fs->islist))
  {
    names[i] = NULL;
    values[i] = NULL;
	
    if ((n = MLFindAttribute(xi->p, "name")) == NULL) continue;

    if (xi->id == INPUT_TEXT || xi->id == INPUT_PASSWORD)
    {
      names[i] = n;
      values[i] = TextFieldGetString(xi->w);
      if (values[i] != NULL) values[i] = MPStrDup(wr->mp, values[i]);
      i++;
    }
    else if (xi->id == INPUT_TEXTAREA)
    {
      names[i] = n;
      values[i] = GetAsciiText(xi->w);
      if (values[i] != NULL) values[i] = MPStrDup(wr->mp, values[i]);
      i++;
    }
    else if (xi->id == INPUT_CHECKBOX || xi->id == INPUT_RADIO)
    {
      XtVaGetValues(xi->w, XtNstate, &checked, NULL); 
      if (checked)
      {
	names[i] = n;
	values[i] = MLFindAttribute(xi->p, "value");
	i++;
      }
    }
    else if (xi->id == INPUT_HIDDEN)
    {
      names[i] = n;
      values[i] = MLFindAttribute(xi->p, "value");
      i++;
    }
    else if (xi->id == INPUT_SELECT)
    {
      for (co = (OptionState *)GListGetHead(xi->oplist); co != NULL;
	   co = (OptionState *)GListGetNext(xi->oplist))
      {
        if (co->selected)
        {
	  names[i] = n;
	  if (co->value != NULL) values[i] = co->value;
	  else values[i] = co->text;
          i++;
        }
      }
    }
  }

  /*
   * Add data from the submit button/image
   */
  if (ci->id == INPUT_IMAGE)
  {
    if ((n = MLFindAttribute(ci->p, "name")) != NULL)
    {
      names[i] = (char *)MPGet(wr->mp, strlen(n) + strlen(".x") + 1);
      strcpy(names[i], n);
      strcat(names[i], ".x");
      values[i] = (char *)MPGet(wr->mp, 25);
      snprintf (values[i], 25, "%d", ci->x);
      i++;
      names[i] = (char *)MPGet(wr->mp, strlen(n) + strlen(".y") + 1);
      strcpy(names[i], n);
      strcat(names[i], ".y");
      values[i] = (char *)MPGet(wr->mp, 25);
      snprintf (values[i], 25, "%d", ci->y);
      i++;
    }
  }
  else if (ci->id == INPUT_SUBMIT)
  {
    if ((n = MLFindAttribute(ci->p, "name")) != NULL)
    {
      names[i] = n;
      values[i] = MLFindAttribute(ci->p, "value");
      i++;
    }
  }
  else
  {
    fprintf (stderr, "UNKNOWN SUBMIT INPUT\n");
    MPDestroy(wr->mp);
    return;
  }

  if (i > 0)
  {
    wr->input_data = NameValueToURLEncoded(wr->mp, names, values, i);
    wr->input_len = strlen((char *)wr->input_data);
  }
  else
  {
    wr->input_data = NULL;
    wr->input_len = 0;
  }

  RenderAction(li->wn, wr, action);

  return;
}
/*
 * SubmitCallback
 */
static void
SubmitCallback(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  InputState *ci = (InputState *)cldata;
  FormState *fs = ci->fs;
  HTMLInfo li = fs->li;

  HandleSubmit(li, fs, ci, "open");

  return;
}

/*
 * ResetCallback
 *
 * Do something about this later.
 */
static void
ResetCallback(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  return;
}

/*
 * SmeCallback
 */
static void
SmeCallback(w, cldata, cbdata)
Widget w;
XtPointer cldata, cbdata;
{
  InputState *ci = (InputState *)cldata;
  String str;
  OptionState *co;

  for (co = (OptionState *)GListGetHead(ci->oplist); co != NULL;
       co = (OptionState *)GListGetNext(ci->oplist))
  {
    if (co->sme == w)
    { 
      if (co->selected)
      {
        if (ci->multi) co->selected = false;
      }
      else
      {
        co->selected = true;
        XtVaGetValues(w, XtNlabel, &str, NULL);
        XtVaSetValues(ci->w, XtNlabel, str, NULL);
      }
    }
    else if (!ci->multi) co->selected = false;
  }

  return;
}

/*
 * SetupInput
 */
static void
SetupInput(li, box)
HTMLInfo li;
HTMLBox box;
{
  InputState *ci = (InputState *)box->closure;

/*
  if (ci->img != NULL) HTMLSetInlinePosition(ci->img, box->x, box->y);
*/
  if (ci->w != NULL)
  {
    XtConfigureWidget(ci->w, (Position)box->x, (Position)box->y,
		      (Dimension)(box->width - 10),
		      (Dimension)(box->height - 5),
		      (Dimension)1);
    XtManageChild(ci->w);
  }

  return;
}

/*
 * CreateInputBox
 */
static HTMLBox
CreateInputBox(li, env, ci, width, height)
HTMLInfo li;
HTMLEnv env;
InputState *ci;
unsigned int width, height;
{
  HTMLBox box;

  box = HTMLCreateBox(li, env);
  box->setup = SetupInput;
  box->destroy = DestroyInput;
  box->width = width + 10;
  box->height = height + 5;
  box->baseline = height;
  box->closure = ci;

  HTMLEnvAddBox(li, env, box);

  return(box);
}

/*
 * CreateInputState
 */
static InputState *
CreateInputState(fs, p, type)
FormState *fs;
MLElement p;
InputType type;
{
  InputState *ci;

  ci = (InputState *)MPCGet(fs->li->mp, sizeof(InputState));
  ci->p = p;
  ci->id = type;
  ci->fs = fs;

  GListAddTail(fs->islist, ci);
  
  return(ci);
}

/*
 * DestroyInput
 */
static void
DestroyInput(li, box)
HTMLInfo li;
HTMLBox box;
{
  InputState *ci = (InputState *)box->closure;

  if (ci->id == INPUT_RADIO) GListRemoveItem(ci->fs->rslist, ci);

  GListRemoveItem(ci->fs->islist, ci);

  if (ci->w != NULL) XtDestroyWidget(ci->w);
  if (ci->img != NULL) HTMLInlineDestroy(ci->img);
  if (ci->iot != NULL) TimeOutDestroy(ci->iot);

  return;
}

/*
 * CreateHidden
 */
static void
CreateHidden(fs, env, p)
FormState *fs;
HTMLEnv env;
MLElement p;
{
  CreateInputState(fs, p, INPUT_HIDDEN);
  return;
}

/*
 * CreateText
 */
static void
CreateText(fs, env, p, type)
FormState *fs;
HTMLEnv env;
MLElement p;
InputType type;
{
  Widget w;
  int width, height;
  XFontStruct *font;
  char *value;
  HTMLBox box;
  InputState *ci;
  Boolean echo;
  char *name;

  if (type == INPUT_PASSWORD)
  {
    name = "password";
    echo = False;
  }
  else
  {
    name = "text";
    echo = True;
  }

  w = XtVaCreateManagedWidget(name,
			      textfieldWidgetClass, fs->li->widget,
			      XtNecho, echo,
			      XtNlength, 500,
			      NULL);

  if ((value = MLFindAttribute(p, "value")) != NULL)
  {
    value = MPStrDup(fs->li->mp, value);
    HTMLStringSpacify(value, strlen(value));
    TextFieldSetString(w, value);
  }
  else
  {
    TextFieldSetString(w, "");
  }

  if ((font = GetFont(w)) == NULL) height = 20;
  else height = font->ascent + font->descent + 5;
  /* Addition by LRD.  tweaked to john's weird style */
  if ((width = MLAttributeToInt(p, "size")) > 0)
  {
    if (width > 200) width = 200;
  }
  else width = 25;

  if (font == NULL) width = width * 8;
  else width = width * XTextWidth(font, "0", 1);
  /* End: replaces "width = 100;" */

  ci = CreateInputState(fs, p, type);
  box = CreateInputBox(fs->li, env, ci, width, height);
  ci->w = w;

  return;
}

/*
 * CreateCheckbox
 */
static void
CreateCheckbox(fs, env, p)
FormState *fs;
HTMLEnv env;
MLElement p;
{
  Widget w;
  Boolean state;
  HTMLBox box;
  InputState *ci;

  if (MLFindAttribute(p, "checked") != NULL) state = True;
  else state = False;

  w = XtVaCreateWidget("checkbox",
		       toggleWidgetClass, fs->li->widget,
		       XtNstate, state,
		       XtNlabel, " ",
		       NULL);

  ci = CreateInputState(fs, p, INPUT_CHECKBOX);
  box = CreateInputBox(fs->li, env, ci, 15, 15);
  ci->w = w;

  return;
}

/*
 * CreateRadio
 */
static void
CreateRadio(fs, env, p)
FormState *fs;
HTMLEnv env;
MLElement p;
{
  Widget w;
  Boolean state;
  char *name;
  InputState *ci, *peer;
  HTMLBox box;

  if (MLFindAttribute(p, "checked") != NULL) state = True;
  else state = False;

  if ((name = MLFindAttribute(p, "name")) == NULL) name = "bozo";

  for (peer = (InputState *)GListGetHead(fs->rslist); peer != NULL;
       peer = (InputState *)GListGetNext(fs->rslist))
  {
    if (strlen(name) == strlen(peer->name) &&
	strcasecmp(name, peer->name) == 0) break;
  }

  w = XtVaCreateWidget("radio",
		       toggleWidgetClass, fs->li->widget,
		       XtNstate, state,
		       XtNlabel, " ",
		       NULL);

  ci = CreateInputState(fs, p, INPUT_RADIO);
  box = CreateInputBox(fs->li, env, ci, 15, 15);
  ci->w = w;
  ci->name = (char *)MPStrDup(fs->li->mp, name);

  if (peer == NULL)
  {
    GListAddHead(fs->rslist, ci);
    XtVaSetValues(w, XtNstate, True, NULL);
  }
  else XtVaSetValues(w, XtNradioGroup, peer->w, NULL);

  return;
}

/*
 * CreateCommand
 */
static void
CreateCommand(fs, env, p, type)
FormState *fs;
HTMLEnv env;
MLElement p;
InputType type;
{
  Arg args[10];
  int argcnt;
  int width, height;
  char *value;
  XFontStruct *font;
  Widget w;
  InputState *ci;
  char *name;
  HTMLBox box;

  argcnt = 0;
  name = type == INPUT_SUBMIT ? "submit":"reset";
  if ((value = MLFindAttribute(p, "value")) != NULL)
  {
    value = MPStrDup(fs->li->mp, value);
    HTMLStringSpacify(value, strlen(value));
    XtSetArg(args[argcnt], XtNlabel, value); argcnt++;
  }
  w = XtCreateWidget(name,
		     commandWidgetClass, fs->li->widget,
		     args, argcnt);

  XtVaGetValues(w, XtNlabel, &value, NULL);

  XtVaGetValues(w, XtNfont, &font, NULL);
  if (font == NULL)
  {
    width = 50;
    height = 20;
  }
  else
  {
    width = XTextWidth(font, value, strlen(value)) + 10;
    height = font->ascent + font->descent + 3;
  }
  ci = CreateInputState(fs, p, type);
  box = CreateInputBox(fs->li, env, ci, width, height);
  ci->w = w;
  
  if (type == INPUT_SUBMIT)
  {
    XtAddCallback(w, XtNcallback, SubmitCallback, (XtPointer)ci);
  }
  else if (type == INPUT_RESET)
  {
    XtAddCallback(w, XtNcallback, ResetCallback, (XtPointer)ci);
  }

  return;
}

/*
 *
 * Public functions
 *
 */

/*
 * HandleFormEnd
 */
void
HTMLFormEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLAddLineBreak(li, env);
  return;
}

/*
 * HandleFormBegin
 */
void
HTMLFormBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  FormState *fs;

  HTMLAddLineBreak(li, env);
  
  fs = (FormState *)MPCGet(li->mp, sizeof(FormState));
  fs->p = p;
  fs->li = li;
  fs->islist = GListCreateX(li->mp);
  fs->rslist = GListCreateX(li->mp);
  fs->action = MLFindAttribute(p, "action");
  
  env->closure = fs;

  return;
}

/*
 * HTMLInput
 */
void
HTMLInput(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  char *type;
  HTMLEnv fenv;
  FormState *fs;

  if ((fenv = HTMLGetIDEnv(env, TAG_FORM)) == NULL) return;
  fs = (FormState *)fenv->closure;

  type = MLFindAttribute(p, "type");
  if (type == NULL || strcasecmp(type, "text") == 0)
  {
    CreateText(fs, env, p, INPUT_TEXT);
  }
  else if (strcasecmp(type, "checkbox") == 0) CreateCheckbox(fs, env, p);
  else if (strcasecmp(type, "hidden") == 0) CreateHidden(fs, env, p);
  else if (strcasecmp(type, "image") == 0) CreateImage(fs, env, p);
  else if (strcasecmp(type, "password") == 0) 
  {
    CreateText(fs, env, p, INPUT_PASSWORD);
  }
  else if (strcasecmp(type, "radio") == 0) CreateRadio(fs, env, p);
  else if (strcasecmp(type, "reset") == 0)
  {
    CreateCommand(fs, env, p, INPUT_RESET);
  }
  else if (strcasecmp(type, "submit") == 0)
  {
    CreateCommand(fs, env, p, INPUT_SUBMIT);
  }

  return;
}

/*
 * HTMLTextareaEnd
 */
void
HTMLTextareaEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv fenv;
  FormState *fs;
  char *tatext;
  InputState *ci;
  int width, height;
  XFontStruct *font;
  int rows, cols;
  Widget w;
  HTMLBox box;
  HTMLObject obj;

  if ((fenv = HTMLGetIDEnv(env, TAG_FORM)) == NULL) return;
  fs = (FormState *)fenv->closure;

  if ((tatext = HTMLGetEnvText(li->mp, env)) == NULL) tatext = NULL;

  obj = (HTMLObject)GListGetHead(env->slist);
  
  w = XtVaCreateWidget("textarea",
                       asciiTextWidgetClass, li->widget,
                       XtNeditType, XtEtextEdit,
                       XtNecho, True,
		       XtNdisplayCaret, True,
		       XtNstring, tatext,
                       NULL);
  font = GetFont(w);

  if (font == NULL) height = 20;
  else height = font->ascent + font->descent + 2;
  width = XTextWidth(font, "X", 1);
  
  if ((rows = MLAttributeToInt(obj->o.p, "rows")) <= 0) rows = 5;
  height *= rows;
  
  if ((cols = MLAttributeToInt(obj->o.p, "cols")) <= 0) cols = 20;
  width *= cols;
  
  ci = CreateInputState(fs, obj->o.p, INPUT_TEXTAREA);
  box = CreateInputBox(fs->li, env, ci, width, height);
  ci->w = w;
  
  return;
}

/*
 *
 * SELECT input
 *
 */

/*
 * MakeSelectWidget
 */
static void
MakeSelectWidget(li, fs, env)
HTMLInfo li;
FormState *fs;
HTMLEnv env;
{
  InputState *ci;
  int width, twidth;
  XFontStruct *font;
  Widget w, smw;
  HTMLBox box;
  OptionState *c;
  GList oplist = fs->oplist;
  HTMLObject obj;

  fs->oplist = NULL;

  if (GListEmpty(oplist)) return;

  for (c = (OptionState *)GListGetHead(oplist); c != NULL;
       c = (OptionState *)GListGetNext(oplist))
  {
    if (c->selected) break;
  }
  if (c == NULL)
  {
    c = (OptionState *)GListGetHead(oplist);
    c->selected = true;
  }
  w = XtVaCreateWidget("menubutton",
		       menuButtonWidgetClass, li->widget,
		       XtNmenuName, "simplemenu",
		       XtNlabel, c->text,
		       NULL);
  
  smw = XtVaCreatePopupShell("simplemenu",
			     simpleMenuWidgetClass, w,
			     XtNwidth, 0,
			     XtNheight, 0,
			     NULL);
  
  XtVaGetValues(w, XtNfont, &font, NULL);
  width = 0;
  for (c = (OptionState *)GListGetHead(oplist); c != NULL;
       c = (OptionState *)GListGetNext(oplist))
  {
    c->sme = XtVaCreateManagedWidget(c->text,
				     smeBSBObjectClass, smw,
				     NULL);
    
    twidth = XTextWidth(font, c->text, strlen(c->text));
    if (width < twidth) width = twidth;
  }

  obj = (HTMLObject)GListGetHead(env->slist);
  ci = CreateInputState(fs, obj->o.p, INPUT_SELECT);
  box = CreateInputBox(fs->li, env, ci,
		       width + 10, font->ascent + font->descent + 4);
  ci->w = w;
  ci->oplist = oplist;

  if (MLFindAttribute(obj->o.p, "multiple") != NULL) ci->multi = true;

  for (c = (OptionState *)GListGetHead(oplist); c != NULL;
       c = (OptionState *)GListGetNext(oplist))
  {
    XtAddCallback(c->sme, XtNcallback, SmeCallback, (XtPointer)ci);
  }
  
  return;
}

/*
 * HTMLOptionEnd
 */
void
HTMLOptionEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv fenv;
  FormState *formstate;
  OptionState *n;
  HTMLObject obj;

  if ((fenv = HTMLGetIDEnv(env, TAG_FORM)) == NULL) return;
  formstate = (FormState *)fenv->closure;

  obj = (HTMLObject)GListGetHead(env->slist);

  n = (OptionState *)MPCGet(li->mp, sizeof(OptionState));
  if ((n->text = HTMLGetEnvText(li->mp, env)) == NULL) n->text = "";
  n->value = MLFindAttribute(obj->o.p, "value");
  if (MLFindAttribute(obj->o.p, "selected") != NULL) n->selected = true;
  GListAddTail(formstate->oplist, n);
  
  return;
}

/*
 * HTMLSelectBegin
 */
void
HTMLSelectBegin(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv fenv;
  FormState *fs;

  if ((fenv = HTMLGetIDEnv(env, TAG_FORM)) == NULL) return;
  fs = (FormState *)fenv->closure;
  if (fs->oplist == NULL) fs->oplist = GListCreateX(li->mp);

  return;
}

/*
 * HTMLSelectEnd
 */
void
HTMLSelectEnd(li, env, p)
HTMLInfo li;
HTMLEnv env;
MLElement p;
{
  HTMLEnv fenv;
  FormState *fs;

  if ((fenv = HTMLGetIDEnv(env, TAG_FORM)) == NULL) return;
  fs = (FormState *)fenv->closure;
  MakeSelectWidget(li, fs, env);

  return;
}

/*
 *
 * Image Input stuff
 *
 */

/*
 * FormImageSelectCallback
 */
static bool
FormImageSelectCallback(closure, x, y, action)
void *closure;
int x, y;
char *action;
{
  InputState *ci = (InputState *)closure;

  ci->x = x;
  ci->y = y;
  HandleSubmit(ci->fs->li, ci->fs, ci, action);

  return(true);
}

bool
FormImageMotionCallback(closure, x, y)
void *closure;
int x, y;
{
  InputState *ci = (InputState *)closure;
  HTMLInfo li = ci->fs->li;

  if (ci->fs->action != NULL) HTMLPrintURL(li, ci->fs->action);

  return(true);
}

/*
 * CreateImage
 */
void
CreateImage(fs, env, p)
FormState *fs;
HTMLEnv env;
MLElement p;
{
  char *url;
  HTMLInlineInfo ii;
  InputState *ci;
  ChimeraRenderHooks orh;

  if ((url = MLFindAttribute(p, "src")) == NULL) return;

  memset(&ii, 0, sizeof(ii));
  ii.p = p;
  ii.closure = ci;

  memset(&orh, 0, sizeof(orh));
  orh.select = FormImageSelectCallback;
  orh.motion = FormImageMotionCallback;

  ci = CreateInputState(fs, p, INPUT_IMAGE);
  ci->img = HTMLCreateInline(fs->li, env, url, &ii, &orh, ci);

  return;
}
