#include "forms.h"

#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QWidget>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

void mq_ensure_app(int *argc, char **argv);   /* xt_core.cpp */

namespace {

struct Form {
    int type = FL_UP_BOX;
    int w = 0;
    int h = 0;
    QWidget *win = nullptr;
    std::vector<FL_OBJECT *> objs;
    std::string title;
};

struct Impl {
    QWidget *widget = nullptr;
    std::string input;
};

static Form *current_form = nullptr;
static FL_OBJECT *returned = nullptr;
static int timeout_seq = 1;

static int *prog_argc = nullptr;
static char **prog_argv = nullptr;

/* the application Motif on Qt makes (mq_ensure_app): one QApplication for
   every toolkit in the program, with the headless hooks MOTIFQT_AUTOQUIT,
   MOTIFQT_SCREENSHOT and MOTIFQT_SCRIPT */
static QApplication *app()
{
    if (!QApplication::instance()) {
        mq_ensure_app(prog_argc, prog_argv);
        if (QApplication::instance())
            qApp->setQuitOnLastWindowClosed(true);   /* closing the form ends fl_do_forms */
    }
    if (!QApplication::instance()) {   /* no platform: Qt says why */
        static int argc = 1;
        static char name[] = "xforms";
        static char *argv[] = {name, nullptr};
        new QApplication(argc, argv);
    }
    return qobject_cast<QApplication *>(QApplication::instance());
}

static Impl *impl(FL_OBJECT *o)
{
    return o ? static_cast<Impl *>(o->spec) : nullptr;
}

static void place(FL_OBJECT *o)
{
    if (impl(o) && impl(o)->widget)
        impl(o)->widget->setGeometry(o->x, o->y, o->w, o->h);
}

static void fire(FL_OBJECT *o)
{
    if (!o) return;
    if (o->object_callback) o->object_callback(o, o->argument);
    else {
        returned = o;
        if (QApplication *a = app()) a->quit();
    }
}

static FL_OBJECT *add(int klass, int type, int x, int y, int w, int h, const char *label, QWidget *widget)
{
    app();
    if (!current_form) return nullptr;
    auto *o = new FL_OBJECT();
    std::memset(o, 0, sizeof *o);
    o->form = reinterpret_cast<FL_FORM *>(current_form);
    o->objclass = klass;
    o->type = type;
    o->boxtype = type;
    o->x = x; o->y = y; o->w = w; o->h = h;
    o->fl1 = 0; o->fl2 = 1; o->fl3 = 0;
    o->active = 1;
    o->visible = 1;
    auto *im = new Impl;
    im->widget = widget;
    o->spec = im;
    if (label) {
        o->label = strdup(label);
    }
    if (widget) {
        widget->setGeometry(x, y, w, h);
        if (o->label && klass != FL_INPUT) {
            if (auto *b = qobject_cast<QPushButton *>(widget)) b->setText(QString::fromUtf8(o->label));
            else if (auto *l = qobject_cast<QLabel *>(widget)) l->setText(QString::fromUtf8(o->label));
            else if (auto *g = qobject_cast<QGroupBox *>(widget)) g->setTitle(QString::fromUtf8(o->label));
        }
        if (auto *b = qobject_cast<QPushButton *>(widget))
            QObject::connect(b, &QPushButton::clicked, b, [o] { if (o->active) fire(o); });
        if (auto *e = qobject_cast<QLineEdit *>(widget))
            QObject::connect(e, &QLineEdit::returnPressed, e, [o] { if (o->active) fire(o); });
        if (auto *s = qobject_cast<QSlider *>(widget))
            QObject::connect(s, &QSlider::valueChanged, s, [o](int) { if (o->active) fire(o); });
        if (auto *list = qobject_cast<QListWidget *>(widget))
            QObject::connect(list, &QListWidget::currentRowChanged, list, [o](int) { if (o->active) fire(o); });
        if (auto *box = qobject_cast<QComboBox *>(widget))
            QObject::connect(box, qOverload<int>(&QComboBox::currentIndexChanged), box, [o](int) { if (o->active) fire(o); });
    }
    current_form->objs.push_back(o);
    return o;
}

static int slider_to_qt(FL_OBJECT *o, double v)
{
    double a = o->fl1, b = o->fl2;
    if (b == a) return 0;
    double t = (v - a) / (b - a);
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return (int)(t * 1000);
}

static double slider_from_qt(FL_OBJECT *o, int v)
{
    return o->fl1 + (o->fl2 - o->fl1) * (v / 1000.0);
}

} // namespace

