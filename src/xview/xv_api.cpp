#include "xview/xview.h"

#include <QAction>
#include <QApplication>
#include <QPaintEvent>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

namespace xvqt {
static QPainter *g_paint = nullptr;
static QSize g_canvas;

QPainter *current_painter() { return g_paint; }
QSize current_canvas_size() { return g_canvas; }
}

void mq_ensure_app(int *argc, char **argv);   /* xt_core.cpp */

namespace {

struct Obj;

struct CanvasWidget : QWidget {
    Obj *obj = nullptr;
    explicit CanvasWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAutoFillBackground(true);
        setMinimumSize(80, 60);
    }
    void paintEvent(QPaintEvent *) override;
};

struct Obj {
    Xv_pkg pkg = 0;
    Obj *parent = nullptr;
    std::vector<Obj *> kids;
    QWidget *widget = nullptr;
    QMenu *menu = nullptr;
    std::string label;
    std::string footer;
    Xv_notify_proc notify = nullptr;
    Xv_repaint_proc repaint = nullptr;
    int value = 0;
    int vmin = 0;
    int vmax = 100;
    int show = 1;
    int w = 0;
    int h = 0;
    bool readonly = false;
};

void CanvasWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0xcc, 0xcc, 0xcc));
    xvqt::g_paint = &p;
    xvqt::g_canvas = size();
    if (obj && obj->repaint)
        obj->repaint(reinterpret_cast<Xv_object>(obj));
    xvqt::g_paint = nullptr;
}

static std::vector<Obj *> &all()
{
    static std::vector<Obj *> v;
    return v;
}

static int prog_argc = 0;
static char **prog_argv = nullptr;

/* the application Motif on Qt makes (mq_ensure_app): one QApplication for
   every toolkit in the program, with the headless hooks MOTIFQT_AUTOQUIT,
   MOTIFQT_SCREENSHOT and MOTIFQT_SCRIPT */
static QApplication *app()
{
    if (!QApplication::instance()) {
        mq_ensure_app(prog_argv ? &prog_argc : nullptr, prog_argv);
        if (QApplication::instance())
            qApp->setQuitOnLastWindowClosed(true);   /* closing the frame ends xv_main_loop */
    }
    if (!QApplication::instance()) {   /* no platform: Qt says why */
        static int argc = 1;
        static char name[] = "xview";
        static char *argv[] = {name, nullptr};
        new QApplication(argc, argv);
    }
    return qobject_cast<QApplication *>(QApplication::instance());
}

static void apply_geom(Obj *o)
{
    if (!o->widget)
        return;
    if (o->w > 0)
        o->widget->setMinimumWidth(o->w);
    if (o->h > 0)
        o->widget->setMinimumHeight(o->h);
    if (o->w > 0 && o->h > 0 && (o->pkg == FRAME || o->pkg == CANVAS))
        o->widget->resize(o->w, o->h);
}

static void apply_label(Obj *o)
{
    if (!o->widget)
        return;
    const QString s = QString::fromUtf8(o->label.c_str());
    switch (o->pkg) {
    case FRAME:
        o->widget->setWindowTitle(s);
        break;
    case PANEL_BUTTON:
        if (auto *b = qobject_cast<QPushButton *>(o->widget))
            b->setText(s);
        break;
    case PANEL_MESSAGE:
        if (auto *l = qobject_cast<QLabel *>(o->widget))
            l->setText(s);
        break;
    case PANEL_TEXT:
        break;
    case PANEL_SLIDER:
        if (auto *sl = qobject_cast<QSlider *>(o->widget))
            sl->setToolTip(s);
        break;
    default:
        break;
    }
}

static QWidget *parent_widget(Obj *parent)
{
    if (!parent)
        return nullptr;
    if (parent->pkg == FRAME) {
        if (auto *mw = qobject_cast<QMainWindow *>(parent->widget))
            return mw->centralWidget();
    }
    return parent->widget;
}

