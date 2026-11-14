/*
 * Motif on Qt - libXpm: XPM images as pixmaps, shape masks and XImages.
 *
 * Our own reader of the XPM 3 format (data arrays, buffers, files): colour
 * keys (c, g, g4, m), "None" for transparency and symbolic colour overrides
 * (XpmColorSymbols), which Qt's own XPM reader does not offer.
 * Licensed under the LGPL 2.1 or later; see LICENSE.LGPL. X11/xpm.h is
 * libXpm's (MIT).
 */
#include "private.h"
#include <X11/xpm.h>
#include <QFile>
#include <cstring>

namespace {
struct XpmPicture {
  QImage image;              /* ARGB32; transparent where the XPM says None */
  bool transparent = false;
  int x_hot = -1, y_hot = -1;
  int cpp = 0, ncolors = 0;
};

/* the strings of an XPM file or buffer: the quoted parts, in order */
QList<QByteArray> xpm_strings (const QByteArray &text)
{
  QList<QByteArray> out;
  int i = 0, n = text.size ();
  while (i < n) {
    char c = text[i];
    if (c == '/' && i + 1 < n && text[i + 1] == '*') {
      int e = text.indexOf ("*/", i + 2);
      i = e < 0 ? n : e + 2;
    } else if (c == '"') {
      int e = i + 1;
      QByteArray s;
      while (e < n && text[e] != '"') { if (text[e] == '\\' && e + 1 < n) e++; s += text[e++]; }
      out << s;
      i = e + 1;
    } else i++;
  }
  return out;
}

int color_key_for (const XpmAttributes *a)
{
  if (a && (a->valuemask & XpmColorKey)) return a->color_key;
  return XPM_COLOR;
}

/* one colour table line: the chars, then key/value pairs (values may have spaces) */
QColor parse_color_line (const QByteArray &line, int cpp, int key, const XpmAttributes *a, bool *none)
{
  QList<QByteArray> words = line.mid (cpp).simplified ().split (' ');
  static const QByteArray keys[] = { "m", "s", "g4", "g", "c" };
  QHash<QByteArray, QByteArray> vals;
  QByteArray cur;
  for (const QByteArray &w : words) {
    bool is_key = false;
    for (const QByteArray &k : keys) if (w == k) is_key = true;
    if (is_key) { cur = w; vals[cur] = QByteArray (); }
    else if (!cur.isEmpty ()) vals[cur] += (vals[cur].isEmpty () ? "" : " ") + w;
  }
  /* a symbolic name the program overrides */
  if (a && (a->valuemask & XpmColorSymbols) && vals.contains ("s"))
    for (unsigned i = 0; i < a->numsymbols; i++) {
      const XpmColorSymbol &cs = a->colorsymbols[i];
      if (cs.name && qstricmp (cs.name, vals["s"].constData ()) == 0) {
        if (cs.value) vals["c"] = vals["g"] = vals["g4"] = vals["m"] = cs.value;
        else { *none = false; return mq_color (cs.pixel); }
      }
    }
  QByteArray v;
  const char *order_color[] = { "c", "g", "g4", "m" }, *order_gray[] = { "g", "g4", "c", "m" },
             *order_gray4[] = { "g4", "g", "c", "m" }, *order_mono[] = { "m", "g4", "g", "c" };
  const char **order = key == XPM_MONO ? order_mono : key == XPM_GRAY4 ? order_gray4 : key == XPM_GRAY ? order_gray : order_color;
  for (int i = 0; i < 4 && v.isEmpty (); i++) v = vals.value (order[i]);
  *none = v.isEmpty () || qstricmp (v.constData (), "none") == 0;
  if (*none) return QColor (0, 0, 0, 0);
  QColor c (QString::fromLatin1 (v));
  if (!c.isValid ()) c = QColor (QString::fromLatin1 (v).remove (' '));   /* "light gray" */
  return c.isValid () ? c : QColor (Qt::black);
}

int parse (const QList<QByteArray> &s, const XpmAttributes *a, XpmPicture *out)
{
  if (s.isEmpty ()) return XpmFileInvalid;
  QList<QByteArray> hdr = s[0].simplified ().split (' ');
  if (hdr.size () < 4) return XpmFileInvalid;
  int w = hdr[0].toInt (), h = hdr[1].toInt (), nc = hdr[2].toInt (), cpp = hdr[3].toInt ();
  if (w <= 0 || h <= 0 || nc <= 0 || cpp <= 0 || s.size () < 1 + nc + h) return XpmFileInvalid;
  if (hdr.size () >= 6) { out->x_hot = hdr[4].toInt (); out->y_hot = hdr[5].toInt (); }
  int key = color_key_for (a);
  QHash<QByteArray, QRgb> table;
  for (int i = 0; i < nc; i++) {
    const QByteArray &line = s[1 + i];
    bool none = false;
    QColor c = parse_color_line (line, cpp, key, a, &none);
    if (none) out->transparent = true;
    table.insert (line.left (cpp), none ? qRgba (0, 0, 0, 0) : c.rgb ());
  }
  out->image = QImage (w, h, QImage::Format_ARGB32);
  for (int y = 0; y < h; y++) {
    const QByteArray &row = s[1 + nc + y];
    QRgb *dst = reinterpret_cast<QRgb *> (out->image.scanLine (y));
    for (int x = 0; x < w; x++) dst[x] = table.value (row.mid (x * cpp, cpp), qRgba (0, 0, 0, 0));
  }
  out->cpp = cpp;
  out->ncolors = nc;
  return XpmSuccess;
}

QList<QByteArray> data_strings (char **data)
{
  QList<QByteArray> s;
  if (!data || !data[0]) return s;
  s << data[0];
  QList<QByteArray> hdr = s[0].simplified ().split (' ');
  int n = hdr.size () >= 4 ? 1 + hdr[2].toInt () + hdr[1].toInt () : 1;
  for (int i = 1; i < n && data[i]; i++) s << data[i];
  return s;
}

void fill_attributes (XpmAttributes *a, const XpmPicture &p)
{
  if (!a) return;
  a->width = p.image.width ();
  a->height = p.image.height ();
  a->valuemask |= XpmSize;
  if (p.x_hot >= 0) { a->x_hotspot = p.x_hot; a->y_hotspot = p.y_hot; a->valuemask |= XpmHotspot; }
  if (a->valuemask & XpmReturnPixels) { a->pixels = nullptr; a->npixels = 0; }
  if (a->valuemask & XpmReturnAllocPixels) { a->alloc_pixels = nullptr; a->nalloc_pixels = 0; }
  a->cpp = p.cpp;
  a->ncolors = p.ncolors;
}

QImage mask_of (const QImage &img)
{
  QImage m (img.size (), QImage::Format_RGB32);
  for (int y = 0; y < img.height (); y++)
    for (int x = 0; x < img.width (); x++) m.setPixel (x, y, qAlpha (img.pixel (x, y)) ? 0xffffffff : 0xff000000);
  return m;
}

int to_pixmaps (const XpmPicture &p, Pixmap *pixmap, Pixmap *mask, XpmAttributes *a)
{
  if (pixmap) {
    /* the transparent parts are black underneath, as in libXpm */
    QImage opaque (p.image.size (), QImage::Format_ARGB32);
    opaque.fill (Qt::black);
    QPainter painter (&opaque);
    painter.drawImage (0, 0, p.image);
    painter.end ();
    *pixmap = mq_pixmap_from_image (opaque);
  }
  if (mask) *mask = p.transparent ? mq_bitmap_from_image (mask_of (p.image)) : None;
  fill_attributes (a, p);
  return XpmSuccess;
}

int to_images (Display *d, const XpmPicture &p, XImage **image, XImage **shape, XpmAttributes *a)
{
  int w = p.image.width (), h = p.image.height ();
  if (image) {
    *image = XCreateImage (d, nullptr, 24, ZPixmap, 0, nullptr, w, h, 32, 0);
    (*image)->data = (char *) calloc (1, (size_t) (*image)->bytes_per_line * h);
    for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++) XPutPixel (*image, x, y, p.image.pixel (x, y) & 0xffffff);
  }
  if (shape) {
    *shape = nullptr;
    if (p.transparent) {
      *shape = XCreateImage (d, nullptr, 1, XYBitmap, 0, nullptr, w, h, 8, 0);
      (*shape)->data = (char *) calloc (1, (size_t) (*shape)->bytes_per_line * h);
      for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) XPutPixel (*shape, x, y, qAlpha (p.image.pixel (x, y)) ? 1 : 0);
    }
  }
  fill_attributes (a, p);
  return XpmSuccess;
}

