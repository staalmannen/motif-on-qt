/*
 * Motif on Qt - Qt style faces for the Athena widgets.
 *
 * Label, Command, Toggle, MenuButton, Repeater and Scrollbar keep all of
 * libXaw's behaviour - translations, actions, callbacks, sizes, radio
 * groups - and only look different: their widget is painted with the Qt
 * style from the state in the Xaw record (label, font, justification,
 * set, highlighted, top/shown ...), so they follow the desktop theme. Xaw
 * still draws into the window's backing image; every such drawing asks
 * for a repaint, which paints the face from the new state.
 *
 * A program's subclass that draws itself (its own expose method) keeps
 * its drawing. MOTIFQT_XAW_CLASSIC=1 shows libXaw's own drawing.
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
#include <cstring>
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
enum Face { NONE, LABEL, COMMAND, TOGGLE, MENU_BUTTON, REPEATER, SCROLLBAR, SIMPLE_MENU };

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