static Obj *make(Obj *parent, Xv_pkg pkg)
{
    app();
    auto *o = new Obj;
    o->pkg = pkg;
    o->parent = parent;
    QWidget *pw = parent_widget(parent);
    switch (pkg) {
    case FRAME: {
        auto *mw = new QMainWindow;
        auto *central = new QWidget;
        auto *lay = new QVBoxLayout(central);
        lay->setContentsMargins(6, 6, 6, 6);
        mw->setCentralWidget(central);
        mw->resize(420, 280);
        o->widget = mw;
        break;
    }
    case PANEL: {
        auto *w = new QWidget(pw);
        auto *lay = new QVBoxLayout(w);
        lay->setAlignment(Qt::AlignTop);
        o->widget = w;
        if (pw && pw->layout())
            pw->layout()->addWidget(w);
        break;
    }
    case PANEL_BUTTON: {
        auto *b = new QPushButton(pw);
        QObject::connect(b, &QPushButton::clicked, b, [o] {
            if (o->notify)
                o->notify(reinterpret_cast<Xv_object>(o), nullptr);
        });
        o->widget = b;
        if (pw && pw->layout())
            pw->layout()->addWidget(b);
        break;
    }
    case PANEL_MESSAGE: {
        auto *l = new QLabel(pw);
        o->widget = l;
        if (pw && pw->layout())
            pw->layout()->addWidget(l);
        break;
    }
    case PANEL_TEXT: {
        auto *e = new QLineEdit(pw);
        QObject::connect(e, &QLineEdit::textChanged, e, [o](const QString &t) {
            o->label = t.toUtf8().toStdString();
            if (o->notify)
                o->notify(reinterpret_cast<Xv_object>(o), nullptr);
        });
        o->widget = e;
        if (pw && pw->layout())
            pw->layout()->addWidget(e);
        break;
    }
    case PANEL_SLIDER: {
        auto *s = new QSlider(Qt::Horizontal, pw);
        s->setRange(0, 100);
        QObject::connect(s, &QSlider::valueChanged, s, [o](int v) {
            o->value = v;
            if (o->notify)
                o->notify(reinterpret_cast<Xv_object>(o), nullptr);
        });
        o->widget = s;
        if (pw && pw->layout())
            pw->layout()->addWidget(s);
        break;
    }
    case CANVAS: {
        auto *c = new CanvasWidget(pw);
        c->obj = o;
        o->widget = c;
        if (pw && pw->layout())
            pw->layout()->addWidget(c);
        break;
    }
    case SCROLLBAR: {
        auto *s = new QScrollBar(Qt::Vertical, pw);
        o->widget = s;
        if (pw && pw->layout())
            pw->layout()->addWidget(s);
        break;
    }
    case MENU:
        o->menu = new QMenu;
        break;
    case MENUITEM:
        if (parent && parent->menu) {
            QAction *a = parent->menu->addAction(QString());
            QObject::connect(a, &QAction::triggered, a, [o] {
                if (o->notify)
                    o->notify(reinterpret_cast<Xv_object>(o), nullptr);
            });
            o->widget = nullptr;
            o->menu = reinterpret_cast<QMenu *>(a); /* stash action */
        }
        break;
    default:
        delete o;
        return nullptr;
    }
    if (parent)
        parent->kids.push_back(o);
    all().push_back(o);
    return o;
}