int read_file (const char *name, QList<QByteArray> *s)
{
  QFile f (QString::fromLocal8Bit (name));
  if (!f.open (QIODevice::ReadOnly)) return XpmOpenFailed;
  *s = xpm_strings (f.readAll ());
  return XpmSuccess;
}
}

extern "C" {
int XpmCreatePixmapFromData (Display *, Drawable, char **data, Pixmap *pixmap, Pixmap *mask, XpmAttributes *a)
{
  XpmPicture p;
  int r = parse (data_strings (data), a, &p);
  return r == XpmSuccess ? to_pixmaps (p, pixmap, mask, a) : r;
}

int XpmCreateImageFromData (Display *d, char **data, XImage **image, XImage **shape, XpmAttributes *a)
{
  XpmPicture p;
  int r = parse (data_strings (data), a, &p);
  return r == XpmSuccess ? to_images (d, p, image, shape, a) : r;
}

int XpmCreatePixmapFromBuffer (Display *, Drawable, char *buffer, Pixmap *pixmap, Pixmap *mask, XpmAttributes *a)
{
  XpmPicture p;
  int r = parse (xpm_strings (QByteArray (buffer)), a, &p);
  return r == XpmSuccess ? to_pixmaps (p, pixmap, mask, a) : r;
}

int XpmCreateImageFromBuffer (Display *d, char *buffer, XImage **image, XImage **shape, XpmAttributes *a)
{
  XpmPicture p;
  int r = parse (xpm_strings (QByteArray (buffer)), a, &p);
  return r == XpmSuccess ? to_images (d, p, image, shape, a) : r;
}

int XpmReadFileToPixmap (Display *, Drawable, const char *filename, Pixmap *pixmap, Pixmap *mask, XpmAttributes *a)
{
  QList<QByteArray> s;
  int r = read_file (filename, &s);
  XpmPicture p;
  if (r == XpmSuccess) r = parse (s, a, &p);
  return r == XpmSuccess ? to_pixmaps (p, pixmap, mask, a) : r;
}

int XpmReadFileToImage (Display *d, const char *filename, XImage **image, XImage **shape, XpmAttributes *a)
{
  QList<QByteArray> s;
  int r = read_file (filename, &s);
  XpmPicture p;
  if (r == XpmSuccess) r = parse (s, a, &p);
  return r == XpmSuccess ? to_images (d, p, image, shape, a) : r;
}

/* the data array of a file: one malloc'ed block, as XpmFree expects */
int XpmReadFileToData (const char *filename, char ***data_return)
{
  QList<QByteArray> s;
  int r = read_file (filename, &s);
  if (r != XpmSuccess) return r;
  size_t total = (s.size () + 1) * sizeof (char *);
  for (const QByteArray &l : s) total += l.size () + 1;
  char **arr = (char **) malloc (total);
  char *strings = (char *) (arr + s.size () + 1);
  for (int i = 0; i < s.size (); i++) {
    arr[i] = strings;
    memcpy (strings, s[i].constData (), s[i].size () + 1);
    strings += s[i].size () + 1;
  }
  arr[s.size ()] = nullptr;
  *data_return = arr;
  return XpmSuccess;
}

void XpmFree (void *ptr) { free (ptr); }
void XpmFreeAttributes (XpmAttributes *a)
{
  if (!a) return;
  if (a->valuemask & XpmReturnPixels) { free (a->pixels); a->pixels = nullptr; a->npixels = 0; }
  if (a->valuemask & XpmReturnAllocPixels) { free (a->alloc_pixels); a->alloc_pixels = nullptr; a->nalloc_pixels = 0; }
  a->valuemask = 0;
}

char *XpmGetErrorString (int code)
{
  switch (code) {
  case XpmColorError: return (char *) "XpmColorError";
  case XpmSuccess: return (char *) "XpmSuccess";
  case XpmOpenFailed: return (char *) "XpmOpenFailed";
  case XpmFileInvalid: return (char *) "XpmFileInvalid";
  case XpmNoMemory: return (char *) "XpmNoMemory";
  case XpmColorFailed: return (char *) "XpmColorFailed";
  default: return (char *) "Invalid XpmError";
  }
}

int XpmLibraryVersion (void) { return XpmIncludeVersion; }
} /* extern "C" */

