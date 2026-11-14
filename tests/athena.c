/*
 * What DDD needs from the layers under Motif: the Athena widgets (genuine
 * libXaw on our Xt), Xpm images, font sets, converters a program registers,
 * the XmIsXxx macros of XmP.h, XtIsTopLevelShell, XmText rows/columns,
 * programmatic text changes that pass no event to modifyVerify, and Latin-1
 * text. Built as plain C, run headless.
 */
#include <Xm/XmP.h>
#include <Xm/Text.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/ScrolledW.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Panner.h>
/* Xm.h's FMT8BIT macro clashes with the variable Xaw's Text.h declares
 * (with the real libraries too) */
#undef FMT8BIT
#undef FMT16BIT
#include <X11/Xaw/AsciiText.h>
#include <X11/xpm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf (stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); exit (1); } } while (0)

static XtAppContext app;
static Widget top, box, quit, panner, text, mtext;
static int converted, vetoed;

/* a program's own String -> XmString converter (as DDD's "@bf" markup) */
static Boolean cvt_xmstring (Display *d, XrmValue *args, Cardinal *n, XrmValue *from, XrmValue *to, XtPointer *data)
{
  static XmString s;
  const char *src = (const char *) from->addr;
  converted++;
  s = XmStringCreateLocalized ((char *) (strncmp (src, "@bf ", 4) == 0 ? src + 4 : src));
  if (to->addr) {
    if (to->size < sizeof (XmString)) { to->size = sizeof (XmString); return False; }
    *(XmString *) to->addr = s;
  } else to->addr = (XPointer) &s;
  to->size = sizeof (XmString);
  return True;
}

/* DDD's read-only source window: only the user's edits carry an event */
static void no_user_edits (Widget w, XtPointer c, XtPointer call)
{
  XmTextVerifyCallbackStruct *cbs = (XmTextVerifyCallbackStruct *) call;
  if (cbs->event) { cbs->doit = False; vetoed++; }
}

static const char *xpm[] = {
  "4 2 3 1",
  "  c None",
  ". c red",
  "X s fg c blue",
  " .X.",
  "X.. ",
};

static void late_checks (XtPointer p, XtIntervalId *id)
{
  /* Box laid out its children and kept them mapped (XMapSubwindows) */
  Dimension bh = 0, qw = 0;
  XtVaGetValues (box, XtNheight, &bh, NULL);
  XtVaGetValues (quit, XtNwidth, &qw, NULL);
  CHECK (XtIsManaged (quit) && quit->core.managed);
  CHECK (bh > 10 && qw > 10);

  /* Xt's default colours are the desktop's: Command's foreground and
   * background differ (black on the window colour, or light on dark) */
  Pixel fg = 0, bg = 0, conv_fg = 1;
  XtVaGetValues (quit, XtNforeground, &fg, XtNbackground, &bg, NULL);
  XrmValue from = { sizeof "XtDefaultForeground", (XPointer) "XtDefaultForeground" }, to = { sizeof conv_fg, (XPointer) &conv_fg };
  CHECK (XtConvertAndStore (quit, XtRString, &from, XtRPixel, &to) && conv_fg == fg && fg != bg);

  /* the text widget's string comes from its source (get_values_hook) */
  char *s = NULL;
  XtVaGetValues (text, XtNstring, &s, NULL);
  CHECK (s && strcmp (s, "Hello Athena") == 0);
  CHECK (XawTextGetInsertionPoint (text) == 5);
  printf ("ok\n");
  exit (0);
}

/* XrmParseCommand binds a specifier without '.' or '*' to the program name
 * with '.' (DDD's --version) */
static void check_parse_command (void)
{
  static XrmOptionDescRec opts[] = { { "--version", "showVersion", XrmoptionNoArg, (XPointer) "on" } };
  int ac = 2;
  char *av[] = { "ddd", "--version", NULL };
  XrmDatabase db = XrmGetStringDatabase ("");
  XrmParseCommand (&db, opts, 1, "Ddd", &ac, av);
  char *type;
  XrmValue v;
  CHECK (ac == 1);
  CHECK (XrmGetResource (db, "ddd.showVersion", "Ddd.showVersion", &type, &v) && strcmp ((char *) v.addr, "on") == 0);
  XrmDestroyDatabase (db);

  /* value escapes as Xlib reads them: octal bytes, kept blanks, \n */
  db = XrmGetStringDatabase ("Ddd*name: L\\303\\274tkehaus\nDdd*help: \\ Help\\ \nDdd*two: a\\nb\nDdd*re: \\d");
  CHECK (XrmGetResource (db, "ddd.name", "Ddd.Name", &type, &v) && strcmp ((char *) v.addr, "L\303\274tkehaus") == 0);
  CHECK (XrmGetResource (db, "ddd.help", "Ddd.Help", &type, &v) && strcmp ((char *) v.addr, " Help ") == 0);
  CHECK (XrmGetResource (db, "ddd.two", "Ddd.Two", &type, &v) && strcmp ((char *) v.addr, "a\nb") == 0);
  CHECK (XrmGetResource (db, "ddd.re", "Ddd.Re", &type, &v) && strcmp ((char *) v.addr, "\\d") == 0);
  XrmDestroyDatabase (db);
}

