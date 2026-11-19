/* XView-on-Qt: a source-level subset of the common XView calls.
 * Not binary-compatible with Sun XView. Attribute values and package
 * ids are ours. The shape of a small program matches classic XView:
 *
 *   xv_init(XV_INIT_ARGS, argc, argv, NULL);
 *   Frame f = xv_create(NULL, FRAME, FRAME_LABEL, "Hi", XV_WIDTH, 400, NULL);
 *   Panel p = xv_create(f, PANEL, NULL);
 *   xv_create(p, PANEL_BUTTON, PANEL_LABEL_STRING, "Quit",
 *             PANEL_NOTIFY_PROC, quit, NULL);
 *   xv_main_loop(f);
 */
#ifndef XVIEW_ON_QT_H
#define XVIEW_ON_QT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xv_object *Xv_object;
typedef Xv_object Frame;
typedef Xv_object Panel;
typedef Xv_object Panel_item;
typedef Xv_object Canvas;
typedef Xv_object Menu;
typedef Xv_object Scrollbar;

typedef int Xv_pkg;
typedef int Xv_attr;

/* Packages */
#define FRAME          1
#define PANEL          2
#define PANEL_BUTTON   3
#define PANEL_MESSAGE  4
#define PANEL_TEXT     5
#define PANEL_SLIDER   6
#define CANVAS         7
#define MENU           8
#define MENUITEM       9
#define SCROLLBAR     10

/* Generic attributes. A list ends with NULL (0). */
#define XV_END              0
#define XV_WIDTH            1   /* int */
#define XV_HEIGHT           2   /* int */
#define XV_X                3   /* int */
#define XV_Y                4   /* int */
#define XV_SHOW             5   /* int (bool) */
#define FRAME_LABEL         10  /* const char * */
#define FRAME_SHOW_FOOTER   11  /* int */
#define FRAME_LEFT_FOOTER   12  /* const char * */
#define PANEL_LABEL_STRING  20  /* const char * */
#define PANEL_NOTIFY_PROC   21  /* Xv_notify_proc */
#define PANEL_VALUE         22  /* int */
#define PANEL_MIN_VALUE     23  /* int */
#define PANEL_MAX_VALUE     24  /* int */
#define PANEL_VALUE_DISPLAY 25  /* int bool, slider */
#define PANEL_READ_ONLY     26  /* int bool, text */
#define CANVAS_REPAINT_PROC 30  /* Xv_repaint_proc */
#define CANVAS_WIDTH        31  /* int paint width */
#define CANVAS_HEIGHT       32  /* int paint height */
#define MENU_STRING         40  /* const char * */
#define MENU_NOTIFY_PROC    41  /* Xv_notify_proc */
#define MENU_GEN_PIN_WINDOW 42  /* unused, accepted */
#define SCROLLBAR_DIRECTION 50  /* int: 0 vertical, 1 horizontal */
#define SCROLLBAR_VIEW_START 51 /* int */
#define SCROLLBAR_OBJECT_LENGTH 52 /* int */
#define SCROLLBAR_VIEW_LENGTH 53 /* int */
#define XV_INIT_ARGS        60

typedef int (*Xv_notify_proc)(Xv_object item, void *event);
typedef void (*Xv_repaint_proc)(Xv_object canvas);

/* xv_init(XV_INIT_ARGS, argc, argv, NULL) or xv_init(NULL). */
Xv_object xv_init(int first, ...);

Xv_object xv_create(Xv_object parent, Xv_pkg pkg, ...);
Xv_object xv_find(Xv_object parent, Xv_pkg pkg, ...);
int       xv_set(Xv_object obj, ...);
int       xv_get(Xv_object obj, Xv_attr attr);
void      xv_destroy(Xv_object obj);

void      xv_main_loop(Xv_object frame);
void      notify_start(void);
void      notify_stop(void);

/* Convenience used by the demo and by tests. */
const char *xv_get_str(Xv_object obj, Xv_attr attr);
void        xv_set_int(Xv_object obj, Xv_attr attr, int value);
void        xv_set_str(Xv_object obj, Xv_attr attr, const char *value);

/* Modal notice. Returns 1 if the user pressed the button, 0 otherwise. */
int xv_notice(Xv_object owner, const char *message, const char *button);

#ifdef __cplusplus
}
#endif

#endif
