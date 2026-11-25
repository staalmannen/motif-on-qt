/* XForms-on-Qt: a source-level subset of the Forms Library API.
 * Not binary-compatible with XForms 1.2. The calls a small program uses
 * match classic XForms:
 *
 *   fl_initialize(&argc, argv, "Demo", NULL, 0);
 *   FL_FORM *f = fl_bgn_form(FL_UP_BOX, 320, 240);
 *   FL_OBJECT *b = fl_add_button(FL_NORMAL_BUTTON, 20, 180, 80, 30, "Quit");
 *   fl_set_object_callback(b, quit_cb, 0);
 *   fl_end_form();
 *   fl_show_form(f, FL_PLACE_CENTER, FL_FULLBORDER, "Demo");
 *   fl_do_forms();
 */
#ifndef XFORMS_ON_QT_H
#define XFORMS_ON_QT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int FL_Coord;

typedef struct forms_ FL_FORM;
typedef struct flobjs_ FL_OBJECT;

typedef void (*FL_CALLBACKPTR)(FL_OBJECT *obj, long argument);
typedef void (*FL_TIMEOUT_CALLBACK)(int id, void *data);

struct flobjs_ {
    FL_FORM *form;
    void *u_vdata;
    char *u_cdata;
    long u_ldata;
    int objclass;
    int type;
    int boxtype;
    FL_Coord x, y, w, h;
    double fl1, fl2, fl3;   /* slider bounds and step */
    double val;
    char *label;
    FL_CALLBACKPTR object_callback;
    long argument;
    int active;
    int visible;
    void *spec;             /* private; do not use */
};

#define FL_PLACE_CENTER   1
#define FL_FULLBORDER     1

#define FL_UP_BOX         1
#define FL_DOWN_BOX       2
#define FL_FLAT_BOX       3
#define FL_FRAME_BOX      4
#define FL_SHADOW_BOX     5

#define FL_BOX            1
#define FL_FRAME          2
#define FL_LABELFRAME     3
#define FL_BUTTON         4
#define FL_TEXT           5
#define FL_INPUT          6
#define FL_SLIDER         7
#define FL_BROWSER        8
#define FL_CHOICE         9

#define FL_NORMAL_BUTTON  0
#define FL_PUSH_BUTTON    1
#define FL_TOUCH_BUTTON   2
#define FL_LIGHTBUTTON    3

#define FL_NORMAL_INPUT   0
#define FL_MULTILINE_INPUT 1
#define FL_HOR_SLIDER     0
#define FL_VERT_SLIDER    1
#define FL_NORMAL_BROWSER 0
#define FL_NORMAL_CHOICE  0
#define FL_NORMAL_TEXT    0

typedef struct { int dummy; } FL_CMD_OPT;
typedef struct { int dummy; } Display;

Display *fl_initialize(int *argc, char *argv[], const char *appclass,
                       FL_CMD_OPT *opt, int nopt);
void fl_finish(void);

FL_FORM *fl_bgn_form(int type, FL_Coord w, FL_Coord h);
void fl_end_form(void);
FL_FORM *fl_addto_form(FL_FORM *form);

FL_OBJECT *fl_add_box(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);
FL_OBJECT *fl_add_frame(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);
FL_OBJECT *fl_add_labelframe(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);
FL_OBJECT *fl_add_button(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);
FL_OBJECT *fl_add_lightbutton(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);
FL_OBJECT *fl_add_text(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);
FL_OBJECT *fl_add_input(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);
FL_OBJECT *fl_add_slider(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);
FL_OBJECT *fl_add_browser(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);
FL_OBJECT *fl_add_choice(int type, FL_Coord x, FL_Coord y, FL_Coord w, FL_Coord h, const char *label);

void fl_set_object_label(FL_OBJECT *obj, const char *label);
void fl_set_object_callback(FL_OBJECT *obj, FL_CALLBACKPTR cb, long arg);
void fl_deactivate_object(FL_OBJECT *obj);
void fl_activate_object(FL_OBJECT *obj);
void fl_hide_object(FL_OBJECT *obj);
void fl_show_object(FL_OBJECT *obj);
void fl_redraw_object(FL_OBJECT *obj);
void fl_free_object(FL_OBJECT *obj);

void fl_set_button(FL_OBJECT *obj, int pushed);
int  fl_get_button(FL_OBJECT *obj);

void fl_set_input(FL_OBJECT *obj, const char *str);
const char *fl_get_input(FL_OBJECT *obj);

void fl_set_slider_bounds(FL_OBJECT *obj, double min, double max);
void fl_set_slider_value(FL_OBJECT *obj, double val);
double fl_get_slider_value(FL_OBJECT *obj);

void fl_clear_browser(FL_OBJECT *obj);
void fl_add_browser_line(FL_OBJECT *obj, const char *text);
int  fl_get_browser(FL_OBJECT *obj);

void fl_clear_choice(FL_OBJECT *obj);
void fl_addto_choice(FL_OBJECT *obj, const char *text);
int  fl_get_choice(FL_OBJECT *obj);
void fl_set_choice(FL_OBJECT *obj, int line);

void fl_show_form(FL_FORM *form, int place, int border, const char *name);
void fl_hide_form(FL_FORM *form);
void fl_free_form(FL_FORM *form);
void fl_set_form_title(FL_FORM *form, const char *title);

FL_OBJECT *fl_do_forms(void);
FL_OBJECT *fl_check_forms(void);

int fl_add_timeout(long msec, FL_TIMEOUT_CALLBACK cb, void *data);

#ifdef __cplusplus
}
#endif

#endif
