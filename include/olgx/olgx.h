/* libolgx-on-Qt: OPEN LOOK graphics primitives drawn with QPainter.
 * During a canvas repaint (CANVAS_REPAINT_PROC) the calls paint into the
 * canvas. Outside a repaint they paint into an offscreen image returned
 * by olgx_image(), which the caller can ignore.
 *
 * States match the classic OLGX button states used by most clients.
 */
#ifndef OLGX_ON_QT_H
#define OLGX_ON_QT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct olgx_info Graphics_info;

#define OLGX_NORMAL     0
#define OLGX_INVOKED    1
#define OLGX_DEFAULT    2
#define OLGX_INACTIVE   4
#define OLGX_HORIZ_MENU_MARK 8

Graphics_info *olgx_main_initialize(void *dpy, int screen);
void           olgx_set_three_d(Graphics_info *g, int three_d);
void           olgx_destroy(Graphics_info *g);

void olgx_draw_box(Graphics_info *g, int x, int y, int w, int h,
                   int state, int fill);
void olgx_draw_button(Graphics_info *g, int x, int y, int w, int h,
                      const char *label, int state);
void olgx_draw_choice(Graphics_info *g, int x, int y, int w, int h,
                      const char *label, int state);
void olgx_draw_text(Graphics_info *g, const char *str, int x, int y,
                    int max_width, int state);
void olgx_draw_scrollbar(Graphics_info *g, int x, int y, int length,
                         int horizontal, int view_start, int view_length,
                         int object_length);
void olgx_draw_menu_mark(Graphics_info *g, int x, int y);

/* Pixel size of the last image olgx painted (offscreen or the canvas). */
int olgx_image_width(void);
int olgx_image_height(void);

#ifdef __cplusplus
}
#endif

#endif
