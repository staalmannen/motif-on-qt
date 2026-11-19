/* Small XView-on-Qt client: frame, panel items, and a canvas that paints
 * with libolgx. Build: cmake -B build && cmake --build build --target xview-hello
 */
#include <olgx/olgx.h>
#include <xview/xview.h>

#include <stdio.h>

static int quit_proc(Xv_object item, void *event)
{
    (void)item;
    (void)event;
    notify_stop();
    return 0;
}

static int slider_proc(Xv_object item, void *event)
{
    (void)event;
    fprintf(stderr, "slider %d\n", xv_get(item, PANEL_VALUE));
    return 0;
}

static void paint(Xv_object canvas)
{
    (void)canvas;
    Graphics_info *g = olgx_main_initialize(NULL, 0);
    olgx_set_three_d(g, 1);
    olgx_draw_button(g, 12, 12, 120, 28, "Open Look", OLGX_NORMAL);
    olgx_draw_button(g, 144, 12, 120, 28, "Default", OLGX_DEFAULT);
    olgx_draw_button(g, 276, 12, 120, 28, "Pressed", OLGX_INVOKED);
    olgx_draw_choice(g, 12, 56, 160, 28, "Choice", OLGX_NORMAL);
    olgx_draw_text(g, "libolgx on QPainter", 12, 110, 240, OLGX_NORMAL);
    olgx_draw_scrollbar(g, 12, 130, 200, 1, 20, 40, 100);
    olgx_destroy(g);
}

int main(int argc, char **argv)
{
    Frame frame;
    Panel panel;

    xv_init(XV_INIT_ARGS, argc, argv, NULL);
    frame = xv_create(NULL, FRAME,
                      FRAME_LABEL, "XView on Qt",
                      FRAME_LEFT_FOOTER, "low-hanging packages",
                      XV_WIDTH, 520,
                      XV_HEIGHT, 360,
                      NULL);
    panel = xv_create(frame, PANEL, NULL);
    xv_create(panel, PANEL_MESSAGE,
              PANEL_LABEL_STRING, "Panel message (QLabel)",
              NULL);
    xv_create(panel, PANEL_BUTTON,
              PANEL_LABEL_STRING, "Quit",
              PANEL_NOTIFY_PROC, quit_proc,
              NULL);
    xv_create(panel, PANEL_TEXT,
              PANEL_LABEL_STRING, "edit me",
              NULL);
    xv_create(panel, PANEL_SLIDER,
              PANEL_MIN_VALUE, 0,
              PANEL_MAX_VALUE, 50,
              PANEL_VALUE, 10,
              PANEL_NOTIFY_PROC, slider_proc,
              NULL);
    xv_create(panel, CANVAS,
              XV_HEIGHT, 180,
              CANVAS_REPAINT_PROC, paint,
              NULL);
    xv_main_loop(frame);
    return 0;
}
