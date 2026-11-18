/*
 * Athena widgets with Qt style faces, and what they rely on in our Xt:
 * a Box sized at start inside a Form (requests before the parent is
 * realized are granted), a Label's given width, per-class actions (Command,
 * Toggle, Scrollbar and SmeBSB each have a "set"), radio groups, a
 * SimpleMenu popup (sized in change_managed when popped up, drawn by its own
 * expose, Xt's MenuPopup/MenuPopdown actions). Driven by MOTIFQT_SCRIPT,
 * built as plain C, run headless.
 */
#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Scrollbar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf (stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit (1); } } while (0)

static Widget box, ok, t1, t2, label, sh, sv, menu;
static int applied, chose_two, scrolled;

static void apply_cb (Widget w, XtPointer c, XtPointer d) { applied++; }
static void two_cb (Widget w, XtPointer c, XtPointer d) { chose_two++; }
static void scroll_cb (Widget w, XtPointer c, XtPointer d) { scrolled++; }

static void check (XtPointer p, XtIntervalId *id)
{
  /* the script clicked Apply, Green, the scroll bar and "Two" in the menu */
  CHECK (applied == 1);
  CHECK (scrolled == 1);
  CHECK (chose_two == 1);
  Boolean s1 = True, s2 = False;
  XtVaGetValues (t1, XtNstate, &s1, NULL);
  XtVaGetValues (t2, XtNstate, &s2, NULL);
  CHECK (!s1 && s2);
  /* the menu: entries stacked, as SimpleMenu lays them out */
  CHECK (menu->core.height > menu->core.width / 2 && menu->core.width > 10);
  printf ("ok\n");
  exit (0);
}

int main (int argc, char **argv)
{
  setenv ("MOTIFQT_SCRIPT", "wait 300; click ok; wait 100; click t2; wait 100; click sh 1 10 7; wait 100; "
                            "click mb; wait 300; drag menu 14 22 14 26; wait 200", 1);
  XtAppContext app;
  Widget top = XtAppInitialize (&app, "XawFaces", NULL, 0, &argc, argv, NULL, NULL, 0);
  Widget form = XtCreateManagedWidget ("form", formWidgetClass, top, NULL, 0);
  box = XtVaCreateManagedWidget ("box", boxWidgetClass, form, XtNorientation, XtorientHorizontal, NULL);
  ok = XtVaCreateManagedWidget ("ok", commandWidgetClass, box, XtNlabel, "Apply", NULL);
  XtAddCallback (ok, XtNcallback, apply_cb, NULL);
  Widget mb = XtVaCreateManagedWidget ("mb", menuButtonWidgetClass, box, XtNlabel, "Options", XtNmenuName, "menu", NULL);
  menu = XtCreatePopupShell ("menu", simpleMenuWidgetClass, mb, NULL, 0);
  XtCreateManagedWidget ("One", smeBSBObjectClass, menu, NULL, 0);
  XtCreateManagedWidget ("line", smeLineObjectClass, menu, NULL, 0);
  XtAddCallback (XtCreateManagedWidget ("Two", smeBSBObjectClass, menu, NULL, 0), XtNcallback, two_cb, NULL);
  Widget box2 = XtVaCreateManagedWidget ("box2", boxWidgetClass, form, XtNorientation, XtorientHorizontal, XtNfromVert, box, NULL);
  t1 = XtVaCreateManagedWidget ("t1", toggleWidgetClass, box2, XtNlabel, "Red", XtNstate, True, NULL);
  t2 = XtVaCreateManagedWidget ("t2", toggleWidgetClass, box2, XtNlabel, "Green", XtNradioGroup, t1, NULL);
  label = XtVaCreateManagedWidget ("label", labelWidgetClass, form, XtNlabel, "Left", XtNjustify, XtJustifyLeft,
                                   XtNwidth, 220, XtNfromVert, box2, NULL);
  CHECK (label->core.width == 220);   /* given, not sized to the text */
  sh = XtVaCreateManagedWidget ("sh", scrollbarWidgetClass, form, XtNorientation, XtorientHorizontal,
                                XtNlength, 220, XtNfromVert, label, NULL);
  XtAddCallback (sh, XtNscrollProc, scroll_cb, NULL);
  XawScrollbarSetThumb (sh, 0.25, 0.4);
  sv = XtVaCreateManagedWidget ("sv", scrollbarWidgetClass, form, XtNlength, 80, XtNfromHoriz, label, XtNfromVert, box2, NULL);
  XtRealizeWidget (top);
  /* the Box was sized to its buttons inside the Form, and the Form placed
   * the vertical scroll bar right of the 220 wide label */
  CHECK (box->core.width >= ok->core.width + mb->core.width);
  CHECK (sv->core.x >= label->core.x + 220);
  XtAppAddTimeOut (app, 1500, check, NULL);
  XtAppMainLoop (app);
  return 1;
}
