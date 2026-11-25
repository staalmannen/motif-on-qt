/*
 * Motif on Qt - scripted input for headless testing.
 *
 * MOTIFQT_SCRIPT="wait 500; key Ctrl+N; click quit; ..." runs these steps
 * one after another (starting once the event loop runs):
 *   wait <ms>                      pause
 *   key [<widget>] <keys>          key presses (QKeySequence syntax: n, Ctrl+Q, PgDown)
 *   click <widget> [1|2|3] [x y]   press and release a mouse button
 *   drag <widget> x1 y1 x2 y2      drag with button 1 held
 *   type <widget> <text>           type text
 *   shot <file.png>                save the top-level windows
 *   dump                           print the widget tree
 *   quit                           exit(0)
 * <widget> is a Motif widget name (the first match, searching all windows),
 * optionally a path such as toolBar.quit.
 * Licensed under the LGPL 2.1 or later; see LICENSE.LGPL.
 */
#include "private.h"
#include <QInputMethodEvent>
#include <cstdio>

void mq_dump_widgets ();
void mq_save_windows (const QString &file);

/* a widget by the text it shows: a button's or label's text, a window's
   title (XView and XForms objects have labels, not Motif names) */
static QString shown_text (QString t) { return t.remove ('&').trimmed (); }
static QWidget *find_by_text (const QList<QWidget *> &scope, const QString &text)
{
  for (QWidget *r : scope) {
    if (r->isWindow () && r->windowTitle () == text) return r;
    QList<QWidget *> all = r->findChildren<QWidget *> ();
    all.prepend (r);
    for (QWidget *w : all) {
      if (!w->isVisible ()) continue;
      if (auto *b = qobject_cast<QAbstractButton *> (w); b && shown_text (b->text ()) == text) return w;
      if (auto *l = qobject_cast<QLabel *> (w); l && shown_text (l->text ()) == text) return w;
    }
  }
  return nullptr;
}

static QWidget *find_widget (const QString &spec)
{
  QStringList path = spec.split ('.');
  QList<QWidget *> roots;
  for (QWidget *t : QApplication::topLevelWidgets ()) if (t->isVisible ()) roots << t;
  QList<QWidget *> scope = roots;
  QWidget *found = nullptr;
  for (const QString &part : path) {
    found = nullptr;
    for (QWidget *r : scope) {
      if (r->objectName () == part) { found = r; break; }
      QList<QWidget *> kids = r->findChildren<QWidget *> (part);
      for (QWidget *k : kids) if (k->isVisible ()) { found = k; break; }
      if (found) break;
    }
    if (!found) found = find_by_text (scope, part);
    if (!found) return nullptr;
    scope = { found };
  }
  return found;
}

static QWidget *focus_target ()
{
  if (QWidget *f = QApplication::focusWidget ()) return f;
  if (QWidget *a = QApplication::activeWindow ()) return a;
  for (QWidget *t : QApplication::topLevelWidgets ()) if (t->isVisible () && !qobject_cast<QMenu *> (t)) return t;
  return nullptr;
}

static void send_keys (QWidget *target, const QKeySequence &seq)
{
  for (int i = 0; i < seq.count (); i++) {
    QKeyCombination kc = seq[i];
    QString text;
    int k = kc.key ();
    if (k >= 0x20 && k < 0x7f && !(kc.keyboardModifiers () & (Qt::ControlModifier | Qt::AltModifier))) {
      QChar c (k);
      text = (kc.keyboardModifiers () & Qt::ShiftModifier) ? c.toUpper () : c.toLower ();
    }
    QWidget *t = QApplication::activePopupWidget () ? QApplication::activePopupWidget () : target;
    QKeyEvent press (MQ_QKeyPress, k, kc.keyboardModifiers (), text);
    QKeyEvent release (MQ_QKeyRelease, k, kc.keyboardModifiers (), text);
    QApplication::sendEvent (t, &press);
    QApplication::sendEvent (t, &release);
  }
}

/* a menu bar item by its labels (File/Open...), mnemonic markers ignored;
   each menu on the way is told it is about to show (programs fill menus
   then, Motif's cascading callbacks) */
static QString plain_label (QString t) { return t.remove ('&').trimmed (); }
static QAction *find_menu_action (const QStringList &path)
{
  for (QWidget *top : QApplication::topLevelWidgets ()) {
    if (!top->isVisible ()) continue;
    for (QMenuBar *bar : top->findChildren<QMenuBar *> ()) {
      QList<QAction *> level = bar->actions ();
      QAction *found = nullptr;
      for (const QString &part : path) {
        found = nullptr;
        for (QAction *act : level)
          if (plain_label (act->text ()) == plain_label (part)) { found = act; break; }
        if (!found) break;
        if (found->menu ()) {
          Q_EMIT found->menu ()->aboutToShow ();
          level = found->menu ()->actions ();
        }
      }
      if (found) return found;
    }
  }
  return nullptr;
}

