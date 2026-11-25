/* What Grace (and its bundled Xbae matrix) needs: bitmap clip masks for
 * images, fills and stipples; XListPixmapFormats; per-character extents;
 * Xt's public converters; Motif's version string; a program's XmManager
 * subclass in a dialog, whose change_managed runs when the dialog pops up
 * and unmanages pages through core.managed (Grace's Tab widget), with the
 * Manager's shadow GCs; small push buttons sized from their labels. */
#include <Xm/XmP.h>
#include <Xm/ManagerP.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/DialogS.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf (stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #e); exit (1); } } while (0)

extern const char _XmVersionString[];
extern void XmRegisterConverters (void);

/* ---- a manager that shows only its first child (as Grace's Tab) ---- */
typedef struct { int change_managed_calls; } PagesPart;
typedef struct { CorePart core; CompositePart composite; ConstraintPart constraint; XmManagerPart manager; PagesPart pages; } PagesRec;
typedef struct { CoreClassPart core_class; CompositeClassPart composite_class; ConstraintClassPart constraint_class;
                 XmManagerClassPart manager_class; } PagesClassRec;

static void pages_change_managed (Widget w)
{
  PagesRec *p = (PagesRec *) w;
  Cardinal i;
  p->pages.change_managed_calls++;
  for (i = 0; i < p->composite.num_children; i++)
    p->composite.children[i]->core.managed = i == 0;
  if (p->core.width < 10) { p->core.width = 200; p->core.height = 100; }
}
static XtGeometryResult pages_geometry (Widget w, XtWidgetGeometry *req, XtWidgetGeometry *reply) { return XtGeometryYes; }

static PagesClassRec pagesClassRec = {
  { (WidgetClass) &xmManagerClassRec, "Pages", sizeof (PagesRec), NULL, NULL, False, NULL, NULL,
    XtInheritRealize, NULL, 0, NULL, 0, NULLQUARK, True, XtExposeCompressMultiple, True, False, NULL, XtInheritResize,
    XtInheritExpose, NULL, NULL, NULL, NULL, XtInheritAcceptFocus, XtVersion, NULL, XtInheritTranslations,
    XtInheritQueryGeometry, NULL, NULL },
  { pages_geometry, pages_change_managed, XtInheritInsertChild, XtInheritDeleteChild, NULL },
  { NULL, 0, sizeof (XmManagerConstraintRec), NULL, NULL, NULL, NULL },
  { XtInheritTranslations, NULL, 0, NULL, 0, XmInheritParentProcess, NULL }
};

static XtAppContext app;
static Widget pages, page1, page2;

static void after_popup (XtPointer d, XtIntervalId *id)
{
  PagesRec *p = (PagesRec *) pages;
  CHECK (p->pages.change_managed_calls > 0);   /* the dialog's realize ran it */
  CHECK (XtIsManaged (page1) && !XtIsManaged (page2));
  CHECK (p->manager.top_shadow_GC && p->manager.bottom_shadow_GC && p->manager.background_GC);
  printf ("grace ok\n");
  exit (0);
}

static unsigned long pix (XImage *im, int x, int y) { return XGetPixel (im, x, y) & 0xffffff; }

