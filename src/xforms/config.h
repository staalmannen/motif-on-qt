/* Minimal config.h for motif-on-qt Athena-style import of XForms 1.2.4.
 * Generated to allow compilation against the project's Xlib-on-Qt.
 * Expand as more features are enabled.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* We are building against motif-on-qt's Xlib implementation */
#define HAVE_LIBX11 1
#define HAVE_X11_XLIB_H 1

/* Common Unix features */
#define HAVE_STRDUP 1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_ERRNO_H 1
#define HAVE_LIMITS_H 1
#define HAVE_MATH_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_TIME_H 1
#define TIME_WITH_SYS_TIME 1

/* X11 extensions we may or may not need yet */
/* #undef HAVE_LIBXPM */
/* #undef HAVE_X11_XPM_H */
/* #undef HAVE_LIBXEXT */

/* Version */
#define VERSION "1.2.4"
#define PACKAGE "xforms"
#define PACKAGE_VERSION "1.2.4"

#endif /* CONFIG_H */