static void run_step (QStringList &steps)
{
  while (!steps.isEmpty ()) {
    QString step = steps.takeFirst ().trimmed ();
    if (step.isEmpty ()) continue;
    QStringList a = step.split (' ', Qt::SkipEmptyParts);
    QString cmd = a.takeFirst ();
    /* input may open a modal loop (a dialog the program runs until it is
     * closed, as NEdit's Open does): the rest of the script is scheduled
     * first, so it goes on inside that loop */
    if (cmd == "key" || cmd == "click" || cmd == "type" || cmd == "drag" || cmd == "menu") {
      QStringList rest = steps;
      steps.clear ();
      QTimer::singleShot (0, qApp, [rest] () mutable { run_step (rest); });
    }
    if (cmd == "wait") {
      QTimer::singleShot (a.value (0).toInt (), qApp, [steps] () mutable { run_step (steps); });
      return;
    } else if (cmd == "key") {
      QWidget *t = a.size () > 1 ? find_widget (a.takeFirst ()) : focus_target ();
      if (!t) { fprintf (stderr, "script: no widget for: %s\n", qPrintable (step)); continue; }
      QKeySequence seq (a.join (' '), QKeySequence::PortableText);
      if (seq.isEmpty ()) seq = QKeySequence (a.join (' '));
      /* a key the application binds as a shortcut goes through Qt's shortcut map */
      bool handled = false;
      {
        /* menu accelerators live on actions that may not be children of the window */
        for (QWidget *top : QApplication::allWidgets ())
          for (QAction *act : top->actions ())
            if (!handled && act->shortcut () == seq && act->isEnabled ()) { act->trigger (); handled = true; }
      }
      if (!handled) send_keys (t, seq);
    } else if (cmd == "click") {
      QWidget *t = find_widget (a.value (0));
      if (!t) { fprintf (stderr, "script: no widget for: %s\n", qPrintable (step)); continue; }
      int b = a.size () > 1 ? a[1].toInt () : 1;
      Qt::MouseButton button = b == 3 ? Qt::RightButton : b == 2 ? Qt::MiddleButton : Qt::LeftButton;
      QPointF pos = a.size () > 3 ? QPointF (a[2].toInt (), a[3].toInt ()) : QPointF (t->width () / 2.0, t->height () / 2.0);
      QPointF global = t->mapToGlobal (pos);
      QMouseEvent press (QEvent::MouseButtonPress, pos, global, button, button, Qt::NoModifier);
      QMouseEvent release (QEvent::MouseButtonRelease, pos, global, button, Qt::NoButton, Qt::NoModifier);
      QApplication::sendEvent (t, &press);
      QApplication::sendEvent (t, &release);
    } else if (cmd == "drag") {
      QWidget *t = find_widget (a.value (0));
      if (!t || a.size () < 5) { fprintf (stderr, "script: bad step: %s\n", qPrintable (step)); continue; }
      QPointF p1 (a[1].toInt (), a[2].toInt ()), p2 (a[3].toInt (), a[4].toInt ());
      QMouseEvent press (QEvent::MouseButtonPress, p1, t->mapToGlobal (p1), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
      QApplication::sendEvent (t, &press);
      for (int i = 1; i <= 8; i++) {
        QPointF p = p1 + (p2 - p1) * i / 8.0;
        QMouseEvent move (QEvent::MouseMove, p, t->mapToGlobal (p), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent (t, &move);
      }
      QMouseEvent release (QEvent::MouseButtonRelease, p2, t->mapToGlobal (p2), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
      QApplication::sendEvent (t, &release);
    } else if (cmd == "type") {
      QWidget *t = find_widget (a.value (0));
      if (!t) { fprintf (stderr, "script: no widget for: %s\n", qPrintable (step)); continue; }
      t->setFocus ();
      QString text = step.section (' ', 2);
      for (QChar c : text) {
        QKeyEvent press (MQ_QKeyPress, c.toUpper ().unicode (), Qt::NoModifier, QString (c));
        QKeyEvent release (MQ_QKeyRelease, c.toUpper ().unicode (), Qt::NoModifier, QString (c));
        QApplication::sendEvent (t, &press);
        QApplication::sendEvent (t, &release);
      }
    } else if (cmd == "preedit" || cmd == "commit") {
      /* what an input method sends: pre-edit text, or committed text */
      QWidget *t = find_widget (a.value (0));
      if (!t) { fprintf (stderr, "script: no widget for: %s\n", qPrintable (step)); continue; }
      t->setFocus ();
      QString text = step.section (' ', 2);
      QInputMethodEvent e (cmd == "preedit" ? text : QString (), {});
      if (cmd == "commit") e.setCommitString (text);
      QApplication::sendEvent (t, &e);
    } else if (cmd == "menu") {
      QString path = step.mid (4).trimmed ();
      QAction *act = find_menu_action (path.split ('/'));
      if (!act || !act->isEnabled ()) fprintf (stderr, "script: no menu item: %s\n", qPrintable (path));
      else act->trigger ();
    } else if (cmd == "shot") {
      mq_save_windows (a.value (0));
    } else if (cmd == "dump") {
      mq_dump_widgets ();
    } else if (cmd == "quit") {
      fflush (stdout);
      ::exit (0);
    } else {
      fprintf (stderr, "script: unknown step: %s\n", qPrintable (step));
    }
    /* let the program react before the next step */
    QTimer::singleShot (50, qApp, [steps] () mutable { run_step (steps); });
    return;
  }
}

void mq_script_start ()
{
  QString script = qEnvironmentVariable ("MOTIFQT_SCRIPT");
  if (script.isEmpty ()) return;
  QStringList steps = script.split (';');
  QTimer::singleShot (0, qApp, [steps] () mutable { run_step (steps); });
}