/* ---------- XpmImage: the parsed form, colour table plus indices ---------- */
namespace {
QList<QByteArray> strings_of_image (const XpmImage *im)
{
  QList<QByteArray> s;
  s << QByteArray::number (im->width) + ' ' + QByteArray::number (im->height) + ' ' +
       QByteArray::number (im->ncolors) + ' ' + QByteArray::number (im->cpp);
  for (unsigned i = 0; i < im->ncolors; i++) {
    const XpmColor &c = im->colorTable[i];
    QByteArray line (c.string ? c.string : "", im->cpp);
    if (c.symbolic) line += QByteArray (" s ") + c.symbolic;
    if (c.m_color) line += QByteArray (" m ") + c.m_color;
    if (c.g4_color) line += QByteArray (" g4 ") + c.g4_color;
    if (c.g_color) line += QByteArray (" g ") + c.g_color;
    if (c.c_color) line += QByteArray (" c ") + c.c_color;
    s << line;
  }
  for (unsigned y = 0; y < im->height; y++) {
    QByteArray row;
    for (unsigned x = 0; x < im->width; x++) {
      unsigned idx = im->data[y * im->width + x];
      const char *chars = idx < im->ncolors && im->colorTable[idx].string ? im->colorTable[idx].string : "";
      row += QByteArray (chars, im->cpp);
    }
    s << row;
  }
  return s;
}

char *dup_or_null (const QByteArray &v) { return v.isEmpty () ? nullptr : strdup (v.constData ()); }

int image_of_strings (const QList<QByteArray> &s, XpmImage *im, XpmInfo *info)
{
  memset (im, 0, sizeof *im);
  if (s.isEmpty ()) return XpmFileInvalid;
  QList<QByteArray> hdr = s[0].simplified ().split (' ');
  if (hdr.size () < 4) return XpmFileInvalid;
  int w = hdr[0].toInt (), h = hdr[1].toInt (), nc = hdr[2].toInt (), cpp = hdr[3].toInt ();
  if (w <= 0 || h <= 0 || nc <= 0 || cpp <= 0 || s.size () < 1 + nc + h) return XpmFileInvalid;
  im->width = w;
  im->height = h;
  im->cpp = cpp;
  im->ncolors = nc;
  im->colorTable = (XpmColor *) calloc (nc, sizeof (XpmColor));
  QHash<QByteArray, unsigned> index;
  for (int i = 0; i < nc; i++) {
    const QByteArray &line = s[1 + i];
    XpmColor &c = im->colorTable[i];
    c.string = strdup (line.left (cpp).constData ());
    index.insert (line.left (cpp), i);
    QList<QByteArray> words = line.mid (cpp).simplified ().split (' ');
    QHash<QByteArray, QByteArray> vals;
    QByteArray cur;
    for (const QByteArray &wd : words) {
      if (wd == "m" || wd == "s" || wd == "g4" || wd == "g" || wd == "c") { cur = wd; vals[cur] = QByteArray (); }
      else if (!cur.isEmpty ()) vals[cur] += (vals[cur].isEmpty () ? "" : " ") + wd;
    }
    c.symbolic = dup_or_null (vals.value ("s"));
    c.m_color = dup_or_null (vals.value ("m"));
    c.g4_color = dup_or_null (vals.value ("g4"));
    c.g_color = dup_or_null (vals.value ("g"));
    c.c_color = dup_or_null (vals.value ("c"));
  }
  im->data = (unsigned int *) calloc ((size_t) w * h, sizeof (unsigned int));
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) im->data[y * w + x] = index.value (s[1 + nc + y].mid (x * cpp, cpp), 0);
  if (info) {
    memset (info, 0, sizeof *info);
    if (hdr.size () >= 6) { info->x_hotspot = hdr[4].toInt (); info->y_hotspot = hdr[5].toInt (); info->valuemask |= XpmHotspot; }
  }
  return XpmSuccess;
}
}