int main (int argc, char **argv)
{
  Widget top = XtVaAppInitialize (&app, "Grace", NULL, 0, &argc, argv, NULL, NULL);
  Display *d = XtDisplay (top);
  int scr = DefaultScreen (d), depth = DefaultDepth (d, scr), n, i, psize = 0;
  Window root = RootWindow (d, scr);

  /* Motif's version string, and the old converter registration */
  CHECK (strstr (_XmVersionString, "Motif") != NULL);
  XmRegisterConverters ();
  CHECK (XExtendedMaxRequestSize (d) > 0);

  /* pixmap formats: how XCreateImage lays out each depth */
  XPixmapFormatValues *pf = XListPixmapFormats (d, &n);
  CHECK (pf && n > 0);
  for (i = 0; i < n; i++) if (pf[i].depth == depth) psize = pf[i].bits_per_pixel / 8;
  XFree (pf);
  CHECK (psize >= 3);

  /* a ZPixmap image drawn through a clip mask (Grace's antialiased text):
     only where the mask's bits are set */
  Pixmap pm = XCreatePixmap (d, root, 40, 20, depth);
  GC gc = XCreateGC (d, pm, 0, NULL);
  XSetForeground (d, gc, 0xffffff);
  XFillRectangle (d, pm, gc, 0, 0, 40, 20);
  int w = 10, h = 10, j, k, l;
  char *data = calloc (w * h, psize);
  for (k = 0; k < h; k++) for (j = 0; j < w; j++) for (l = 0; l < psize; l++)
    data[psize * (k * w + j) + l] = (char) (0xff0000 >> (8 * l));
  XImage *im = XCreateImage (d, DefaultVisual (d, scr), depth, ZPixmap, 0, data, w, h, 8, 0);
  char mask[2 * 10];
  memset (mask, 0, sizeof mask);
  for (k = 0; k < h; k++) mask[k * 2] = 0x1f;   /* the left five pixels of each row (LSB first) */
  Pixmap cm = XCreateBitmapFromData (d, root, mask, w, h);
  XSetClipMask (d, gc, cm);
  XSetClipOrigin (d, gc, 5, 5);
  im->bitmap_bit_order = LSBFirst;
  im->byte_order = LSBFirst;
  XPutImage (d, pm, gc, im, 0, 0, 5, 5, w, h);
  /* the same mask for a fill */
  XSetForeground (d, gc, 0x0000ff);
  XSetClipOrigin (d, gc, 25, 5);
  XFillRectangle (d, pm, gc, 25, 5, 10, 10);
  XSetClipMask (d, gc, None);
  /* a stipple: its set bits are painted */
  XSetForeground (d, gc, 0x00ff00);
  XSetStipple (d, gc, cm);
  XSetFillStyle (d, gc, FillStippled);
  XSetTSOrigin (d, gc, 0, 10);
  XFillRectangle (d, pm, gc, 0, 10, 10, 10);
  XSetFillStyle (d, gc, FillSolid);
  XImage *out = XGetImage (d, pm, 0, 0, 40, 20, AllPlanes, ZPixmap);
  CHECK (pix (out, 7, 7) == 0xff0000 && pix (out, 12, 7) == 0xffffff && pix (out, 13, 7) == 0xffffff);
  CHECK (pix (out, 27, 7) == 0x0000ff && pix (out, 32, 7) == 0xffffff);
  CHECK (pix (out, 2, 15) == 0x00ff00 && pix (out, 7, 15) == 0xffffff);

  /* per-character extents (Xbae clips cell text with them) */
  char **missing; int nmissing; char *def;
  XFontSet fs = XCreateFontSet (d, "-*-*-medium-r-normal--14-*-*-*-*-*-*-*", &missing, &nmissing, &def);
  CHECK (fs);
  XRectangle ink[8], logical[8], overall;
  int nchars = 0;
  CHECK (XmbTextPerCharExtents (fs, "Grace", 5, ink, logical, 8, &nchars, NULL, &overall));
  CHECK (nchars == 5 && logical[0].x == 0 && logical[1].x > 0 && logical[4].x > logical[3].x);
  CHECK (overall.width == XmbTextEscapement (fs, "Grace", 5));
  CHECK (!XmbTextPerCharExtents (fs, "Grace", 5, ink, logical, 2, &nchars, NULL, NULL) && nchars == 5);

  /* Xt's converters as functions: into static storage, into the caller's
     buffer, and a buffer too small */
  XrmValue from = { 4, (XPointer) "red" }, to = { 0, NULL };
  CHECK (XtCvtStringToPixel (d, NULL, NULL, &from, &to, NULL) && (*(Pixel *) to.addr & 0xffffff) == 0xff0000);
  Pixel p = 0;
  to.size = sizeof p; to.addr = (XPointer) &p;
  from.addr = (XPointer) "blue"; from.size = 5;
  CHECK (XtCvtStringToPixel (d, NULL, NULL, &from, &to, NULL) && (p & 0xffffff) == 0x0000ff);
  char small;
  to.size = 1; to.addr = &small;
  CHECK (!XtCvtStringToPixel (d, NULL, NULL, &from, &to, NULL) && to.size == sizeof (Pixel));
  int iv = 0;
  from.addr = (XPointer) "42"; from.size = 3; to.size = sizeof iv; to.addr = (XPointer) &iv;
  CHECK (XtCvtStringToInt (d, NULL, NULL, &from, &to, NULL) && iv == 42);

  /* a small push button is as wide as its label needs (styles make text
     buttons 80 pixels wide) */
  Widget form = XmCreateForm (top, "form", NULL, 0);
  Widget zx = XtVaCreateManagedWidget ("ZX", xmPushButtonWidgetClass, form, NULL);
  XtManageChild (form);
  XtRealizeWidget (top);
  XtWidgetGeometry pref;
  XtQueryGeometry (zx, NULL, &pref);
  Dimension zw = 0;
  XtVaGetValues (zx, XmNwidth, &zw, NULL);
  CHECK (zw > 0 && zw < 60);

  /* the program's manager in a dialog: managing the dialog's child pops it
     up and realizes it */
  Widget shell = XmCreateDialogShell (top, "pagesDialog", NULL, 0);
  Widget box = XmCreateForm (shell, "box", NULL, 0);
  pages = XtCreateWidget ("pages", (WidgetClass) &pagesClassRec, box, NULL, 0);
  page1 = XtVaCreateManagedWidget ("one", xmPushButtonWidgetClass, pages, NULL);
  page2 = XtVaCreateManagedWidget ("two", xmPushButtonWidgetClass, pages, NULL);
  XtManageChild (pages);
  XtManageChild (box);
  XtAppAddTimeOut (app, 300, after_popup, NULL);
  XtAppMainLoop (app);
  return 1;
}