/* what keys type: Latin-1, legacy keysyms (the euro sign, Cyrillic),
 * Unicode keysyms, the keypad, Control; their names */
static void check_keys (Display *d)
{
  XKeyEvent e;
  memset (&e, 0, sizeof e);
  e.type = KeyPress;
  e.display = d;
  char buf[16];
  KeySym k;
  e.keycode = XK_eacute;
  CHECK (XLookupString (&e, buf, sizeof buf, &k, NULL) == 1 && (unsigned char) buf[0] == 0xe9 && k == XK_eacute);
  e.keycode = XK_EuroSign;   /* a legacy keysym: the C locale has no euro, so UTF-8 */
  CHECK (XLookupString (&e, buf, sizeof buf, &k, NULL) == 3 && memcmp (buf, "\xe2\x82\xac", 3) == 0);
  e.keycode = 0x01000000 | 0x1F600;   /* an emoji, as a Unicode keysym */
  CHECK (XLookupString (&e, buf, sizeof buf, &k, NULL) == 4 && memcmp (buf, "\xf0\x9f\x98\x80", 4) == 0);
  e.keycode = XK_KP_5;
  CHECK (XLookupString (&e, buf, sizeof buf, &k, NULL) == 1 && buf[0] == '5');
  e.keycode = 'a';
  e.state = ControlMask;
  CHECK (XLookupString (&e, buf, sizeof buf, &k, NULL) == 1 && buf[0] == 1);
  e.state = 0;
  e.keycode = XK_F1;
  CHECK (XLookupString (&e, buf, sizeof buf, &k, NULL) == 0 && k == XK_F1);

  /* UTF-8 whatever the locale; too small a buffer says so */
  Status st;
  e.keycode = XK_Cyrillic_a;
  CHECK (Xutf8LookupString (NULL, &e, buf, sizeof buf, &k, &st) == 2 && memcmp (buf, "\xd0\xb0", 2) == 0 && st == XLookupBoth);
  CHECK (Xutf8LookupString (NULL, &e, buf, 1, &k, &st) == 2 && st == XBufferOverflow);
  wchar_t wbuf[4];
  CHECK (XwcLookupString (NULL, &e, wbuf, 4, &k, &st) == 1 && wbuf[0] == 0x430);

  CHECK (strcmp (XKeysymToString (0x010020ac), "U20AC") == 0);
  CHECK (XStringToKeysym ("U20AC") == 0x010020ac);
  KeySym lo, up;
  XConvertCase (0x01000000 | 0x0416, &lo, &up);   /* Cyrillic capital zhe */
  CHECK (lo == (0x01000000 | 0x0436) && up == (0x01000000 | 0x0416));
  XConvertCase (XK_Eacute, &lo, &up);
  CHECK (lo == XK_eacute && up == XK_Eacute);
}

