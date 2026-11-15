/*
 * Native Qt faces for the simplest Athena widgets.
 * The genuine Xaw Xt classes and resources remain in use; this file only
 * supplies their Qt presentation and mirrors their private state.
 * Licensed under the LGPL 2.1 or later; see LICENSE.LGPL.
 */
#include "private.h"

#include <X11/Xaw/CommandP.h>
#include <X11/Xaw/LabelP.h>
#include <X11/Xaw/SimpleP.h>
#include <X11/Xaw/ToggleP.h>

#include <cstring>

static bool xaw_is (Widget w, const char *name)
{
  for (MqClass *k = w ? R (w)->klass : nullptr; k; k = k->superclass)
    if (!strcmp (k->class_name, name)) return true;
  return false;
}

static void xaw_call (Widget w, XtPointer data)
{
  XtCallCallbacks (w, XtNcallback, data);
}

static void xaw_native_clicked (Widget w)
{
  if (xaw_is (w, "Toggle")) {
    ToggleWidget tw = (ToggleWidget) w;
    xaw_call (w, (XtPointer) (long) !!tw->command.set);
  } else {
    xaw_call (w, nullptr);
  }
}

QObject *mq_xaw_create (Widget w)
{
  if (xaw_is (w, "Toggle")) {
    auto *button = new QCheckBox;
    button->setProperty ("_mq_xaw_native", true);
    QObject::connect (button, &QCheckBox::clicked, button, [w] () { xaw_native_clicked (w); });
    return button;
  }
  if (xaw_is (w, "Command")) {
    auto *button = new QPushButton;
    button->setProperty ("_mq_xaw_native", true);
    QObject::connect (button, &QPushButton::clicked, button, [w] () { xaw_native_clicked (w); });
    return button;
  }
  if (xaw_is (w, "Label")) {
    auto *label = new QLabel;
    label->setProperty ("_mq_xaw_native", true);
    label->setTextFormat (Qt::PlainText);
    return label;
  }
  return nullptr;                 /* retain the existing canvas/Xlib path */
}

bool mq_xaw_native (Widget w)
{
  QObject *o = w ? R (w)->obj.data () : nullptr;
  return o && o->property ("_mq_xaw_native").toBool ();
}

static void sync_label (Widget w, QLabel *label)
{
  LabelWidget lw = (LabelWidget) w;
  QString text = lw->label.label ? mq_mb_decode (lw->label.label) : QString ();
  Pixmap p = lw->label.pixmap;
  if (p && p != None) {
    label->setText (QString ());
    label->setPixmap (mq_pixmap_qt (p));
  } else {
    label->setPixmap (QPixmap ());
    label->setText (text);
  }
  switch (lw->label.justify) {
  case XtJustifyLeft: label->setAlignment (Qt::AlignLeft | Qt::AlignVCenter); break;
  case XtJustifyRight: label->setAlignment (Qt::AlignRight | Qt::AlignVCenter); break;
  default: label->setAlignment (Qt::AlignHCenter | Qt::AlignVCenter); break;
  }
  label->setContentsMargins (lw->label.internal_width, lw->label.internal_height,
                             lw->label.internal_width, lw->label.internal_height);
  if (lw->label.font) label->setFont (mq_font_for (lw->label.font->fid));
  if (lw->simple.tip) label->setToolTip (mq_mb_decode (lw->simple.tip));
  QPalette pal = label->palette ();
  pal.setColor (QPalette::WindowText, mq_color (lw->label.foreground));
  label->setPalette (pal);
}

static void sync_button (Widget w, QAbstractButton *button)
{
  LabelWidget lw = (LabelWidget) w;
  QString text = lw->label.label ? mq_mb_decode (lw->label.label) : QString ();
  button->setText (text);
  Pixmap p = lw->label.pixmap;
  button->setIcon (p && p != None ? QIcon (mq_pixmap_qt (p)) : QIcon ());
  if (p && p != None) button->setIconSize (mq_pixmap_qt (p).size ());
  if (lw->label.font) button->setFont (mq_font_for (lw->label.font->fid));
  if (lw->simple.tip) button->setToolTip (mq_mb_decode (lw->simple.tip));

  QPalette pal = button->palette ();
  pal.setColor (QPalette::ButtonText, mq_color (lw->label.foreground));
  pal.setColor (QPalette::WindowText, mq_color (lw->label.foreground));
  pal.setColor (QPalette::Button, mq_color (w->core.background_pixel));
  button->setPalette (pal);

  if (xaw_is (w, "Toggle")) {
    ToggleWidget tw = (ToggleWidget) w;
    QSignalBlocker blocker (button);
    button->setChecked (tw->command.set != False);
  }
}

void mq_xaw_sync (Widget w)
{
  if (!mq_xaw_native (w)) return;
  if (auto *label = mq_q<QLabel> (w)) sync_label (w, label);
  else if (auto *button = mq_q<QAbstractButton> (w)) sync_button (w, button);
}