extern "C" Display *fl_initialize(int *argc, char **argv, const char *, FL_CMD_OPT *, int)
{
    prog_argc = argc;
    prog_argv = argv;
    app();
    return nullptr;
}

extern "C" void fl_finish(void)
{
    if (QApplication *a = QApplication::instance() ? app() : nullptr) a->quit();
}

extern "C" FL_FORM *fl_bgn_form(int type, FL_Coord w, FL_Coord h)
{
    app();
    auto *f = new Form;
    f->type = type;
    f->w = w;
    f->h = h;
    f->win = new QWidget;
    f->win->resize(w, h);
    current_form = f;
    return reinterpret_cast<FL_FORM *>(f);
}

extern "C" void fl_end_form(void) { current_form = nullptr; }

extern "C" FL_FORM *fl_addto_form(FL_FORM *form)
{
    current_form = reinterpret_cast<Form *>(form);
    return form;
}

extern "C" FL_OBJECT *fl_add_box(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    auto *fr = new QFrame(current_form ? current_form->win : nullptr);
    fr->setFrameShape(type == FL_FLAT_BOX ? QFrame::NoFrame : QFrame::Box);
    fr->setFrameShadow(type == FL_DOWN_BOX ? QFrame::Sunken : QFrame::Raised);
    return add(FL_BOX, type, x, y, w, h, label, fr);
}

extern "C" FL_OBJECT *fl_add_frame(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    return fl_add_box(type == 0 ? FL_FRAME_BOX : type, x, y, w, h, label);
}

extern "C" FL_OBJECT *fl_add_labelframe(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    (void)type;
    auto *g = new QGroupBox(current_form ? current_form->win : nullptr);
    return add(FL_LABELFRAME, FL_FRAME_BOX, x, y, w, h, label, g);
}

extern "C" FL_OBJECT *fl_add_button(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    auto *b = new QPushButton(current_form ? current_form->win : nullptr);
    if (type == FL_LIGHTBUTTON || type == FL_PUSH_BUTTON) b->setCheckable(true);
    return add(FL_BUTTON, type, x, y, w, h, label, b);
}

extern "C" FL_OBJECT *fl_add_lightbutton(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    return fl_add_button(type == 0 ? FL_LIGHTBUTTON : type, x, y, w, h, label);
}

extern "C" FL_OBJECT *fl_add_text(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    (void)type;
    auto *l = new QLabel(current_form ? current_form->win : nullptr);
    return add(FL_TEXT, FL_NORMAL_TEXT, x, y, w, h, label, l);
}

extern "C" FL_OBJECT *fl_add_input(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    QWidget *wdt;
    if (type == FL_MULTILINE_INPUT)
        wdt = new QPlainTextEdit(current_form ? current_form->win : nullptr);
    else
        wdt = new QLineEdit(current_form ? current_form->win : nullptr);
    FL_OBJECT *o = add(FL_INPUT, type, x, y, w, h, label, wdt);
    if (o && label && type != FL_MULTILINE_INPUT)
        if (auto *e = qobject_cast<QLineEdit *>(wdt)) e->setPlaceholderText(QString::fromUtf8(label));
    return o;
}

extern "C" FL_OBJECT *fl_add_slider(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    auto *s = new QSlider(type == FL_VERT_SLIDER ? Qt::Vertical : Qt::Horizontal,
                          current_form ? current_form->win : nullptr);
    s->setRange(0, 1000);
    FL_OBJECT *o = add(FL_SLIDER, type, x, y, w, h, label, s);
    if (o) o->val = 0;
    return o;
}

extern "C" FL_OBJECT *fl_add_browser(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    (void)type;
    auto *list = new QListWidget(current_form ? current_form->win : nullptr);
    return add(FL_BROWSER, FL_NORMAL_BROWSER, x, y, w, h, label, list);
}

extern "C" FL_OBJECT *fl_add_choice(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label)
{
    (void)type;
    auto *box = new QComboBox(current_form ? current_form->win : nullptr);
    return add(FL_CHOICE, FL_NORMAL_CHOICE, x, y, w, h, label, box);
}

