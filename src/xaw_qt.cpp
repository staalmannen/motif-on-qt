/*
 * Motif on Qt - Athena widgets whose face is a real Qt widget.
 *
 * Label, Command, Toggle, MenuButton, Repeater, Scrollbar, SimpleMenu and
 * its Sme entries keep libXaw's behaviour (translations, actions, callbacks,
 * sizes, radio groups). The canvas hosts a QLabel, QPushButton, QScrollBar
 * or QFrame that follows the desktop theme; the child ignores the pointer
 * so Xaw still receives the events. Xaw's expose is not run for these
 * classes. A subclass that draws itself (its own expose method) is left
 * alone. MOTIFQT_XAW_CLASSIC=1 restores libXaw's own drawing.
 *
 * Licensed under the LGPL 2.1 or later; see LICENSE.LGPL.
 */
#include "private.h"

/* Xm.h's FMT8BIT macro clashes with Xaw's Text.h, included by some of these */
#undef FMT8BIT
#undef FMT16BIT
#include <X11/Xaw/LabelP.h>
#include <X11/Xaw/CommandP.h>
#include <X11/Xaw/ToggleP.h>
#include <X11/Xaw/MenuButtoP.h>
#include <X11/Xaw/RepeaterP.h>
#include <X11/Xaw/ScrollbarP.h>
#include <X11/Xaw/SimpleMenP.h>
#include <X11/Xaw/SmeBSBP.h>
#include <X11/Xaw/SmeLineP.h>

#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionSlider>
#include <QStyleOptionMenuItem>
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <cstring>
#include <cstdint>
#include <cstdlib>

/* weak: only programs that use these Athena classes link them */
#pragma weak labelClassRec
#pragma weak commandClassRec
#pragma weak toggleClassRec
#pragma weak menuButtonClassRec
#pragma weak repeaterClassRec
#pragma weak scrollbarClassRec
#pragma weak simpleMenuClassRec
#pragma weak smeBSBClassRec
#pragma weak smeLineClassRec

