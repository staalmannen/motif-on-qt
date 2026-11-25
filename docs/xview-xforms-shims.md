# Thin Qt Shims for XView, XForms (and Athena)

Athena-style fast import of the original sources is a useful **initial transition state**.
Ultimately the goal is to integrate Athena, XView and XForms as **thin shims over Qt**,
in the same style as the existing Motif implementation: genuine public headers, real
(or sufficiently compatible) object/instance records where the API requires them,
resource/attribute tables, and `MqClass`-style (or equivalent) create/apply/fetch hooks
that map packages/objects onto Qt widgets and layouts.

This document records the analysis of what is required and concrete package/object
mappings.

## Shared foundation (already present)

- Xlib-on-Qt (`src/xlib.cpp` and related): windows backed by QImages, GCs → QPainter,
  colours, fonts (XLFD), events, pixmaps, XImages, input methods, selections/clipboard.
- Qt event loop integration (`XtAppMainLoop` path and friends).
- Testing harness: `MOTIFQT_SCRIPT`, offscreen, screenshots, tree dump, autoquit.
- HiDPI, text encoding helpers (`mq_mb_decode` / `mq_mb_encode`).

Any gaps that surface when linking real XView or XForms sources against the current
Xlib layer become high-priority Xlib work; the same discipline used for Athena applies.

## Athena-style import scaffolding (in tree)

| Toolkit | Location            | Status |
|---------|---------------------|--------|
| XForms  | `src/xforms/`       | README + minimal `config.h`; drop official 1.2.4 lib sources here |
| XView   | `src/xview/`        | README with recommended source trees and layout |

See the READMEs in those directories for exact vendor steps. Once the sources are
present, add CMake options (`MQ_BUILD_XFORMS`, `MQ_BUILD_XVIEW`) following the
Athena pattern and start resolving missing Xlib symbols.

## Athena (Xaw) — current state and target

**Current (transition):** Genuine libXaw 1.0.16 + libXmu sources under `src/xaw/` and
`src/xmu/` run as foreign classes via `xt_foreign.cpp`. Drawing goes through our Xlib.
Selected widgets (Label, Command, Toggle, MenuButton, Repeater, Scrollbar, SimpleMenu)
optionally receive Qt-style faces from `xaw_qt.cpp` (`MOTIFQT_XAW_CLASSIC=1` restores
classic drawing).

**Target (thin shims):** Replace the foreign-class path for the core set with Motif-style
`MqClass` implementations that own Qt widgets/layouts, keep the Xt instance records in
step, and continue to support program-defined subclasses. Classic faces remain available
as a painting option. The genuine sources can stay for reference and for any widgets that
are rarely used or that are pure drawing (e.g. some of the text/sink machinery).

### Athena widget → Qt mapping (core set)

| Athena class          | Typical Qt counterpart                          | Notes |
|-----------------------|--------------------------------------------------|-------|
| Label                 | QLabel                                           | Alignment, font, pixmap, justify |
| Command               | QPushButton                                      | Callbacks, set state |
| Toggle                | QCheckBox / QRadioButton                         | Radio groups |
| MenuButton            | QPushButton + QMenu                              | |
| SimpleMenu / SmeBSB / SmeLine | QMenu / QAction / separator               | Popup with grab |
| Scrollbar             | QScrollBar                                       | Orientation, thumb, callbacks |
| Repeater              | QPushButton (auto-repeat)                        | |
| Box                   | QHBoxLayout / QVBoxLayout / QGridLayout          | |
| Form                  | Custom QLayout (attachments) or QFormLayout      | |
| Paned                 | QSplitter                                        | |
| Viewport / Porthole   | QScrollArea                                      | |
| Panner                | Custom (or QScrollBar pair)                      | |
| Text / AsciiText      | QPlainTextEdit / QLineEdit                       | Source/sink complexity; keep Xlib drawing path for full fidelity if needed |
| List                  | QListWidget                                      | |
| Dialog / TransientShell | QDialog / QWidget (transient)                  | |
| Tip                   | Qt tooltips (already done)                       | |

Geometry and constraint handling for Form/Box follow the same iterative or packing rules
already implemented for Motif managers. Resources come from the existing generated tables
or from the Xaw `XtResource` lists.

## XView (OPEN LOOK)

