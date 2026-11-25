/* Small XForms-on-Qt client. Not the XForms 1.2 ABI.
 * cmake --build build --target xforms-hello && ./build/xforms-hello
 */
#include <forms.h>
#include <stdio.h>

static void quit_cb(FL_OBJECT *obj, long arg)
{
    (void)obj;
    (void)arg;
    fl_finish();
}

static void slider_cb(FL_OBJECT *obj, long arg)
{
    (void)arg;
    fprintf(stderr, "slider %g\n", fl_get_slider_value(obj));
}

int main(int argc, char **argv)
{
    FL_FORM *form;
    FL_OBJECT *quit, *slider, *browser, *choice;

    fl_initialize(&argc, argv, "XFormsHello", NULL, 0);
    form = fl_bgn_form(FL_UP_BOX, 420, 300);
    fl_add_text(FL_NORMAL_TEXT, 20, 10, 220, 24, "XForms on Qt");
    quit = fl_add_button(FL_NORMAL_BUTTON, 300, 250, 90, 30, "Quit");
    fl_set_object_callback(quit, quit_cb, 0);
    fl_add_input(FL_NORMAL_INPUT, 20, 44, 200, 28, "name");
    slider = fl_add_slider(FL_HOR_SLIDER, 20, 84, 200, 24, "level");
    fl_set_slider_bounds(slider, 0, 10);
    fl_set_slider_value(slider, 3);
    fl_set_object_callback(slider, slider_cb, 0);
    browser = fl_add_browser(FL_NORMAL_BROWSER, 240, 44, 160, 120, "");
    fl_add_browser_line(browser, "one");
    fl_add_browser_line(browser, "two");
    choice = fl_add_choice(FL_NORMAL_CHOICE, 20, 130, 160, 28, "");
    fl_addto_choice(choice, "alpha");
    fl_addto_choice(choice, "beta");
    fl_add_labelframe(FL_UP_BOX, 20, 180, 200, 50, "group");
    fl_end_form();

    fl_show_form(form, FL_PLACE_CENTER, FL_FULLBORDER, "XForms on Qt");
    fl_do_forms();
    fl_free_form(form);
    return 0;
}
