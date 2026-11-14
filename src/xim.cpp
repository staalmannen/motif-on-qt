/*
 * Motif on Qt - input methods: XIM/XIC (Xlib) and XmIm (Motif) on Qt's
 * input method support.
 *
 * Program widgets (MqCanvas: drawing areas, Athena widgets, NEdit's text
 * widget ...) take part in Qt's input method: dead keys, compose, ibus,
 * fcitx. Committed text reaches the program as ordinary key presses, one
 * per character, so XLookupString, the Xmb/Xutf8/Xwc lookups and
 * translation tables all see it, with or without an input context.
 *
 * XOpenIM and XCreateIC succeed and offer the "over the spot" and "root"
 * styles. The spot location a program gives (XNSpotLocation,
 * XmNspotLocation) answers Qt's cursor rectangle query, so the candidate
 * window opens there, and the pre-edit text is shown in a small popup at
 * the spot - as an X input method server draws it for over-the-spot
 * clients.
 *
 * Licensed under the LGPL 2.1 or later; see LICENSE.LGPL.
 */
#include "private.h"
#include <Xm/XmIm.h>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QLabel>
#include <QGuiApplication>
#include <QInputMethod>
#include <cstdarg>
#include <clocale>
#include <cstring>

/* ---------------------------------------------------------------- per widget */
namespace {
struct ImState {
  QPoint spot { -1, -1 };      /* baseline position of the cursor, widget coordinates */
  QPointer<QLabel> preedit;
};

QHash<QWidget *, ImState> &states ()
{
  static auto *h = new QHash<QWidget *, ImState>;
  return *h;
}

QWidget *widget_of_window (Window w)
{
  Widget xw = w ? mq_widget_for_window (w) : nullptr;
  return xw ? mq_qw (xw) : nullptr;
}

void enable (QWidget *qw)
{
  if (!qw) return;
  qw->setAttribute (Qt::WA_InputMethodEnabled);
  if (!states ().contains (qw)) {
    states ().insert (qw, ImState ());
    QObject::connect (qw, &QObject::destroyed, [qw] () {
      ImState st = states ().take (qw);
      if (st.preedit) st.preedit->deleteLater ();
    });
  }
}

void set_spot (QWidget *qw, QPoint p)
{
  if (!qw) return;
  enable (qw);
  states ()[qw].spot = p;
  if (qw->hasFocus ()) QGuiApplication::inputMethod ()->update (Qt::ImCursorRectangle);
  ImState &st = states ()[qw];
  if (st.preedit && st.preedit->isVisible ()) st.preedit->move (qw->mapToGlobal (QPoint (p.x (), p.y () - qw->fontMetrics ().ascent ())));
}

QRect cursor_rect (QWidget *qw)
{
  QFontMetrics fm = qw->fontMetrics ();
  QPoint spot = states ().value (qw).spot;
  if (spot.x () < 0) spot = QPoint (0, fm.ascent ());   /* no spot given: the top left corner */
  return QRect (spot.x (), spot.y () - fm.ascent (), 1, fm.height ());
}

void show_preedit (QWidget *qw, const QString &text)
{
  ImState &st = states ()[qw];
  if (text.isEmpty ()) {
    if (st.preedit) st.preedit->hide ();
    return;
  }
  if (!st.preedit) {
    st.preedit = new QLabel;
    st.preedit->setWindowFlags (Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    st.preedit->setAttribute (Qt::WA_ShowWithoutActivating);
    st.preedit->setObjectName ("mq_preedit");
    st.preedit->setFont (qw->font ());
    st.preedit->setContentsMargins (1, 0, 1, 0);
    st.preedit->setAutoFillBackground (true);
  }
  st.preedit->setText ("<u>" + text.toHtmlEscaped () + "</u>");
  st.preedit->adjustSize ();
  st.preedit->move (qw->mapToGlobal (cursor_rect (qw).topLeft ()));
  st.preedit->show ();
}
}

bool mq_im_query (QWidget *qw, Qt::InputMethodQuery q, QVariant *out)
{
  if (!qw->testAttribute (Qt::WA_InputMethodEnabled)) return false;
  switch (q) {
  case Qt::ImEnabled: *out = true; return true;
  case Qt::ImCursorRectangle: *out = cursor_rect (qw); return true;
  case Qt::ImFont: *out = qw->font (); return true;
  case Qt::ImHints: *out = (int) Qt::ImhNone; return true;
  case Qt::ImCursorPosition: case Qt::ImAnchorPosition: *out = 0; return true;
  case Qt::ImSurroundingText: case Qt::ImCurrentSelection: case Qt::ImTextBeforeCursor: case Qt::ImTextAfterCursor:
    *out = QString (); return true;
  default: return false;
  }
}

void mq_im_event (QWidget *qw, QInputMethodEvent *e)
{
  show_preedit (qw, e->preeditString ());
  /* the committed text: one key press and release per character, as if
   * typed (the keysym is the character's) */
  QPointer<QWidget> guard (qw);
  for (uint c : e->commitString ().toUcs4 ()) {
    if (!guard) break;
    QString t = QString::fromUcs4 ((const char32_t *) &c, 1);
    QKeyEvent press (MQ_QKeyPress, Qt::Key_unknown, Qt::NoModifier, t);
    QCoreApplication::sendEvent (qw, &press);
    if (!guard) break;
    QKeyEvent release (MQ_QKeyRelease, Qt::Key_unknown, Qt::NoModifier, t);
    QCoreApplication::sendEvent (qw, &release);
  }
  e->accept ();
}

void mq_im_enable (QWidget *qw) { enable (qw); }

/* ---------------------------------------------------------------- Xlib XIM */
struct _XIM {
  Display *display;
  QByteArray locale;
};

struct _XIC {
  XIM im;
  XIMStyle style = XIMPreeditNothing | XIMStatusNothing;
  Window client = 0, focus = 0;
  XPoint spot { 0, 0 };
  bool has_spot = false;
};

namespace {
/* the input styles this "server" offers: over the spot (pre-edit at the
 * spot, drawn by us), root (pre-edit drawn by us where Qt puts it), none */
XIMStyle offered[] = {
  XIMPreeditPosition | XIMStatusNothing, XIMPreeditPosition | XIMStatusNone,
  XIMPreeditNothing | XIMStatusNothing, XIMPreeditNothing | XIMStatusNone,
  XIMPreeditNone | XIMStatusNone,
};

QWidget *focus_widget (XIC ic) { return widget_of_window (ic->focus ? ic->focus : ic->client); }

void apply_spot (XIC ic)
{
  if (ic->has_spot) set_spot (focus_widget (ic), QPoint (ic->spot.x, ic->spot.y));
}

/* name/value pairs up to a NULL name; nested lists (XVaCreateNestedList:
 * the pairs again, NULL-terminated) are walked in place */
using Pairs = QList<QPair<const char *, void *>>;

void add_nested (Pairs &out, void *list)
{
  void **p = (void **) list;
  for (int i = 0; p && p[i]; i += 2) {
    const char *name = (const char *) p[i];
    if (!strcmp (name, XNVaNestedList)) add_nested (out, p[i + 1]);
    else out << qMakePair (name, p[i + 1]);
  }
}

Pairs read_pairs (va_list ap)
{
  Pairs out;
  while (const char *name = va_arg (ap, const char *)) {
    void *value = va_arg (ap, void *);
    if (!strcmp (name, XNVaNestedList)) add_nested (out, value);
    else out << qMakePair (name, value);
  }
  return out;
}

/* returns the name of the first argument it does not know, as Xlib does */
const char *set_ic (XIC ic, const Pairs &args)
{
  for (const auto &a : args) {
    const char *n = a.first;
    if (!strcmp (n, XNInputStyle)) ic->style = (XIMStyle) (uintptr_t) a.second;
    else if (!strcmp (n, XNClientWindow)) ic->client = (Window) (uintptr_t) a.second;
    else if (!strcmp (n, XNFocusWindow)) ic->focus = (Window) (uintptr_t) a.second;
    else if (!strcmp (n, XNPreeditAttributes) || !strcmp (n, XNStatusAttributes)) {
      Pairs nested;
      add_nested (nested, a.second);
      for (const auto &b : nested)
        if (!strcmp (b.first, XNSpotLocation) && b.second) {
          ic->spot = *(XPoint *) b.second;
          ic->has_spot = true;
        }
    } else if (!strcmp (n, XNSpotLocation) && a.second) {
      ic->spot = *(XPoint *) a.second;
      ic->has_spot = true;
    } else if (!strcmp (n, XNResourceName) || !strcmp (n, XNResourceClass) || !strcmp (n, XNGeometryCallback) ||
               !strcmp (n, XNDestroyCallback) || !strcmp (n, XNStringConversionCallback) ||
               !strcmp (n, XNResetState) || !strcmp (n, XNHotKey) || !strcmp (n, XNHotKeyState) ||
               !strcmp (n, XNPreeditState) || !strcmp (n, XNVaNestedList)) {
      /* accepted, nothing to do */
    } else return n;
  }
  if (focus_widget (ic)) enable (focus_widget (ic));
  apply_spot (ic);
  return nullptr;
}
}

extern "C" {
XIM XOpenIM (Display *d, struct _XrmHashBucketRec *, char *, char *)
{
  auto *im = new _XIM;
  im->display = d ? d : mq_display;
  const char *l = setlocale (LC_CTYPE, nullptr);
  im->locale = l ? l : "C";
  return im;
}

Status XCloseIM (XIM im) { delete im; return 1; }
Display *XDisplayOfIM (XIM im) { return im ? im->display : mq_display; }
char *XLocaleOfIM (XIM im) { return im ? im->locale.data () : (char *) "C"; }

char *XGetIMValues (XIM, ...)
{
  va_list ap;
  va_start (ap, 0);
  Pairs args = read_pairs (ap);
  va_end (ap);
  for (const auto &a : args) {
    if (!strcmp (a.first, XNQueryInputStyle)) {
      int n = sizeof offered / sizeof offered[0];
      auto *styles = (XIMStyles *) malloc (sizeof (XIMStyles) + n * sizeof (XIMStyle));
      styles->count_styles = n;
      styles->supported_styles = (XIMStyle *) (styles + 1);
      memcpy (styles->supported_styles, offered, sizeof offered);
      *(XIMStyles **) a.second = styles;   /* the caller XFree's it */
    } else return (char *) a.first;
  }
  return nullptr;
}

char *XSetIMValues (XIM, ...) { return nullptr; }

XIC XCreateIC (XIM im, ...)
{
  auto *ic = new _XIC;
  ic->im = im;
  va_list ap;
  va_start (ap, im);
  Pairs args = read_pairs (ap);
  va_end (ap);
  set_ic (ic, args);
  return ic;
}

void XDestroyIC (XIC ic)
{
  if (!ic) return;
  if (QWidget *qw = focus_widget (ic)) show_preedit (qw, QString ());
  delete ic;
}

char *XSetICValues (XIC ic, ...)
{
  if (!ic) return nullptr;
  va_list ap;
  va_start (ap, ic);
  Pairs args = read_pairs (ap);
  va_end (ap);
  return (char *) set_ic (ic, args);
}

char *XGetICValues (XIC ic, ...)
{
  if (!ic) return nullptr;
  va_list ap;
  va_start (ap, ic);
  Pairs args = read_pairs (ap);
  va_end (ap);
  for (const auto &a : args) {
    if (!strcmp (a.first, XNFilterEvents)) *(unsigned long *) a.second = KeyPressMask | KeyReleaseMask;
    else if (!strcmp (a.first, XNInputStyle)) *(XIMStyle *) a.second = ic->style;
    else if (!strcmp (a.first, XNClientWindow)) *(Window *) a.second = ic->client;
    else if (!strcmp (a.first, XNFocusWindow)) *(Window *) a.second = ic->focus ? ic->focus : ic->client;
    else return (char *) a.first;
  }
  return nullptr;
}

XIM XIMOfIC (XIC ic) { return ic ? ic->im : nullptr; }

void XSetICFocus (XIC ic)
{
  if (!ic) return;
  enable (focus_widget (ic));
  apply_spot (ic);
}

void XUnsetICFocus (XIC ic)
{
  if (QWidget *qw = ic ? focus_widget (ic) : nullptr) show_preedit (qw, QString ());
}

char *XmbResetIC (XIC ic)
{
  if (QWidget *qw = ic ? focus_widget (ic) : nullptr) show_preedit (qw, QString ());
  QGuiApplication::inputMethod ()->reset ();
  return nullptr;
}
char *Xutf8ResetIC (XIC ic) { return XmbResetIC (ic); }
wchar_t *XwcResetIC (XIC ic) { XmbResetIC (ic); return nullptr; }

/* Qt filters keys for the input method before they reach us */
Bool XFilterEvent (XEvent *, Window) { return False; }

/* the input method is always there: tell the program at once */
Bool XRegisterIMInstantiateCallback (Display *d, struct _XrmHashBucketRec *, char *, char *, XIDProc proc, XPointer client_data)
{
  if (proc) proc (d ? d : mq_display, client_data, nullptr);
  return True;
}
Bool XUnregisterIMInstantiateCallback (Display *, struct _XrmHashBucketRec *, char *, char *, XIDProc, XPointer) { return True; }

/* ---------------------------------------------------------------- Motif XmIm */
static void xm_im_args (Widget w, ArgList args, Cardinal n, bool focus)
{
  QWidget *qw = mq_qw (w);
  if (!qw) return;
  enable (qw);
  for (Cardinal i = 0; i < n; i++)
    if (!strcmp (args[i].name, XmNspotLocation) && args[i].value)
      set_spot (qw, QPoint (((XPoint *) args[i].value)->x, ((XPoint *) args[i].value)->y));
  if (focus && qw->isVisible () && !qw->hasFocus ()) { /* Motif's traversal gives the focus */ }
}

static QVector<Arg> va_args (va_list ap)
{
  QVector<Arg> out;
  while (const char *name = va_arg (ap, const char *)) {
    XtArgVal v = va_arg (ap, XtArgVal);
    Arg a;
    XtSetArg (a, (String) name, v);
    out << a;
  }
  return out;
}

void XmImRegister (Widget w, unsigned int) { if (QWidget *qw = mq_qw (w)) enable (qw); }
void XmImUnregister (Widget w) { if (QWidget *qw = mq_qw (w)) { show_preedit (qw, QString ()); qw->setAttribute (Qt::WA_InputMethodEnabled, false); } }
void XmImSetValues (Widget w, ArgList args, Cardinal n) { xm_im_args (w, args, n, false); }
void XmImSetFocusValues (Widget w, ArgList args, Cardinal n) { xm_im_args (w, args, n, true); }
void XmImVaSetValues (Widget w, ...)
{
  va_list ap;
  va_start (ap, w);
  QVector<Arg> a = va_args (ap);
  va_end (ap);
  xm_im_args (w, a.data (), a.size (), false);
}
void XmImVaSetFocusValues (Widget w, ...)
{
  va_list ap;
  va_start (ap, w);
  QVector<Arg> a = va_args (ap);
  va_end (ap);
  xm_im_args (w, a.data (), a.size (), true);
}
void XmImUnsetFocus (Widget w) { if (QWidget *qw = mq_qw (w)) show_preedit (qw, QString ()); }

static XIM s_xm_im;
XIM XmImGetXIM (Widget) { if (!s_xm_im) s_xm_im = XOpenIM (mq_display, nullptr, nullptr, nullptr); return s_xm_im; }
void XmImCloseXIM (Widget) {}
XIC XmImGetXIC (Widget w, XmInputPolicy, ArgList args, Cardinal n)
{
  XmImRegister (w, 0);
  XmImSetValues (w, args, n);
  return XCreateIC (XmImGetXIM (w), XNInputStyle, (XIMStyle) (XIMPreeditPosition | XIMStatusNothing),
                    XNClientWindow, XtWindow (w), XNFocusWindow, XtWindow (w), NULL);
}
XIC XmImSetXIC (Widget, XIC ic) { return ic; }
void XmImFreeXIC (Widget, XIC ic) { XDestroyIC (ic); }
void XmImMbResetIC (Widget w, char **mb)
{
  if (mb) *mb = nullptr;
  XmImUnsetFocus (w);
  QGuiApplication::inputMethod ()->reset ();
}
XIMResetState XmImGetXICResetState (Widget) { return XIMInitialState; }
} /* extern "C" */
