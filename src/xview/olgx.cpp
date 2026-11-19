#include "olgx/olgx.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QtGlobal>

#include <memory>

/* Shared with xv_api.cpp: the painter of the canvas currently repainting,
 * or nullptr when olgx should use its own image. */
namespace xvqt {
QPainter *current_painter();
QSize current_canvas_size();
}

struct olgx_info {
    int three_d = 1;
    QColor bg = QColor(0xcc, 0xcc, 0xcc);
    QColor fg = QColor(0x11, 0x11, 0x11);
    QColor light = QColor(0xff, 0xff, 0xff);
    QColor dark = QColor(0x55, 0x55, 0x55);
    QColor fill_inv = QColor(0x88, 0x88, 0x88);
};

static QImage g_image(64, 64, QImage::Format_ARGB32_Premultiplied);
static std::unique_ptr<QPainter> g_owned;

static QPainter *painter()
{
    if (QPainter *p = xvqt::current_painter())
        return p;
    if (!g_owned) {
        g_image.fill(qRgb(0xcc, 0xcc, 0xcc));
        g_owned = std::make_unique<QPainter>(&g_image);
        g_owned->setFont(QFont(QStringLiteral("Sans Serif"), 10));
    }
    return g_owned.get();
}

static void ensure_image(int w, int h)
{
    if (xvqt::current_painter())
        return;
    w = qMax(w, 1);
    h = qMax(h, 1);
    if (g_image.width() < w || g_image.height() < h) {
        if (g_owned)
            g_owned->end();
        g_owned.reset();
        QImage n(qMax(w, g_image.width()), qMax(h, g_image.height()),
                 QImage::Format_ARGB32_Premultiplied);
        n.fill(qRgb(0xcc, 0xcc, 0xcc));
        g_image = n;
    }
}

extern "C" Graphics_info *olgx_main_initialize(void *, int)
{
    return new Graphics_info;
}

extern "C" void olgx_set_three_d(Graphics_info *g, int three_d)
{
    if (g)
        g->three_d = three_d ? 1 : 0;
}

extern "C" void olgx_destroy(Graphics_info *g)
{
    delete g;
}

static void draw_3d_frame(QPainter *p, const Graphics_info *g,
                          int x, int y, int w, int h, int invoked)
{
    if (w < 2 || h < 2)
        return;
    const QColor hi = invoked ? g->dark : g->light;
    const QColor lo = invoked ? g->light : g->dark;
    p->setPen(QPen(hi, 1));
    p->drawLine(x, y, x + w - 1, y);
    p->drawLine(x, y, x, y + h - 1);
    p->setPen(QPen(lo, 1));
    p->drawLine(x, y + h - 1, x + w - 1, y + h - 1);
    p->drawLine(x + w - 1, y, x + w - 1, y + h - 1);
    if (g->three_d && w > 4 && h > 4) {
        p->setPen(QPen(invoked ? g->light : g->dark, 1));
        p->drawLine(x + 1, y + h - 2, x + w - 2, y + h - 2);
        p->drawLine(x + w - 2, y + 1, x + w - 2, y + h - 2);
    }
}

extern "C" void olgx_draw_box(Graphics_info *g, int x, int y, int w, int h,
                              int state, int fill)
{
    if (!g)
        return;
    ensure_image(x + w, y + h);
    QPainter *p = painter();
    const bool invoked = (state & OLGX_INVOKED) != 0;
    const bool inactive = (state & OLGX_INACTIVE) != 0;
    if (fill) {
        p->fillRect(x + 1, y + 1, qMax(0, w - 2), qMax(0, h - 2),
                    invoked ? g->fill_inv : g->bg);
    }
    draw_3d_frame(p, g, x, y, w, h, invoked);
    if (inactive) {
        p->setPen(QPen(g->dark, 1, Qt::DotLine));
        p->drawRect(x + 2, y + 2, qMax(0, w - 5), qMax(0, h - 5));
    }
}

extern "C" void olgx_draw_button(Graphics_info *g, int x, int y, int w, int h,
                                 const char *label, int state)
{
    if (!g)
        return;
    const bool def = (state & OLGX_DEFAULT) != 0;
    if (def && w > 6 && h > 6) {
        ensure_image(x + w, y + h);
        QPainter *p = painter();
        p->setPen(QPen(g->fg, 1));
        p->drawRect(x, y, w - 1, h - 1);
        olgx_draw_box(g, x + 2, y + 2, w - 4, h - 4, state, 1);
        x += 2;
        y += 2;
        w -= 4;
        h -= 4;
    } else {
        olgx_draw_box(g, x, y, w, h, state, 1);
    }
    if (!label)
        return;
    QPainter *p = painter();
    p->setPen((state & OLGX_INACTIVE) ? g->dark : g->fg);
    QRect r(x, y, w, h);
    p->drawText(r, Qt::AlignCenter, QString::fromUtf8(label));
}

extern "C" void olgx_draw_choice(Graphics_info *g, int x, int y, int w, int h,
                                 const char *label, int state)
{
    olgx_draw_button(g, x, y, qMax(0, w - 16), h, label, state);
    olgx_draw_menu_mark(g, x + qMax(0, w - 14), y + h / 2);
}

extern "C" void olgx_draw_text(Graphics_info *g, const char *str, int x, int y,
                               int max_width, int state)
{
    if (!g || !str)
        return;
    ensure_image(x + qMax(max_width, 8), y + 16);
    QPainter *p = painter();
    p->setPen((state & OLGX_INACTIVE) ? g->dark : g->fg);
    QRect r(x, y - 12, max_width > 0 ? max_width : 400, 16);
    p->drawText(r, Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8(str));
}

extern "C" void olgx_draw_scrollbar(Graphics_info *g, int x, int y, int length,
                                    int horizontal, int view_start,
                                    int view_length, int object_length)
{
    if (!g || length < 4)
        return;
    const int thick = 14;
    const int w = horizontal ? length : thick;
    const int h = horizontal ? thick : length;
    olgx_draw_box(g, x, y, w, h, OLGX_NORMAL, 1);
    if (object_length <= 0)
        object_length = 1;
    view_length = qBound(1, view_length, object_length);
    view_start = qBound(0, view_start, object_length - view_length);
    const int span = (horizontal ? w : h) - 4;
    const int thumb = qMax(8, span * view_length / object_length);
    const int pos = (span - thumb) * view_start / qMax(1, object_length - view_length);
    QPainter *p = painter();
    p->setPen(Qt::NoPen);
    p->setBrush(g->light);
    if (horizontal)
        p->drawRect(x + 2 + pos, y + 2, thumb, h - 4);
    else
        p->drawRect(x + 2, y + 2 + pos, w - 4, thumb);
    draw_3d_frame(p, g, horizontal ? x + 2 + pos : x + 2,
                  horizontal ? y + 2 : y + 2 + pos,
                  horizontal ? thumb : w - 4,
                  horizontal ? h - 4 : thumb, 0);
}

extern "C" void olgx_draw_menu_mark(Graphics_info *g, int x, int y)
{
    if (!g)
        return;
    ensure_image(x + 10, y + 6);
    QPainter *p = painter();
    p->setPen(QPen(g->fg, 1));
    p->drawLine(x, y - 3, x + 6, y);
    p->drawLine(x + 6, y, x, y + 3);
}

extern "C" int olgx_image_width(void)
{
    if (xvqt::current_painter())
        return xvqt::current_canvas_size().width();
    return g_image.width();
}

extern "C" int olgx_image_height(void)
{
    if (xvqt::current_painter())
        return xvqt::current_canvas_size().height();
    return g_image.height();
}