namespace {
enum Face { NONE, LABEL, COMMAND, TOGGLE, MENU_BUTTON, REPEATER, SCROLLBAR, SIMPLE_MENU, SME, SME_LINE };

bool classic ()
{
  static int c = -1;
  if (c < 0) {
    const char *e = getenv ("MOTIFQT_XAW_CLASSIC");
    c = e && *e && strcmp (e, "0") ? 1 : 0;
  }
  return c;
}

/* the nearest Athena class this widget's class is, if its class still draws
 * the way that class does (a subclass with its own expose keeps it) */
Face face_of (Widget w)
{
  struct { WidgetClass rec; Face face; } known[] = {
    { &toggleClassRec ? (WidgetClass) &toggleClassRec : nullptr, TOGGLE },
    { &menuButtonClassRec ? (WidgetClass) &menuButtonClassRec : nullptr, MENU_BUTTON },
    { &repeaterClassRec ? (WidgetClass) &repeaterClassRec : nullptr, REPEATER },
    { &commandClassRec ? (WidgetClass) &commandClassRec : nullptr, COMMAND },
    { &labelClassRec ? (WidgetClass) &labelClassRec : nullptr, LABEL },
    { &scrollbarClassRec ? (WidgetClass) &scrollbarClassRec : nullptr, SCROLLBAR },
    { &simpleMenuClassRec ? (WidgetClass) &simpleMenuClassRec : nullptr, SIMPLE_MENU },
  };
  for (WidgetClass c = w->core.widget_class; c; c = c->core_class.superclass)
    for (const auto &k : known)
      if (k.rec && c == k.rec)
        return w->core.widget_class->core_class.expose == k.rec->core_class.expose ? k.face : NONE;
  return NONE;
}

QColor xcolor (Pixel p) { return mq_color (p); }

/* a palette with the widget's own colours where the program set them (the
 * defaults are the palette's already) */
QPalette palette_for (Widget w, QWidget *qw, Pixel fg, Pixel background)
{
  QPalette pal = QApplication::palette (qw);
  QColor bg = xcolor (background), f = xcolor (fg);
  /* the program's own colours, for the enabled look only: an insensitive
   * widget keeps the style's disabled colours */
  for (auto group : { QPalette::Active, QPalette::Inactive }) {
    if (bg != pal.color (group, QPalette::Window)) {
      pal.setColor (group, QPalette::Window, bg);
      pal.setColor (group, QPalette::Button, bg);
    }
    if (f != pal.color (group, QPalette::WindowText)) {
      pal.setColor (group, QPalette::WindowText, f);
      pal.setColor (group, QPalette::ButtonText, f);
    }
  }
  if (!XtIsSensitive (w)) pal.setCurrentColorGroup (QPalette::Disabled);
  return pal;
}

QFont label_font (Widget w)
{
  LabelWidget lw = (LabelWidget) w;
  if (lw->simple.international && lw->label.fontset) return mq_fontset_qfont (lw->label.fontset);
  if (lw->label.font) return mq_font_for (lw->label.font->fid);
  return QApplication::font ();
}

QString label_text (Widget w)
{
  LabelWidget lw = (LabelWidget) w;
  if (!lw->label.label) return QString ();
  if (lw->label.encoding == XawTextEncodingChar2b) {
    /* two bytes a character */
    QString s;
    const unsigned char *p = (const unsigned char *) lw->label.label;
    for (int i = 0; i + 1 < (int) lw->label.label_len * 2 && (p[i] || p[i + 1]); i += 2) s += QChar ((p[i] << 8) | p[i + 1]);
    return s;
  }
  return mq_mb_decode (lw->label.label);
}

Qt::Alignment label_alignment (Widget w)
{
  switch (((LabelWidget) w)->label.justify) {
  case XtJustifyLeft: return Qt::AlignLeft | Qt::AlignVCenter;
  case XtJustifyRight: return Qt::AlignRight | Qt::AlignVCenter;
  default: return Qt::AlignHCenter | Qt::AlignVCenter;
  }
}

/* the label part: a pixmap, or the left bitmap and the text */
void paint_label_content (Widget w, QWidget *qw, QPainter &p, const QRect &r, const QPalette &pal, bool shifted,
                          QPalette::ColorRole role)
{
  LabelWidget lw = (LabelWidget) w;
  QRect area = r.adjusted (lw->label.internal_width, lw->label.internal_height, -lw->label.internal_width, -lw->label.internal_height);
  if (shifted) area.translate (qw->style ()->pixelMetric (QStyle::PM_ButtonShiftHorizontal, nullptr, qw),
                               qw->style ()->pixelMetric (QStyle::PM_ButtonShiftVertical, nullptr, qw));
  if (lw->label.pixmap && lw->label.pixmap != None) {
    QPixmap pm = mq_pixmap_qt (lw->label.pixmap);
    qw->style ()->drawItemPixmap (&p, area, label_alignment (w), pm);
    return;
  }
  if (lw->label.left_bitmap && lw->label.left_bitmap != None) {
    QPixmap bm = mq_pixmap_qt (lw->label.left_bitmap);
    qw->style ()->drawItemPixmap (&p, QRect (area.left (), area.top (), bm.width (), area.height ()), Qt::AlignLeft | Qt::AlignVCenter, bm);
    area.setLeft (area.left () + bm.width () + lw->label.internal_width);
  }
  p.setFont (label_font (w));
  qw->style ()->drawItemText (&p, area, label_alignment (w), pal, XtIsSensitive (w), label_text (w), role);
}

void paint_label (Widget w, QWidget *qw, QPainter &p)
{
  LabelWidget lw = (LabelWidget) w;
  QPalette pal = palette_for (w, qw, lw->label.foreground, w->core.background_pixel);
  p.fillRect (qw->rect (), pal.color (QPalette::Window));
  paint_label_content (w, qw, p, qw->rect (), pal, false, QPalette::WindowText);
}

void paint_button (Widget w, QWidget *qw, QPainter &p, Face face)
{
  CommandWidget cw = (CommandWidget) w;
  /* Xaw swaps the foreground and background while a button is set (it
   * draws itself inverted): the face shows the set state its own way */
  bool set = cw->command.set;
  Pixel fg = set ? w->core.background_pixel : cw->label.foreground;
  Pixel bg = set ? cw->label.foreground : w->core.background_pixel;
  QPalette pal = palette_for (w, qw, fg, bg);
  p.fillRect (qw->rect (), pal.color (QPalette::Window));
  QStyleOptionButton opt;
  opt.initFrom (qw);
  opt.rect = qw->rect ();
  opt.palette = pal;
  opt.state &= ~(QStyle::State_MouseOver | QStyle::State_HasFocus);
  if (XtIsSensitive (w)) opt.state |= QStyle::State_Enabled; else opt.state &= ~QStyle::State_Enabled;
  /* a toggle's set state is "on"; a command is set while pressed */
  if (face == TOGGLE) opt.state |= set ? QStyle::State_On : QStyle::State_Off;
  opt.state |= set ? QStyle::State_Sunken : QStyle::State_Raised;
  if (cw->command.highlighted != HighlightNone) opt.state |= QStyle::State_MouseOver;
  qw->style ()->drawControl (QStyle::CE_PushButtonBevel, &opt, &p, qw);
  /* the label within Xaw's own margins (what its size was computed for);
   * a menu button has no indicator in Athena, nor room for one */
  paint_label_content (w, qw, p, qw->rect (), pal, set && face != TOGGLE, QPalette::ButtonText);
}

void paint_scrollbar (Widget w, QWidget *qw, QPainter &p)
{
  ScrollbarWidget sw = (ScrollbarWidget) w;
  QPalette pal = palette_for (w, qw, sw->scrollbar.foreground, w->core.background_pixel);
  bool horizontal = sw->scrollbar.orientation == XtorientHorizontal;
  const int total = 100000;
  float shown = qBound (0.0f, sw->scrollbar.shown, 1.0f), top = qBound (0.0f, sw->scrollbar.top, 1.0f);
  QStyleOptionSlider opt;
  opt.initFrom (qw);
  opt.palette = pal;
  opt.orientation = horizontal ? Qt::Horizontal : Qt::Vertical;
  opt.state &= ~QStyle::State_HasFocus;
  if (horizontal) opt.state |= QStyle::State_Horizontal;
  opt.subControls = QStyle::SC_All;
  opt.activeSubControls = QStyle::SC_None;
  opt.pageStep = qMax (1, (int) (shown * total));
  opt.minimum = 0;
  opt.maximum = qMax (0, total - opt.pageStep);
  opt.sliderPosition = opt.sliderValue = qMin (opt.maximum, (int) (top * total));
  opt.singleStep = qMax (1, opt.pageStep / 10);
  /* Xaw's thumb runs the whole length, without arrow buttons: draw the
   * style's scroll bar stretched so that its groove is the widget */
  opt.rect = qw->rect ();
  /* the track: where the slider moves (the pages and the slider) */
  QRect groove = qw->style ()->subControlRect (QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSubPage, qw) |
                 qw->style ()->subControlRect (QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, qw) |
                 qw->style ()->subControlRect (QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarAddPage, qw);
  int before = horizontal ? groove.left () - opt.rect.left () : groove.top () - opt.rect.top ();
  int after = horizontal ? opt.rect.right () - groove.right () : opt.rect.bottom () - groove.bottom ();
  /* the stretched bar starts at 0 (styles place their parts from the
   * rectangle's size); the painter moves it so its track is the widget */
  QSize size = qw->size () + (horizontal ? QSize (before + after, 0) : QSize (0, before + after));
  opt.rect = QRect (QPoint (0, 0), size);
  p.fillRect (qw->rect (), pal.color (QPalette::Window));
  p.save ();
  p.setClipRect (qw->rect ());
  p.translate (horizontal ? -before : 0, horizontal ? 0 : -before);
  qw->style ()->drawComplexControl (QStyle::CC_ScrollBar, &opt, &p, qw);
  p.restore ();
}

bool is_class (Widget w, void *rec)
{
  if (!rec) return false;
  for (WidgetClass c = w->core.widget_class; c; c = c->core_class.superclass)
    if (c == (WidgetClass) rec) return true;
  return false;
}

/* a menu: the style's menu panel, its entries as menu items (highlighted
 * where Athena highlights: simple_menu.entry_set), lines as separators;
 * entries of other classes keep Xaw's drawing (the backing image) */
void paint_simple_menu (Widget w, QWidget *qw, QPainter &p, const QImage *img)
{
  SimpleMenuWidget smw = (SimpleMenuWidget) w;
  QPalette pal = palette_for (w, qw, BlackPixelOfScreen (XtScreen (w)), w->core.background_pixel);
  pal.setCurrentColorGroup (QPalette::Active);
  p.fillRect (qw->rect (), pal.color (QPalette::Window));
  QStyleOption frame;
  frame.initFrom (qw);
  frame.rect = qw->rect ();
  frame.palette = pal;
  qw->style ()->drawPrimitive (QStyle::PE_PanelMenu, &frame, &p, qw);
  for (Cardinal i = 0; i < smw->composite.num_children; i++) {
    Widget e = smw->composite.children[i];
    if (!XtIsManaged (e)) continue;
    QRect r (e->core.x, e->core.y, e->core.width, e->core.height);
    QStyleOptionMenuItem opt;
    opt.initFrom (qw);
    opt.rect = r;
    opt.palette = pal;
    opt.state &= ~(QStyle::State_MouseOver | QStyle::State_HasFocus);
    opt.menuRect = qw->rect ();
    opt.maxIconWidth = 0;
    opt.reservedShortcutWidth = 0;
    if (is_class (e, &smeLineClassRec)) {
      opt.menuItemType = QStyleOptionMenuItem::Separator;
      qw->style ()->drawControl (QStyle::CE_MenuItem, &opt, &p, qw);
    } else if (is_class (e, &smeBSBClassRec) && e->core.widget_class->core_class.expose == smeBSBClassRec.rect_class.expose) {
      /* the style draws the item's background and highlight; the label
       * keeps Athena's margins and justification (its size was computed
       * for them) */
      SmeBSBObject b = (SmeBSBObject) e;
      bool enabled = XtIsSensitive (e), selected = enabled && (Widget) smw->simple_menu.entry_set == e;
      opt.menuItemType = QStyleOptionMenuItem::Normal;
      if (enabled) opt.state |= QStyle::State_Enabled; else opt.state &= ~QStyle::State_Enabled;
      if (selected) opt.state |= QStyle::State_Selected;
      qw->style ()->drawControl (QStyle::CE_MenuItem, &opt, &p, qw);
      QPalette ipal = pal;
      ipal.setCurrentColorGroup (enabled ? QPalette::Active : QPalette::Disabled);
      QRect area = r.adjusted (b->sme_bsb.left_margin, 0, -b->sme_bsb.right_margin, 0);
      if (b->sme_bsb.left_bitmap && b->sme_bsb.left_bitmap != None)
        qw->style ()->drawItemPixmap (&p, QRect (r.left () + (b->sme_bsb.left_margin - b->sme_bsb.left_bitmap_width) / 2, r.top (),
                                                 b->sme_bsb.left_bitmap_width, r.height ()), Qt::AlignCenter, mq_pixmap_qt (b->sme_bsb.left_bitmap));
      if (b->sme_bsb.right_bitmap && b->sme_bsb.right_bitmap != None)
        qw->style ()->drawItemPixmap (&p, QRect (r.right () - b->sme_bsb.right_margin + 1, r.top (), b->sme_bsb.right_margin, r.height ()),
                                      Qt::AlignCenter, mq_pixmap_qt (b->sme_bsb.right_bitmap));
      Qt::Alignment align = b->sme_bsb.justify == XtJustifyRight ? Qt::AlignRight : b->sme_bsb.justify == XtJustifyCenter ? Qt::AlignHCenter : Qt::AlignLeft;
      p.setFont (b->sme.international && b->sme_bsb.fontset ? mq_fontset_qfont (b->sme_bsb.fontset)
                 : b->sme_bsb.font ? mq_font_for (b->sme_bsb.font->fid) : qw->font ());
      qw->style ()->drawItemText (&p, area, align | Qt::AlignVCenter, ipal, enabled,
                                  b->sme_bsb.label ? mq_mb_decode (b->sme_bsb.label) : QString (),
                                  selected ? QPalette::HighlightedText : QPalette::Text);
    } else if (img) {
      /* another kind of entry: what Xaw drew for it */
      qreal d = img->devicePixelRatio ();
      p.drawImage (QRectF (r), *img, QRectF (r.x () * d, r.y () * d, r.width () * d, r.height () * d));
    }
  }
}

QWidget *face_child (QWidget *canvas)
{
  return canvas->findChild<QWidget *> ("mq-xaw-face", Qt::FindDirectChildrenOnly);
}

QWidget *ensure_face (QWidget *canvas, Face face)
{
  QWidget *c = face_child (canvas);
  if (c && c->property ("face").toInt () != (int) face) {
    c->hide ();
    c->deleteLater ();
    c = nullptr;
  }
  if (!c && face != SIMPLE_MENU && face != NONE) {
    switch (face) {
    case LABEL:
      c = new QLabel (canvas);
      break;
    case SCROLLBAR:
      c = new QScrollBar (canvas);
      break;
    case SME_LINE: {
      auto *f = new QFrame (canvas);
      f->setFrameShape (QFrame::HLine);
      f->setFrameShadow (QFrame::Sunken);
      c = f;
      break;
    }
    default:
      c = new QPushButton (canvas);
      break;
    }
    c->setObjectName ("mq-xaw-face");
    c->setProperty ("face", (int) face);
    c->setAttribute (Qt::WA_TransparentForMouseEvents);
    c->setFocusPolicy (Qt::NoFocus);
    c->show ();
  }
  if (c) c->setGeometry (canvas->rect ());
  return c;
}

void sync_button (Widget w, QPushButton *b, Face face)
{
  CommandWidget cw = (CommandWidget) w;
  bool set = cw->command.set;
  b->setFont (label_font (w));
  b->setText (label_text (w));
  b->setEnabled (XtIsSensitive (w));
  b->setFlat (false);
  if (face == TOGGLE) {
    b->setCheckable (true);
    b->setChecked (set);
    b->setDown (false);
  } else {
    b->setCheckable (false);
    b->setDown (set);
  }
  Pixmap pix = ((LabelWidget) w)->label.pixmap;
  if (!pix || pix == None) pix = ((LabelWidget) w)->label.left_bitmap;
  b->setIcon (pix && pix != None ? mq_pixmap_qt (pix) : QPixmap ());
  b->setPalette (palette_for (w, b, set ? w->core.background_pixel : cw->label.foreground,
                              set ? cw->label.foreground : w->core.background_pixel));
}

void sync_menu (Widget w, QWidget *canvas)
{
  SimpleMenuWidget smw = (SimpleMenuWidget) w;
  QPalette pal = palette_for (w, canvas, BlackPixelOfScreen (XtScreen (w)), w->core.background_pixel);
  canvas->setAutoFillBackground (true);
  canvas->setPalette (pal);
  QList<Widget> live;
  for (Cardinal i = 0; i < smw->composite.num_children; i++) {
    Widget e = smw->composite.children[i];
    if (XtIsManaged (e)) live.append (e);
  }
  const QList<QWidget *> rows = canvas->findChildren<QWidget *> ("mq-xaw-entry", Qt::FindDirectChildrenOnly);
  for (QWidget *row : rows) {
    Widget e = (Widget) (uintptr_t) row->property ("xaw").toULongLong ();
    if (!live.contains (e)) { row->hide (); row->deleteLater (); }
  }
  for (Widget e : live) {
    QWidget *row = nullptr;
    for (QWidget *c : canvas->findChildren<QWidget *> ("mq-xaw-entry", Qt::FindDirectChildrenOnly))
      if ((Widget) (uintptr_t) c->property ("xaw").toULongLong () == e) { row = c; break; }
    bool line = is_class (e, &smeLineClassRec);
    bool bsb = is_class (e, &smeBSBClassRec);
    if (!line && !bsb) continue;
    if (!row) {
      if (line) {
        auto *f = new QFrame (canvas);
        f->setFrameShape (QFrame::HLine);
        f->setFrameShadow (QFrame::Sunken);
        row = f;
      } else row = new QLabel (canvas);
      row->setObjectName ("mq-xaw-entry");
      row->setProperty ("xaw", (qulonglong) (uintptr_t) e);
      row->setAttribute (Qt::WA_TransparentForMouseEvents);
      row->setFocusPolicy (Qt::NoFocus);
    }
    row->setGeometry (e->core.x, e->core.y, e->core.width, e->core.height);
    row->show ();
    if (!bsb) continue;
    auto *lab = qobject_cast<QLabel *> (row);
    if (!lab) continue;
    SmeBSBObject b = (SmeBSBObject) e;
    bool enabled = XtIsSensitive (e);
    bool selected = enabled && (Widget) smw->simple_menu.entry_set == e;
    lab->setEnabled (enabled);
    lab->setFont (b->sme.international && b->sme_bsb.fontset ? mq_fontset_qfont (b->sme_bsb.fontset)
                  : b->sme_bsb.font ? mq_font_for (b->sme_bsb.font->fid) : canvas->font ());
    lab->setText (b->sme_bsb.label ? mq_mb_decode (b->sme_bsb.label) : QString ());
    lab->setAlignment ((b->sme_bsb.justify == XtJustifyRight ? Qt::AlignRight
                        : b->sme_bsb.justify == XtJustifyCenter ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::AlignVCenter);
    lab->setContentsMargins (b->sme_bsb.left_margin, 0, b->sme_bsb.right_margin, 0);
    QPalette ip = pal;
    if (selected) {
      ip.setColor (QPalette::Window, ip.color (QPalette::Highlight));
      ip.setColor (QPalette::WindowText, ip.color (QPalette::HighlightedText));
    }
    lab->setAutoFillBackground (true);
    lab->setPalette (ip);
    if (b->sme_bsb.left_bitmap && b->sme_bsb.left_bitmap != None) lab->setPixmap (mq_pixmap_qt (b->sme_bsb.left_bitmap));
  }
}

} /* namespace */

bool mq_xaw_qt_face (Widget w)
{
  if (classic () || !w || !R (w)->klass->foreign) return false;
  return face_of (w) != NONE;
}

void mq_xaw_sync (Widget w, QWidget *qw)
{
  Face face = face_of (w);
  if (face == NONE || !qw) return;
  if (face == SIMPLE_MENU) {
    QWidget *old = face_child (qw);
    if (old) { old->hide (); old->deleteLater (); }
    sync_menu (w, qw);
    return;
  }
  QWidget *c = ensure_face (qw, face);
  if (!c) return;
  switch (face) {
  case LABEL: {
    auto *lab = qobject_cast<QLabel *> (c);
    if (!lab) break;
    LabelWidget lw = (LabelWidget) w;
    lab->setFont (label_font (w));
    lab->setEnabled (XtIsSensitive (w));
    lab->setAlignment (label_alignment (w));
    lab->setContentsMargins (lw->label.internal_width, lw->label.internal_height, lw->label.internal_width, lw->label.internal_height);
    lab->setPalette (palette_for (w, lab, lw->label.foreground, w->core.background_pixel));
    if (lw->label.pixmap && lw->label.pixmap != None) {
      lab->setPixmap (mq_pixmap_qt (lw->label.pixmap));
      lab->setText (QString ());
    } else {
      lab->setPixmap (QPixmap ());
      lab->setText (label_text (w));
    }
    break;
  }
  case COMMAND:
  case TOGGLE:
  case MENU_BUTTON:
  case REPEATER:
    if (auto *b = qobject_cast<QPushButton *> (c)) sync_button (w, b, face);
    break;
  case SCROLLBAR: {
    auto *sb = qobject_cast<QScrollBar *> (c);
    if (!sb) break;
    ScrollbarWidget sw = (ScrollbarWidget) w;
    bool horizontal = sw->scrollbar.orientation == XtorientHorizontal;
    const int total = 100000;
    float shown = qBound (0.0f, sw->scrollbar.shown, 1.0f);
    float top = qBound (0.0f, sw->scrollbar.top, 1.0f);
    int page = qMax (1, (int) (shown * total));
    sb->setOrientation (horizontal ? Qt::Horizontal : Qt::Vertical);
    sb->setEnabled (XtIsSensitive (w));
    sb->setRange (0, qMax (0, total - page));
    sb->setPageStep (page);
    sb->setValue (qMin (sb->maximum (), (int) (top * total)));
    sb->setPalette (palette_for (w, sb, sw->scrollbar.foreground, w->core.background_pixel));
    break;
  }
  default:
    break;
  }
}

bool mq_xaw_paint (Widget w, QWidget *qw, QPainter &p)
{
  if (classic () || !w || !R (w)->klass->foreign) return false;
  Face face = face_of (w);
  switch (face) {
  case NONE: return false;
  case LABEL: paint_label (w, qw, p); return true;
  case SCROLLBAR: paint_scrollbar (w, qw, p); return true;
  case SIMPLE_MENU: paint_simple_menu (w, qw, p, mq_window_image (mq_window_for (w), false)); return true;
  default: paint_button (w, qw, p, face); return true;
  }
}
