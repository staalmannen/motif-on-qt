/*
 * Motif on Qt: the tooltip hooks of the Athena Simple widget. libXaw's Tip
 * widget pops up its own window; here a widget's XtNtip text becomes the
 * Qt tooltip of its window (mq_set_tooltip in xlib.cpp).
 */
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/SimpleP.h>
#include <X11/Xaw/Tip.h>

void mq_set_tooltip (Widget w, const char *text);

void XawTipEnable (Widget w)
{
  if (XtIsSubclass (w, simpleWidgetClass)) mq_set_tooltip (w, ((SimpleWidget) w)->simple.tip);
}

void XawTipDisable (Widget w)
{
  mq_set_tooltip (w, NULL);
}

