QObject *mq_canvas_create (Widget w);
QObject *mq_xaw_create (Widget w);        /* native Qt controls for supported Athena widgets */
bool mq_xaw_native (Widget w);
void mq_xaw_sync (Widget w);
void mq_toggle_set (Widget w, bool on, bool notify);