XView is **not** Xt-based. It provides an object-oriented C API with opaque handles,
packages, and attribute lists (`xv_create`, `xv_set`, `xv_get`, …). The Notifier replaces
the Xt main loop. Graphics primitives live in libolgx.

**Implementation started (thin shim, not a source import).** Because the toolkit is
large and not Xt-based, the primary path is Motif-style: public C calls over Qt.
`include/xview/xview.h`, `src/xview/xv_api.cpp` and `src/xview/olgx.cpp` cover FRAME,
PANEL (button, message, text, slider), CANVAS, MENU/MENUITEM, SCROLLBAR, `xv_notice`,
and the common olgx drawing calls (box, button, choice, text, scrollbar, menu mark)
on QPainter. Build with `cmake --build build --target xview-hello`. This is a
source-level subset (our attribute numbers), not the Sun ABI. A real XView tree can
still be kept aside as a behaviour oracle; it is not linked into the library.

### Core packages and suggested Qt mappings

| XView package              | Role                                      | Suggested Qt / Motif-on-Qt mapping |
|----------------------------|-------------------------------------------|------------------------------------|
| SERVER / SCREEN            | Connection, display metrics               | Existing Xlib Display/Screen       |
| FRAME (BASE, CMD, …)       | Top-level / command windows, footers, pin | QMainWindow / QDialog + status bar |
| WINDOW                     | Generic window                            | QWidget / MqCanvas                 |
| PANEL                      | Control panel container                   | QWidget + QLayout (H/V/grid)       |
| PANEL_BUTTON               | Action button                             | QPushButton                        |
| PANEL_TOGGLE / CHOICE / CHECK_BOX | Choice / radio / check             | QCheckBox / QRadioButton / QButtonGroup |
| PANEL_TEXT / NUMERIC_TEXT / MULTILINE_TEXT | Text entry                    | QLineEdit / QSpinBox / QPlainTextEdit |
| PANEL_SLIDER / GAUGE       | Numeric slider / indicator                | QSlider / QProgressBar             |
| PANEL_LIST                 | Scrolling list                            | QListWidget                        |
| PANEL_MESSAGE              | Static label                              | QLabel                             |
| SCROLLBAR                  | Scrollbar                                 | QScrollBar                         |
| CANVAS (+ paint window)    | Drawing surface                           | MqCanvas (Xlib drawing)            |
| MENU / MENU_ITEM           | Menus                                     | QMenu / QAction                    |
| NOTICE                     | Modal message                             | QMessageBox                        |
| ICON                       | Icon                                      | QIcon / window icon                |
| FONT / CMS / CURSOR        | Font, colour map, cursor                  | Existing Xlib / QFont / QCursor    |
| TEXTSW / TTYSW             | Text / terminal widgets                   | QPlainTextEdit / QTermWidget-style or canvas + Xlib |
| FILE_CHOOSER               | File dialog                               | QFileDialog (or Motif FileSelectionBox shim) |
| SELECTION / DRAGDROP       | Selections, DnD                           | QClipboard + Qt DnD                |
| NOTIFIER                   | Event / timer / input dispatch            | Qt event loop + QTimer / QSocketNotifier |

### Implementation notes for a thin XView shim

1. **Public headers** matching the XView 3.2 API (packages, attributes, `xv_*` functions).
2. **Object registry** analogous to `MqClass` / `MqRec`: each package has create / set / get /
   destroy hooks; the opaque `Xv_opaque` handle points at our private data + any required
   public record.
3. **Attribute tables** (name, type, size, default) extracted or hand-maintained from the
   original headers / documentation; `xv_set`/`xv_get` write the correct number of bytes.
4. **Notifier** bridged to the existing Qt event loop (timeouts, file descriptors, X events
   already translated for Xt). `xv_main_loop` becomes a thin wrapper around the Qt loop.
5. **libolgx** either reimplemented over QPainter (OPEN LOOK look) or given a “classic vs
   themed” switch like Athena.
6. **Geometry / layout** for PANEL and FRAME follow OPEN LOOK rules (or approximate with
   Qt layouts + residual Xlib positioning for canvas children).

Athena-style import of the real sources remains the fastest way to discover missing Xlib
surface and to run existing clients while the thin shims are built package-by-package.

## XForms (Forms Library)