extern "C" {
int XpmCreatePixmapFromXpmImage (Display *, Drawable, XpmImage *im, Pixmap *pixmap, Pixmap *mask, XpmAttributes *a)
{
  XpmPicture p;
  int r = parse (strings_of_image (im), a, &p);
  return r == XpmSuccess ? to_pixmaps (p, pixmap, mask, a) : r;
}

int XpmCreateImageFromXpmImage (Display *d, XpmImage *im, XImage **image, XImage **shape, XpmAttributes *a)
{
  XpmPicture p;
  int r = parse (strings_of_image (im), a, &p);
  return r == XpmSuccess ? to_images (d, p, image, shape, a) : r;
}

int XpmCreateXpmImageFromData (char **data, XpmImage *im, XpmInfo *info) { return image_of_strings (data_strings (data), im, info); }
int XpmCreateXpmImageFromBuffer (char *buffer, XpmImage *im, XpmInfo *info) { return image_of_strings (xpm_strings (QByteArray (buffer)), im, info); }

int XpmReadFileToXpmImage (const char *filename, XpmImage *im, XpmInfo *info)
{
  QList<QByteArray> s;
  int r = read_file (filename, &s);
  return r == XpmSuccess ? image_of_strings (s, im, info) : r;
}

int XpmCreateDataFromXpmImage (char ***data_return, XpmImage *im, XpmInfo *)
{
  QList<QByteArray> s = strings_of_image (im);
  /* one block: the pointers, then the strings (freed with one XpmFree) */
  size_t size = (s.size () + 1) * sizeof (char *);
  for (const QByteArray &l : s) size += l.size () + 1;
  char **data = (char **) malloc (size);
  char *p = (char *) (data + s.size () + 1);
  for (int i = 0; i < s.size (); i++) { data[i] = p; memcpy (p, s[i].constData (), s[i].size () + 1); p += s[i].size () + 1; }
  data[s.size ()] = nullptr;
  *data_return = data;
  return XpmSuccess;
}

void XpmFreeXpmImage (XpmImage *im)
{
  if (!im) return;
  for (unsigned i = 0; im->colorTable && i < im->ncolors; i++) {
    XpmColor &c = im->colorTable[i];
    free (c.string); free (c.symbolic); free (c.m_color); free (c.g4_color); free (c.g_color); free (c.c_color);
  }
  free (im->colorTable);
  free (im->data);
  im->colorTable = nullptr;
  im->data = nullptr;
}

void XpmFreeXpmInfo (XpmInfo *info)
{
  if (!info) return;
  free (info->hints_cmt); free (info->colors_cmt); free (info->pixels_cmt);
  info->hints_cmt = info->colors_cmt = info->pixels_cmt = nullptr;
}
}