extern "C" void fl_set_object_label(FL_OBJECT *obj, const char *label)
{
    if (!obj) return;
    free(obj->label);
    obj->label = label ? strdup(label) : nullptr;
    QWidget *w = impl(obj) ? impl(obj)->widget : nullptr;
    if (!w) return;
    const QString s = QString::fromUtf8(obj->label ? obj->label : "");
    if (auto *b = qobject_cast<QPushButton *>(w)) b->setText(s);
    else if (auto *l = qobject_cast<QLabel *>(w)) l->setText(s);
    else if (auto *g = qobject_cast<QGroupBox *>(w)) g->setTitle(s);
}

extern "C" void fl_set_object_callback(FL_OBJECT *obj, FL_CALLBACKPTR cb, long arg)
{
    if (!obj) return;
    obj->object_callback = cb;
    obj->argument = arg;
}

extern "C" void fl_deactivate_object(FL_OBJECT *obj)
{
    if (!obj) return;
    obj->active = 0;
    if (impl(obj) && impl(obj)->widget) impl(obj)->widget->setEnabled(false);
}

extern "C" void fl_activate_object(FL_OBJECT *obj)
{
    if (!obj) return;
    obj->active = 1;
    if (impl(obj) && impl(obj)->widget) impl(obj)->widget->setEnabled(true);
}

extern "C" void fl_hide_object(FL_OBJECT *obj)
{
    if (!obj) return;
    obj->visible = 0;
    if (impl(obj) && impl(obj)->widget) impl(obj)->widget->hide();
}

extern "C" void fl_show_object(FL_OBJECT *obj)
{
    if (!obj) return;
    obj->visible = 1;
    if (impl(obj) && impl(obj)->widget) impl(obj)->widget->show();
}

extern "C" void fl_redraw_object(FL_OBJECT *obj)
{
    if (obj) place(obj);
    if (impl(obj) && impl(obj)->widget) impl(obj)->widget->update();
}

extern "C" void fl_free_object(FL_OBJECT *obj)
{
    if (!obj) return;
    if (impl(obj) && impl(obj)->widget) impl(obj)->widget->deleteLater();
    delete impl(obj);
    free(obj->label);
    if (Form *f = reinterpret_cast<Form *>(obj->form)) {
        auto &v = f->objs;
        for (auto it = v.begin(); it != v.end(); ++it)
            if (*it == obj) { v.erase(it); break; }
    }
    delete obj;
}

extern "C" void fl_set_button(FL_OBJECT *obj, int pushed)
{
    if (!impl(obj)) return;
    if (auto *b = qobject_cast<QPushButton *>(impl(obj)->widget)) {
        QSignalBlocker block(b);
        b->setChecked(pushed != 0);
    }
}

extern "C" int fl_get_button(FL_OBJECT *obj)
{
    if (!impl(obj)) return 0;
    if (auto *b = qobject_cast<QPushButton *>(impl(obj)->widget)) return b->isChecked() ? 1 : 0;
    return 0;
}

extern "C" void fl_set_input(FL_OBJECT *obj, const char *str)
{
    if (!impl(obj)) return;
    impl(obj)->input = str ? str : "";
    QWidget *w = impl(obj)->widget;
    if (auto *e = qobject_cast<QLineEdit *>(w)) {
        QSignalBlocker block(e);
        e->setText(QString::fromUtf8(impl(obj)->input.c_str()));
    } else if (auto *t = qobject_cast<QPlainTextEdit *>(w)) {
        QSignalBlocker block(t);
        t->setPlainText(QString::fromUtf8(impl(obj)->input.c_str()));
    }
}

extern "C" const char *fl_get_input(FL_OBJECT *obj)
{
    if (!impl(obj)) return "";
    QWidget *w = impl(obj)->widget;
    if (auto *e = qobject_cast<QLineEdit *>(w)) impl(obj)->input = e->text().toUtf8().toStdString();
    else if (auto *t = qobject_cast<QPlainTextEdit *>(w)) impl(obj)->input = t->toPlainText().toUtf8().toStdString();
    return impl(obj)->input.c_str();
}

extern "C" void fl_set_slider_bounds(FL_OBJECT *obj, double min, double max)
{
    if (!obj) return;
    obj->fl1 = min;
    obj->fl2 = max;
    if (impl(obj)) {
        if (auto *s = qobject_cast<QSlider *>(impl(obj)->widget)) {
            QSignalBlocker block(s);
            s->setValue(slider_to_qt(obj, obj->val));
        }
    }
}