static void set_attr(Obj *o, Xv_attr attr, va_list &ap)
{
    switch (attr) {
    case XV_WIDTH:
        o->w = va_arg(ap, int);
        apply_geom(o);
        break;
    case XV_HEIGHT:
        o->h = va_arg(ap, int);
        apply_geom(o);
        break;
    case XV_X:
        if (o->widget)
            o->widget->move(va_arg(ap, int), o->widget->y());
        else
            va_arg(ap, int);
        break;
    case XV_Y:
        if (o->widget)
            o->widget->move(o->widget->x(), va_arg(ap, int));
        else
            va_arg(ap, int);
        break;
    case XV_SHOW:
        o->show = va_arg(ap, int);
        if (o->widget)
            o->widget->setVisible(o->show);
        break;
    case FRAME_LABEL:
    case PANEL_LABEL_STRING:
    case MENU_STRING: {
        const char *s = va_arg(ap, const char *);
        o->label = s ? s : "";
        apply_label(o);
        if (o->pkg == MENUITEM && o->menu) {
            auto *a = reinterpret_cast<QAction *>(o->menu);
            a->setText(QString::fromUtf8(o->label.c_str()));
        }
        if (o->pkg == PANEL_TEXT)
            if (auto *e = qobject_cast<QLineEdit *>(o->widget))
                e->setText(QString::fromUtf8(o->label.c_str()));
        break;
    }
    case FRAME_SHOW_FOOTER:
        va_arg(ap, int);
        break;
    case FRAME_LEFT_FOOTER: {
        const char *s = va_arg(ap, const char *);
        o->footer = s ? s : "";
        if (auto *mw = qobject_cast<QMainWindow *>(o->widget))
            mw->statusBar()->showMessage(QString::fromUtf8(o->footer.c_str()));
        break;
    }
    case PANEL_NOTIFY_PROC:
    case MENU_NOTIFY_PROC:
        o->notify = va_arg(ap, Xv_notify_proc);
        break;
    case CANVAS_REPAINT_PROC:
        o->repaint = va_arg(ap, Xv_repaint_proc);
        if (o->widget)
            o->widget->update();
        break;
    case PANEL_VALUE:
        o->value = va_arg(ap, int);
        if (auto *s = qobject_cast<QSlider *>(o->widget))
            s->setValue(o->value);
        if (auto *sb = qobject_cast<QScrollBar *>(o->widget))
            sb->setValue(o->value);
        break;
    case PANEL_MIN_VALUE:
        o->vmin = va_arg(ap, int);
        if (auto *s = qobject_cast<QSlider *>(o->widget))
            s->setRange(o->vmin, o->vmax);
        break;
    case PANEL_MAX_VALUE:
        o->vmax = va_arg(ap, int);
        if (auto *s = qobject_cast<QSlider *>(o->widget))
            s->setRange(o->vmin, o->vmax);
        break;
    case PANEL_READ_ONLY:
        o->readonly = va_arg(ap, int) != 0;
        if (auto *e = qobject_cast<QLineEdit *>(o->widget))
            e->setReadOnly(o->readonly);
        break;
    case SCROLLBAR_DIRECTION:
        if (va_arg(ap, int) && o->widget)
            qobject_cast<QScrollBar *>(o->widget)->setOrientation(Qt::Horizontal);
        break;
    case SCROLLBAR_OBJECT_LENGTH:
        if (auto *sb = qobject_cast<QScrollBar *>(o->widget))
            sb->setMaximum(qMax(0, va_arg(ap, int) - 1));
        else
            va_arg(ap, int);
        break;
    case SCROLLBAR_VIEW_LENGTH:
        if (auto *sb = qobject_cast<QScrollBar *>(o->widget))
            sb->setPageStep(va_arg(ap, int));
        else
            va_arg(ap, int);
        break;
    case SCROLLBAR_VIEW_START:
        if (auto *sb = qobject_cast<QScrollBar *>(o->widget))
            sb->setValue(va_arg(ap, int));
        else
            va_arg(ap, int);
        break;
    case CANVAS_WIDTH:
    case CANVAS_HEIGHT:
        /* accepted, stored as width/height */
        if (attr == CANVAS_WIDTH)
            o->w = va_arg(ap, int);
        else
            o->h = va_arg(ap, int);
        apply_geom(o);
        break;
    case PANEL_VALUE_DISPLAY:
    case MENU_GEN_PIN_WINDOW:
        va_arg(ap, int);
        break;
    default:
        /* Unknown attribute: consume one pointer-sized value so the list
         * does not desynchronise. Callers must still pass a real value. */
        va_arg(ap, void *);
        break;
    }
}

static void consume_attrs(Obj *o, va_list &ap)
{
    for (;;) {
        Xv_attr attr = va_arg(ap, Xv_attr);
        if (attr == XV_END)
            break;
        set_attr(o, attr, ap);
    }
}

static Obj *as(Xv_object obj)
{
    return reinterpret_cast<Obj *>(obj);
}

} // namespace

extern "C" Xv_object xv_init(int first, ...)
{
    va_list ap;
    va_start(ap, first);
    if (first == XV_INIT_ARGS) {   /* the program's arguments go to Qt (-platform ...) */
        prog_argc = va_arg(ap, int);
        prog_argv = va_arg(ap, char **);
    }
    va_end(ap);
    app();
    return nullptr;
}

extern "C" Xv_object xv_create(Xv_object parent, Xv_pkg pkg, ...)
{
    Obj *o = make(as(parent), pkg);
    if (!o)
        return nullptr;
    va_list ap;
    va_start(ap, pkg);
    consume_attrs(o, ap);
    va_end(ap);
    return reinterpret_cast<Xv_object>(o);
}