XForms is a pure-Xlib toolkit with a simple C API: forms contain objects of many classes.
It has its own event loop (`fl_do_forms` / callbacks) and an extensible object-class
architecture. Sources (1.2.4) are LGPL and still packaged.

**Implementation started (thin shim, not a source import).** `include/forms.h` and
`src/xforms/fl_api.cpp` cover a form plus box, frame, labelframe, button, lightbutton,
text, input, slider, browser and choice, with `fl_do_forms` on the Qt event loop.
Build with `cmake --build build --target xforms-hello`. This is a source-level subset,
not the XForms 1.2 ABI. The old `config.h` is still there if a real tree is vendored
later as an oracle.

### Core object classes and suggested Qt mappings

| XForms class / object     | Role                                      | Suggested Qt mapping |
|---------------------------|-------------------------------------------|----------------------|
| FL_FORM                   | Top-level form / window                   | QWidget / QDialog    |
| Box / Frame / Labelframe  | Decorative containers                     | QFrame / QGroupBox   |
| Button (normal, light, round, …) | Push / toggle buttons                | QPushButton / QToolButton / QCheckBox |
| Slider / Scrollbar / Dial / Counter | Valuators                         | QSlider / QScrollBar / QDial / QSpinBox |
| Input (normal, multiline, …) | Text entry                             | QLineEdit / QPlainTextEdit |
| Browser / Select / Nmenu  | Lists / choice / menus                   | QListWidget / QComboBox / QMenu |
| Choice / Menu             | Pop-up choice                             | QComboBox / QMenu    |
| Text / Bitmap / Pixmap    | Static text / images                      | QLabel               |
| Canvas                    | Free drawing surface                      | MqCanvas (Xlib)      |
| Timer                     | Timed objects                             | QTimer               |
| Tabfolder / Folder        | Notebook-style                            | QTabWidget           |
| Free object               | Application-drawn                         | MqCanvas + expose    |

### Implementation notes for a thin XForms shim

1. Public headers for the Forms Library API (`fl_*`, `FL_*`).
2. Form and object registries with create / set / get / destroy hooks; `FL_OBJECT` points at
   our private data while exposing the fields the public API and free-object handlers expect.
3. Event loop: `fl_do_forms` (and the callback model) driven from the Qt event loop; mouse,
   keyboard and expose events already arrive via the Xlib layer.
4. Drawing helpers and image support that currently use Xlib/Xpm map directly onto the
   existing implementation.
5. Geometry is mostly absolute or simple packing; Qt layouts can be used for containers while
   free/canvas objects keep Xlib coordinates.

Because the object set is bounded and the design is clean, XForms is the lowest-effort
target for a first complete thin-shim library after the Athena core.

## Recommended sequence

1. **Vendor the sources** into `src/xforms/` and `src/xview/` (see the READMEs there).
2. **Xlib completeness** — any missing calls discovered by linking the real sources.
3. **Athena thin-shim conversion** of the already-faced widgets (Label, Command, Toggle,
   MenuButton, Scrollbar, SimpleMenu, …) while keeping foreign-class fallback for the rest.
4. **XForms prototype** — form + button / input / slider / browser; then grow the object set.
5. **XView prototype** — FRAME + PANEL + a handful of panel items + CANVAS; Notifier bridge;
   then menus, notices, textsw.
6. Packaging: optional components, linker scripts / pkg-config entries following the Motif /
   Xaw pattern, so unmodified clients can be built with the same `motif-on-qt-env` style
   wrappers.

## Testing

Reuse the existing harness:

```bash
QT_QPA_PLATFORM=offscreen \
MOTIFQT_SCRIPT="wait 500; click …; shot out.png; dump; quit" \
./xview-or-xforms-client
```

Side-by-side screenshots against real XView / XForms under Xvfb remain the acceptance
criterion, exactly as for Motif and Athena.

## References inside the tree

- `docs/architecture.md` — Motif / Xt / Xlib layering and `MqClass` model
- `docs/target-applications.md` — clients that exercise the current Motif/Athena support
- `src/xaw_qt.cpp` — example of Qt faces on top of foreign Athena classes
- `src/xt_foreign.cpp` — how program-defined (and currently Athena) classes are driven
- `src/xforms/README`, `src/xview/README` — vendor steps for the Athena-style imports

This analysis is intended as a living design note; update the mapping tables as individual
packages/objects are implemented.