extern "C" void fl_set_slider_value(FL_OBJECT *obj, double val)
{
    if (!obj) return;
    obj->val = val;
    if (impl(obj)) {
        if (auto *s = qobject_cast<QSlider *>(impl(obj)->widget)) {
            QSignalBlocker block(s);
            s->setValue(slider_to_qt(obj, val));
        }
    }
}

extern "C" double fl_get_slider_value(FL_OBJECT *obj)
{
    if (!obj || !impl(obj)) return 0;
    if (auto *s = qobject_cast<QSlider *>(impl(obj)->widget))
        obj->val = slider_from_qt(obj, s->value());
    return obj->val;
}

extern "C" void fl_clear_browser(FL_OBJECT *obj)
{
    if (!impl(obj)) return;
    if (auto *list = qobject_cast<QListWidget *>(impl(obj)->widget)) list->clear();
}

extern "C" void fl_add_browser_line(FL_OBJECT *obj, const char *text)
{
    if (!impl(obj)) return;
    if (auto *list = qobject_cast<QListWidget *>(impl(obj)->widget)) {
        QSignalBlocker block(list);
        list->addItem(QString::fromUtf8(text ? text : ""));
    }
}

extern "C" int fl_get_browser(FL_OBJECT *obj)
{
    if (!impl(obj)) return 0;
    if (auto *list = qobject_cast<QListWidget *>(impl(obj)->widget))
        return list->currentRow() + 1;
    return 0;
}

extern "C" void fl_clear_choice(FL_OBJECT *obj)
{
    if (!impl(obj)) return;
    if (auto *box = qobject_cast<QComboBox *>(impl(obj)->widget)) box->clear();
}

extern "C" void fl_addto_choice(FL_OBJECT *obj, const char *text)
{
    if (!impl(obj)) return;
    if (auto *box = qobject_cast<QComboBox *>(impl(obj)->widget)) {
        QSignalBlocker block(box);
        box->addItem(QString::fromUtf8(text ? text : ""));
    }
}

extern "C" int fl_get_choice(FL_OBJECT *obj)
{
    if (!impl(obj)) return 0;
    if (auto *box = qobject_cast<QComboBox *>(impl(obj)->widget))
        return box->currentIndex() + 1;
    return 0;
}

extern "C" void fl_set_choice(FL_OBJECT *obj, int line)
{
    if (!impl(obj)) return;
    if (auto *box = qobject_cast<QComboBox *>(impl(obj)->widget)) {
        QSignalBlocker block(box);
        box->setCurrentIndex(line - 1);
    }
}

extern "C" void fl_show_form(FL_FORM *form, int, int, const char *name)
{
    Form *f = reinterpret_cast<Form *>(form);
    if (!f || !f->win) return;
    if (name) {
        f->title = name;
        f->win->setWindowTitle(QString::fromUtf8(name));
    }
    f->win->resize(f->w, f->h);
    f->win->show();
}

extern "C" void fl_hide_form(FL_FORM *form)
{
    Form *f = reinterpret_cast<Form *>(form);
    if (f && f->win) f->win->hide();
}

extern "C" void fl_set_form_title(FL_FORM *form, const char *title)
{
    Form *f = reinterpret_cast<Form *>(form);
    if (!f || !f->win) return;
    f->title = title ? title : "";
    f->win->setWindowTitle(QString::fromUtf8(f->title.c_str()));
}

extern "C" void fl_free_form(FL_FORM *form)
{
    Form *f = reinterpret_cast<Form *>(form);
    if (!f) return;
    if (current_form == f) current_form = nullptr;
    std::vector<FL_OBJECT *> copy = f->objs;
    for (FL_OBJECT *o : copy) fl_free_object(o);
    if (f->win) f->win->deleteLater();
    delete f;
}

extern "C" FL_OBJECT *fl_do_forms(void)
{
    returned = nullptr;
    app()->exec();
    return returned;
}

extern "C" FL_OBJECT *fl_check_forms(void)
{
    returned = nullptr;
    app()->processEvents();
    return returned;
}

extern "C" int fl_add_timeout(long msec, FL_TIMEOUT_CALLBACK cb, void *data)
{
    int id = timeout_seq++;
    QTimer::singleShot((int)msec, app(), [id, cb, data] { if (cb) cb(id, data); });
    return id;
}