int main (int argc, char **argv)
{
  check_parse_command ();
  XtSetTypeConverter (XmRString, XmRXmString, cvt_xmstring, NULL, 0, XtCacheNone, NULL);
  top = XtVaAppInitialize (&app, "Athena", NULL, 0, &argc, argv, NULL, NULL);
  CHECK (XtIsTopLevelShell (top) && XtIsWMShell (top));
  check_keys (XtDisplay (top));

  /* Athena */
  Widget paned = XtCreateManagedWidget ("paned", panedWidgetClass, top, NULL, 0);
  box = XtCreateManagedWidget ("box", boxWidgetClass, paned, NULL, 0);
  quit = XtCreateManagedWidget ("Quit", commandWidgetClass, box, NULL, 0);
  panner = XtCreateManagedWidget ("panner", pannerWidgetClass, box, NULL, 0);
  text = XtVaCreateManagedWidget ("text", asciiTextWidgetClass, paned, XtNeditType, XawtextEdit,
                                  XtNstring, "Hello Athena", XtNwidth, 200, XtNheight, 40, NULL);
  XawTextSetInsertionPoint (text, 5);

  /* Motif in the same program; the converter turns resources into labels */
  Widget form = XtVaCreateManagedWidget ("form", xmFormWidgetClass, paned, NULL);
  XrmValue from, to;
  XmString label = NULL;
  from.addr = (XPointer) "@bf Bold";
  from.size = strlen (from.addr) + 1;
  to.addr = (XPointer) &label;
  to.size = sizeof label;
  CHECK (XtConvertAndStore (form, XmRString, &from, XmRXmString, &to) && label);
  CHECK (converted == 1);
  char *plain = NULL;
  XmStringGetLtoR (label, XmFONTLIST_DEFAULT_TAG, &plain);
  CHECK (plain && strcmp (plain, "Bold") == 0);

  Arg a[3];
  XtSetArg (a[0], XmNrows, 10);
  XtSetArg (a[1], XmNcolumns, 40);
  XtSetArg (a[2], XmNeditMode, XmMULTI_LINE_EDIT);
  mtext = XmCreateScrolledText (form, "source", a, 3);
  XtManageChild (mtext);
  CHECK (XmIsScrolledWindow (XtParent (mtext)) && XmIsText (mtext) && XmIsManager (form) && !XmIsPrimitive (form));
  short rows = 0, cols = 0;
  XtVaGetValues (mtext, XmNrows, &rows, XmNcolumns, &cols, NULL);
  CHECK (rows == 10 && cols == 40);
  XtWidgetGeometry g;
  g.request_mode = 0;
  XtQueryGeometry (mtext, NULL, &g);
  CHECK (g.height > 5 * 10 && g.width > 4 * 40);

  XtAddCallback (mtext, XmNmodifyVerifyCallback, no_user_edits, NULL);
  XmTextSetString (mtext, "  1 #include <stdio.h>\n  2 int main;");
  XmTextReplace (mtext, 0, 1, " ");
  CHECK (vetoed == 0 && XmTextGetLastPosition (mtext) == 36);

  /* the cursor as Motif moves it (DDD's console reads its prompt position
   * back from it): an insertion at the cursor moves it past the new text,
   * text before it shifts it, text after it leaves it */
  XmTextSetString (mtext, "(gdb) ");
  XmTextSetInsertionPosition (mtext, 6);
  XmTextInsert (mtext, 6, "out\n");
  CHECK (XmTextGetInsertionPosition (mtext) == 10);
  XmTextInsert (mtext, 0, "ab");
  CHECK (XmTextGetInsertionPosition (mtext) == 12);
  XmTextInsert (mtext, 12, "");
  XmTextReplace (mtext, 12, 12, "end");
  CHECK (XmTextGetInsertionPosition (mtext) == 15);
  XmTextSetInsertionPosition (mtext, 3);
  XmTextInsert (mtext, 5, "xy");
  CHECK (XmTextGetInsertionPosition (mtext) == 3);

  /* Latin-1 bytes (DDD's sources, gdb in the C locale) come back unchanged */
  XmTextSetString (mtext, "Copyright \251 L\374tkehaus");
  char *back = XmTextGetString (mtext);
  CHECK (back && strcmp (back, "Copyright \251 L\374tkehaus") == 0);
  XtFree (back);

  /* one string with a Latin-1 byte and a UTF-8 sequence: each as written */
  XmString mixed = XmStringCreateLocalized ("\251 Universit\303\244t");
  char *mt = NULL;
  XmStringGetLtoR (mixed, XmFONTLIST_DEFAULT_TAG, &mt);
  /* an XmString keeps the bytes it was given, as in Motif */
  CHECK (mt && strcmp (mt, "\251 Universit\303\244t") == 0);
  XtFree (mt);
  /* text widgets hold characters: in the C locale both come back as Latin-1 */
  XmTextSetString (mtext, "\251 Universit\303\244t");
  back = XmTextGetString (mtext);
  CHECK (back && strcmp (back, "\251 Universit\344t") == 0);
  XtFree (back);
  XmStringFree (mixed);

  /* Xpm images: the parsed form and back */
  XpmImage im;
  XpmInfo info;
  CHECK (XpmCreateXpmImageFromData ((char **) xpm, &im, &info) == XpmSuccess);
  CHECK (im.width == 4 && im.height == 2 && im.ncolors == 3 && strcmp (im.colorTable[2].symbolic, "fg") == 0);
  Pixmap pm = 0, mask = 0;
  XpmAttributes attr;
  attr.valuemask = 0;
  CHECK (XpmCreatePixmapFromXpmImage (XtDisplay (top), DefaultRootWindow (XtDisplay (top)), &im, &pm, &mask, &attr) == XpmSuccess);
  CHECK (pm && mask && attr.width == 4 && attr.height == 2);
  XImage *xi = NULL;
  CHECK (XpmCreateImageFromXpmImage (XtDisplay (top), &im, &xi, NULL, NULL) == XpmSuccess);
  CHECK (XGetPixel (xi, 1, 0) == 0xff0000 && XGetPixel (xi, 2, 0) == 0x0000ff);
  XDestroyImage (xi);
  XpmFreeXpmImage (&im);
  XpmFreeXpmInfo (&info);

  /* font sets */
  char **missing, *def;
  int nmissing;
  XFontSet fs = XCreateFontSet (XtDisplay (top), "-*-helvetica-medium-r-*-*-12-*,*", &missing, &nmissing, &def);
  CHECK (fs && nmissing == 0);
  CHECK (XmbTextEscapement (fs, "Hello", 5) > XmbTextEscapement (fs, "Hi", 2));
  XFontSetExtents *ext = XExtentsOfFontSet (fs);
  CHECK (ext && ext->max_logical_extent.height > 5);
  XFreeFontSet (XtDisplay (top), fs);

  XtRealizeWidget (top);
  XtAppAddTimeOut (app, 300, late_checks, NULL);
  XtAppMainLoop (app);
  return 1;
}
