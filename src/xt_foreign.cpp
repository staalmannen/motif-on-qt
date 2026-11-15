  if (const char *tm = w->core.widget_class->core_class.tm_table)
    if (XtTranslations t = XtParseTranslationTable (tm)) XtAugmentTranslations (w, t);