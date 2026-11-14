/*
 * Input methods: XOpenIM/XCreateIC as Athena and Xlib programs use them,
 * XmIm as Motif programs use it, and text an input method commits reaching
 * a drawing area (read with Xutf8LookupString) and an international Athena
 * text widget as key presses. The commits come from MOTIFQT_SCRIPT's
 * "commit" step, as Qt delivers them from ibus, fcitx or compose.
 * Built as plain C, run headless in a UTF-8 locale.
 */
#include <Xm/Xm.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/XmIm.h>
/* Xm.h's FMT8BIT macro clashes with Xaw's Text.h (with the real libraries too) */
#undef FMT8BIT
#undef FMT16BIT
#include <X11/Xaw/AsciiText.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#define CHECK(expr) do { if (!(expr)) { fprintf (stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit (1); } } while (0)

static XIC ic;
static Widget da, text;
static char typed[256];
static int presses;

static void key_handler (Widget w, XtPointer c, XEvent *e, Boolean *cont)
{
  char buf[32];
  KeySym k;
  Status st;
  if (e->type != KeyPress) return;
  presses++;
  int n = Xutf8LookupString (ic, &e->xkey, buf, sizeof buf, &k, &st);
  CHECK (st == XLookupBoth || st == XLookupChars);
  if (n > 0 && strlen (typed) + n < sizeof typed) strncat (typed, buf, n);
}

static void check (XtPointer p, XtIntervalId *id)
{
  /* "日本€é" committed to the drawing area: four key presses */
  CHECK (presses == 4);
  CHECK (strcmp (typed, "\xe6\x97\xa5\xe6\x9c\xac\xe2\x82\xac\xc3\xa9") == 0);
  /* "€ö" committed to the international Athena text widget */
  char *s = NULL;
  XtVaGetValues (text, XtNstring, &s, NULL);
  CHECK (s && strstr (s, "\xe2\x82\xac\xc3\xb6") != NULL);
  printf ("ok\n");
  exit (0);
}

int main (int argc, char **argv)
{
  if (!setlocale (LC_ALL, "C.UTF-8")) setlocale (LC_ALL, "en_US.UTF-8");
  setenv ("MOTIFQT_SCRIPT", "wait 300; commit da \xe6\x97\xa5\xe6\x9c\xac\xe2\x82\xac\xc3\xa9; wait 100; "
                            "commit text \xe2\x82\xac\xc3\xb6; wait 100", 1);
  XtAppContext app;
  Widget top = XtVaAppInitialize (&app, "InputMethod", NULL, 0, &argc, argv, NULL, NULL);
  Display *d = XtDisplay (top);

  /* the "server" offers over-the-spot and root styles */
  XIM im = XOpenIM (d, NULL, NULL, NULL);
  CHECK (im != NULL);
  XIMStyles *styles = NULL;
  CHECK (XGetIMValues (im, XNQueryInputStyle, &styles, NULL) == NULL && styles);
  int position = 0, nothing = 0;
  for (int i = 0; i < styles->count_styles; i++) {
    if (styles->supported_styles[i] == (XIMPreeditPosition | XIMStatusNothing)) position = 1;
    if (styles->supported_styles[i] == (XIMPreeditNothing | XIMStatusNothing)) nothing = 1;
  }
  CHECK (position && nothing);
  XFree (styles);

  Widget form = XtVaCreateManagedWidget ("form", xmFormWidgetClass, top, NULL);
  da = XtVaCreateManagedWidget ("da", xmDrawingAreaWidgetClass, form, XmNwidth, 200, XmNheight, 80,
                                XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, NULL);
  XtAddEventHandler (da, KeyPressMask, False, key_handler, NULL);
  text = XtVaCreateManagedWidget ("text", asciiTextWidgetClass, form, XtNeditType, XawtextEdit,
                                  XtNinternational, True, XtNwidth, 200, XtNheight, 40,
                                  XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, da, XmNleftAttachment, XmATTACH_FORM, NULL);
  /* a Motif program's own text widget registers with XmIm */
  XPoint xm_spot = { 5, 20 };
  XmImRegister (da, 0);
  XmImVaSetValues (da, XmNspotLocation, &xm_spot, NULL);
  XtRealizeWidget (top);

  /* an over-the-spot context, the spot in a nested list as programs pass it */
  XPoint spot = { 10, 30 };
  XVaNestedList pre = XVaCreateNestedList (0, XNSpotLocation, &spot, NULL);
  ic = XCreateIC (im, XNInputStyle, XIMPreeditPosition | XIMStatusNothing, XNClientWindow, XtWindow (top),
                  XNFocusWindow, XtWindow (da), XNPreeditAttributes, pre, NULL);
  XFree (pre);
  CHECK (ic != NULL && XIMOfIC (ic) == im);
  unsigned long filter = 0;
  CHECK (XGetICValues (ic, XNFilterEvents, &filter, NULL) == NULL && (filter & KeyPressMask));
  CHECK (XSetICValues (ic, XNSpotLocation, &spot, NULL) == NULL);
  CHECK (XSetICValues (ic, "noSuchAttribute", 1, NULL) != NULL);   /* the unknown name comes back */
  XSetICFocus (ic);

  XtAppAddTimeOut (app, 1500, check, NULL);
  XtAppMainLoop (app);
  return 1;
}
