/*
 * fallback.c
 *
 * Copyright 1993-1997, John Kilburg <john@cs.unlv.edu>
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
#include <stdio.h>

char *fallback_resources[] =
{
  "*background:                moccasin",
  "*showGrip:                  false",

  "*Scrollbar.background:      burlywood2",
  "*Command.background:        burlywood2",
  "*Toggle.background:         burlywood2",
  "*MenuButton.background:     burlywood2",
  "*SimpleMenu.background:     burlywood2",
  "*SmeBSB.background:         burlywood2",
  "*Box.orientation:           horizontal",
  "*Label.borderWidth:         0",

  "*allowHoriz:                true",
  "*allowVert:                 true",

  /* labels for commands */
  "*open.label:                Open",
  "*quit.label:                Quit",
  "*reload.label:              Reload",
  "*help.label:                Help",
  "*home.label:                Home",
  "*back.label:                Back",
  "*cancel.label:              Cancel",
  "*dup.label:                 Clone",
  "*addmark.label:             Add Mark",
  "*viewmark.label:            View Mark",
  "*bookmark.label:            Bookmark",
  "*save.label:                Save",
  "*source.label:              Source",
  "*addgroup.label:            Add Group",
  "*rmgroup.label:             Remove Group",
  "*rmmark.label:              Remove Mark",
  "*mlabel.label:              Mark",
  "*glabel.label:              Groups",
  "*dismiss.label:             Dismiss",

  "*www_toplevel.height:       600",
  "*www_toplevel.width:        650",

  /* message widget */
  "*message.width:             600",
  "*message.editable:          false",
  "*message.displayCaret:      false",
  "*message.borderWidth:       0",

  /* bookmark */
  "*grouplist.defaultColumns:  1",
  "*grouplist.verticalList:    true",
  "*groupview.width:           400",
  "*groupview.height:          100",
  "*groupview.allowVert:       true",
  "*marklist.defaultColumns:   1",
  "*marklist.verticalList:     true",
  "*markview.width:            400",
  "*markview.height:           100",
  "*markview.allowVert:        true",

  /* resources for dialog widget */
  "*dialog.ok.label:           OK",
  "*dialog.clear.label:        Clear",
  "*dialog.dismiss.label:      Dismiss",
  "*dialog.value.length:       80",
  "*openpop.dialog.label:      Enter URL",
  "*savepop.dialog.label:      Enter Filename",
  "*download.dialog.label:     Enter Filename",
  "*agpop.dialog.label:        Enter Group Name",
  "*ampop.dialog.label:        Enter Mark Name",

  "*urllabel.label:            URL:",
  "*urllabel.left:             ChainLeft",
  "*urllabel.right:            ChainLeft",
  "*url.left:                  ChainLeft",
  "*url.right:                 ChainRight",
  "*url.fromHoriz:             urllabel",

  "*Label.font: -*-lucidatypewriter-medium-r-normal-*-*-120-*-*-*-*-iso8859-1",
  "*Text.font:  -*-lucidatypewriter-medium-r-normal-*-*-120-*-*-*-*-iso8859-1",
  "*Command.font: -*-lucida-bold-r-normal-sans-*-120-*-*-*-*-iso8859-1",
  "*Toggle.font:  -*-lucida-bold-r-normal-sans-*-120-*-*-*-*-iso8859-1",

  NULL
};