extern "C" Xv_object xv_find(Xv_object parent, Xv_pkg pkg, ...)
{
    Obj *p = as(parent);
    va_list ap;
    va_start(ap, pkg);
    /* Drain attributes; find currently only matches package under parent. */
    if (p) {
        Obj tmp = *p;
        tmp.widget = nullptr;
        tmp.menu = nullptr;
        tmp.kids.clear();
        /* do not apply to a copy that owns widgets: consume only */
    }
    /* Re-walk by consuming on a dummy object without a widget. */
    Obj dummy;
    dummy.pkg = pkg;
    consume_attrs(&dummy, ap);
    va_end(ap);
    if (p) {
        for (Obj *k : p->kids)
            if (k->pkg == pkg)
                return reinterpret_cast<Xv_object>(k);
    }
    for (Obj *k : all())
        if (k->pkg == pkg && k->parent == p)
            return reinterpret_cast<Xv_object>(k);
    return nullptr;
}

extern "C" int xv_set(Xv_object obj, ...)
{
    Obj *o = as(obj);
    if (!o)
        return -1;
    va_list ap;
    va_start(ap, obj);
    consume_attrs(o, ap);
    va_end(ap);
    return 0;
}

extern "C" int xv_get(Xv_object obj, Xv_attr attr)
{
    Obj *o = as(obj);
    if (!o)
        return 0;
    switch (attr) {
    case XV_WIDTH:
        return o->widget ? o->widget->width() : o->w;
    case XV_HEIGHT:
        return o->widget ? o->widget->height() : o->h;
    case XV_SHOW:
        return o->widget ? o->widget->isVisible() : o->show;
    case PANEL_VALUE:
        if (auto *s = qobject_cast<QSlider *>(o->widget))
            return s->value();
        if (auto *sb = qobject_cast<QScrollBar *>(o->widget))
            return sb->value();
        return o->value;
    case PANEL_MIN_VALUE:
        return o->vmin;
    case PANEL_MAX_VALUE:
        return o->vmax;
    default:
        return 0;
    }
}

extern "C" const char *xv_get_str(Xv_object obj, Xv_attr attr)
{
    Obj *o = as(obj);
    if (!o)
        return "";
    if (attr == FRAME_LEFT_FOOTER)
        return o->footer.c_str();
    if (attr == PANEL_LABEL_STRING || attr == FRAME_LABEL || attr == MENU_STRING) {
        if (o->pkg == PANEL_TEXT)
            if (auto *e = qobject_cast<QLineEdit *>(o->widget)) {
                o->label = e->text().toUtf8().toStdString();
            }
        return o->label.c_str();
    }
    return "";
}

extern "C" void xv_set_int(Xv_object obj, Xv_attr attr, int value)
{
    xv_set(obj, attr, value, XV_END);
}

extern "C" void xv_set_str(Xv_object obj, Xv_attr attr, const char *value)
{
    xv_set(obj, attr, value, XV_END);
}

static void destroy_one(Obj *o)
{
    if (!o)
        return;
    std::vector<Obj *> kids = o->kids;
    for (Obj *k : kids)
        destroy_one(k);
    o->kids.clear();
    if (o->parent) {
        auto &pk = o->parent->kids;
        for (auto it = pk.begin(); it != pk.end(); ++it)
            if (*it == o) {
                pk.erase(it);
                break;
            }
    }
    auto &v = all();
    for (auto it = v.begin(); it != v.end(); ++it)
        if (*it == o) {
            v.erase(it);
            break;
        }
    if (o->pkg == MENU && o->menu) {
        delete o->menu; /* also deletes its actions */
        o->menu = nullptr;
    }
    if (o->pkg == MENUITEM)
        o->menu = nullptr; /* action is owned by the QMenu */
    if (o->widget && (!o->parent || o->pkg == FRAME))
        o->widget->deleteLater();
    else if (o->widget)
        o->widget->deleteLater();
    o->widget = nullptr;
    delete o;
}

extern "C" void xv_destroy(Xv_object obj)
{
    destroy_one(as(obj));
}

extern "C" void notify_stop(void)
{
    if (QApplication *a = app())
        a->quit();
}

extern "C" void notify_start(void)
{
    app()->exec();
}

extern "C" void xv_main_loop(Xv_object frame)
{
    Obj *o = as(frame);
    if (o && o->widget)
        o->widget->show();
    notify_start();
}

extern "C" int xv_notice(Xv_object owner, const char *message, const char *button)
{
    app();
    QWidget *w = nullptr;
    if (Obj *o = as(owner))
        w = o->widget;
    QMessageBox box(w);
    box.setWindowTitle(QStringLiteral("Notice"));
    box.setText(QString::fromUtf8(message ? message : ""));
    box.addButton(QString::fromUtf8(button ? button : "OK"), QMessageBox::AcceptRole);
    box.exec();
    return 1;
}
