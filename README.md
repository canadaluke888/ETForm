# ETForm

ETForm is a Python framework for building native terminal forms and read-only
documents with an ncurses interface. Forms collect structured input. Documents
render hierarchical manuals, project notes, and other ordered terminal content.

The public API is intentionally small: create a `Form` to collect values or a
`Doc` to render read-only content, add items in display order, then call `run()`.
ETForm handles terminal rendering, navigation, scrolling, and reusable styled
display objects.

## Features

- Native ncurses UI implemented as a CPython extension.
- Text, boolean, choice, checkbox, integer, and list inputs.
- Static sections, text blocks, separators, and `STable` tables for grouping larger forms.
- Interactive `ITable` inputs with editable rows and structured table results.
- Styled text with alignment, color, bold, italicized, dim, underline, blink,
  strikethrough, and wrapping attributes.
- Inline text editing with cursor movement and clipboard paste.
- Scrollable integer fields with optional `min` and `max` bounds.
- File, calendar, and clock picker tools for text and list fields.
- Static Unicode Braille canvases for compact terminal graphics.
- Hierarchical read-only documents with headings, paragraphs, code blocks,
  reusable display objects, margins, boxes, and scrolling.
- Typed package metadata through `py.typed` and `.pyi` stubs.

## Install

Install using PIP:

```bash
pip install etform
```

Windows is not supported and macOS builds have not been tested. Python 3.11 or newer is required.

To build from source, ETForm needs a C compiler, Python development headers,
wide-character ncurses, and the panel library.

On Debian or Ubuntu:

```bash
sudo apt update
sudo apt install python3-dev build-essential libncurses-dev
```

From a source checkout:

```bash
python3 -m pip install .
```

For editable development:

```bash
python3 -m pip install -e .
```

Clipboard paste uses `xclip` when available. Running the interface requires an
interactive terminal with a suitable terminfo database.

## Quick Start

```python
from etform import BrailleCanvas, Calendar, Clock, FilePicker, Form, Image, STable, Text


form = Form(title="Project Setup")
commands = STable(
    title=Text("Commands", align="center", bold=True, color="yellow"),
    color="cyan",
)
commands.add_column("Command")
commands.add_column("Default")
commands.add_row("Build", "python3 setup.py build_ext --inplace")
commands.add_row("Demo", "python3 examples/form_demo.py")

form.add_section(
    Text("Create a project configuration.", color="cyan", bold=True),
    commands,
    boxed=True,
)

form.add_text_input(
    key="name",
    label=Text("Project Name", color="cyan", bold=True),
    description="The display name for this project.",
)

form.add_int_input(
    key="workers",
    label="Workers",
    default=4,
    min=1,
    max=32,
    scrollable=True,
    description="Number of worker processes.",
)

form.add_choice_input(
    key="environment",
    label="Environment",
    choices=["Development", "Staging", "Production"],
    default="Development",
)

form.add_checkbox_input(
    key="features",
    label="Features",
    options=["Logging", "Metrics", "Tracing"],
    default=["Logging"],
)

form.add_text_input(
    key="config_path",
    label="Config File",
    tools=FilePicker(mode="file", start_dir="."),
)

form.add_text_input(
    key="release_date",
    label="Release Date",
    tools=Calendar(),
)

form.add_text_input(
    key="release_time",
    label="Release Time",
    tools=Clock(format="12h", include_seconds=True),
)

values = form.run()
if values is None:
    print("Canceled")
else:
    print(values)
```

Run the included demo from the repository root:

```bash
python3 examples/form_demo.py
```

Run the separate document demo with:

```bash
python3 examples/doc_demo.py
```

## Core API

Import the framework objects from `etform`:

```python
from etform import BrailleCanvas, Calendar, Clock, CodeBlock, Doc, FilePicker, Form, Group, ITable, Image, OrderedList, Paragraph, STable, Text, UnorderedList
```

### `Doc`

`Doc` is the read-only counterpart to `Form`. It renders static document items
vertically in the order passed to `Doc.add()` and supports line scrolling, page
scrolling, and Home/End navigation. Margins reduce the document viewport without
introducing coordinate placement.

```python
from etform import CodeBlock, Doc, Group, OrderedList, Paragraph, Text, UnorderedList

doc = Doc(
    title="Project Notes",
    margin_x=4,
    syntax_highlight=True,
    show_border=True,
)

doc.add(
    Text("Project Notes", align="center", bold=True, color="cyan"),
    Paragraph(
        "ETForm documents support ",
        Text("inline styled text", color="yellow", bold=True),
        " without a markup parser.",
    ),
    UnorderedList(
        "Plain strings become paragraph-style list items",
        Paragraph("Use ", Text("Text", color="cyan"), " for inline spans."),
        OrderedList("Nested ordered item"),
    ),
    CodeBlock(
        code="print('hello from ETForm docs')",
        lang="python",
    ),
)

controls = Group("Controls", boxed=True)
controls.add(
    Paragraph("Use Up and Down to scroll one line at a time."),
    Paragraph("Press Q or Escape to close the document."),
)
doc.add(controls)
doc.run()
```

The page border uses the document margins as its outer boundaries and scrolls
with the document. Its bottom edge is rendered at the actual end of the
document rather than at the bottom of the viewport. Disable it with
`show_border=False`.

`Doc.add()` accepts `str`, `Text`, `Paragraph`, `Group`, `Table`,
`CodeBlock`, `BrailleCanvas`, `Image`, `OrderedList`, and `UnorderedList`, and
returns the document for chaining. Plain strings and standalone `Text` objects
are converted to paragraph-style blocks internally. `Paragraph` is the
block-level container for mixed inline runs; `Text` remains the styled inline
unit. `OrderedList` and `UnorderedList` are static document renderables and are
separate from `Form.add_list_input()`.

`Group(label, boxed=False, padding=1)` groups normal document items together.
It accepts the same renderables through `Group.add()` and can be added with
`Doc.add(group)`. Code blocks preserve indentation and clip safely at the right
margin. When `syntax_highlight=True`, recognized `CodeBlock.lang` values such
as `"python"`, `"c"`, `"javascript"`, `"json"`, and `"shell"` receive
token-based syntax colors. Set it to `False` to render code as plain text.
Press Q or Escape to exit.

### `Form`

`Form(title: str)` creates a form screen. Add fields in the order they should
appear, then call `run()`.

`run()` returns a `dict[str, Any]` when the user saves the form, or `None` when
the user exits without saving. Static sections, tables, text sections, and
separators are visual only and are not included in the returned dictionary.
When static content is taller than the viewport, Up/Down scroll through it in
steps without selecting a field. Page Up/Page Down move by larger viewport
steps, and Home/End jump to the document boundaries. Field selection resumes
when the pending field reaches the visible viewport.

### `ITable`

`ITable` defines an editable table input. Add column names before attaching it
to a form. On entry, the first row is created automatically; type into the
selected cell, use the arrow keys to navigate, Ctrl+R to add a row, Ctrl+D to
delete the selected row, and Ctrl+E to leave the table editor.
Press Ctrl+M to select a source row, use Up/Down to select a destination,
and press Enter to swap them. Both rows are highlighted in their column
colors; Left/Right changes the visible columns, and Escape cancels the swap.
Some terminals send the same key code for Ctrl+M and Enter, so Enter also
starts a swap while editing a table.
Columns use randomly chosen, contrasting colors that remain stable while the
form runs. When the table is wider than the available space, moving between
cells reveals the corresponding columns.

```python
dependencies = ITable(key="dependencies", title="Dependencies", show_lines=True)
dependencies.add_column("Package")
dependencies.add_column("Version")
form.add_table_input(dependencies)
```

The submitted value is a mapping with `columns` and `data` lists. `Table`
remains an alias for `STable`, the static table display used in sections and
documents.

### `Text`

`Text` styles labels, section text, separator labels, and checkbox/list option
labels.

```python
Text(
    "Workspace",
    align="center",
    bold=True,
    italicized=True,
    underline=True,
    color="cyan",
    wrap_text=True,
)
```

Supported alignments are `left`, `center`, and `right`. Supported colors are
`default`, `white`, `cyan`, `yellow`, `green`, `blue`, `magenta`, and `red`.
Text styling is used by labels, section text, separator labels, table titles,
checkbox/list option labels, and text input placeholders. `dim`, `underline`,
and `blink` map to standard curses attributes. `strikethrough` is exposed as a
style flag and renders when the active curses implementation provides a
strikeout attribute.

### `FilePicker`

`FilePicker(mode="file", start_dir=".")` attaches a file or directory browser to
a compatible field.

```python
FilePicker(mode="directory", start_dir=".")
```

Supported modes are `file` and `directory`.

### `Calendar`

`Calendar()` attaches an interactive date picker to text and list fields. It
opens anywhere text input is needed, uses the current `YYYY-MM-DD` value when
available, and returns a valid `YYYY-MM-DD` date string.

```python
form.add_text_input(
    key="release_date",
    label="Release Date",
    tools=Calendar(),
)
```

### `Clock`

`Clock(format="24h", include_seconds=False)` attaches a live digital and Braille
analog time picker to text and list fields. It returns `HH:MM[:SS]` in 24-hour
mode or `hh:MM[:SS] AM/PM` in 12-hour mode.

```python
form.add_text_input(
    key="release_time",
    label="Release Time",
    tools=Clock(format="12h", include_seconds=True),
)
```

### `BrailleCanvas`

`BrailleCanvas(width, height, color="default", padding=1)` stores a 2x4-dot
Unicode Braille raster per terminal cell. Draw with dot coordinates, then pass
the canvas to `add_section()`; the form stores a static copy.

```python
canvas = BrailleCanvas(24, 8, color="cyan")
canvas.draw_circle(canvas.dot_width // 2, canvas.dot_height // 2, 12)
canvas.draw_line(0, 0, canvas.dot_width - 1, canvas.dot_height - 1)
canvas.draw_image("logo.png")
form.add_section(canvas, boxed=True)
```

`draw_image(path)` accepts image formats supported by `stb_image`, including
PNG, JPEG, BMP, GIF, TGA, PSD, HDR, PIC, and PNM. The image is aspect-fitted
and centered in the Braille dot grid. Transparent pixels are composited over
white. Luminance dithering varies Braille dot density to preserve shading,
highlights, and reflections instead of applying a single hard threshold. On
256-color terminals each Braille cell uses the nearest xterm color from the
source image; 8-color and monochrome terminals use the canvas's configured
fallback color. Since one Braille character has one foreground color, all
raised dots within the same 2x4 cell share that color. Drawing overlays existing
canvas content; call `clear()` first when replacement is desired.

### `Image`

`Image` is the portable, image-focused section API. It decodes once when the
object is created and renders through Unicode Braille, so it works without
Kitty, Sixel, or other terminal-specific graphics protocols.

```python
image = Image(
    "logo.png",
    width=48,
    color="cyan",
    padding=1,
)
form.add_section(image, boxed=True)
```

`width` controls the terminal-cell resolution. When `height=None`, the height
is derived from the source aspect ratio in Braille's 2x4 dot coordinate space.
Pass an explicit height to constrain the image to a fixed section area. Images
use 256-color cells when available and retain the configured `color` as the
8-color or monochrome fallback.

### `Table`

`Table` renders static tabular content inside `Doc.add()` or `Form.add_section()`.

```python
table = Table(
    title=Text("Command Defaults", align="center", bold=True, color="yellow"),
    color="cyan",
)
table.add_column("Command")
table.add_column("Default")
table.add_row("Build", "python3 setup.py build_ext --inplace")
table.add_separator_row(Text("Validation", color="yellow", dim=True))
table.add_row("Demo", "python3 examples/form_demo.py")

form.add_section(Text("Developer Settings", color="cyan", bold=True), table, boxed=True)
```

Rows must have the same number of cells as the table has columns. The table
title accepts `str` or `Text`, renders above the table, and is aligned within
the table bounds. Tables keep an outer border and a rule below the header by
default, while omitting body row and column grid lines. Set `show_lines=True`
to draw row and column grid lines for headers and normal rows. Separators are
always one full-width row without column dividers;
they are blank by default, accept a plain string for left-aligned unstyled text,
or accept `Text` for formatted and aligned separator text.

## Field Types

### Text Input

```python
form.add_text_input(
    key="project_name",
    label="Project Name",
    default="",
    placeholder=Text("None", align="right", color="cyan", italicized=True),
    description="Project display name.",
    field_color="cyan",
)
```

Text fields return strings. `placeholder` accepts `str` or `Text` and is
display-only ghost text shown when the field value is empty. `Text`
placeholders keep their alignment, color, bold, italicized, dim, underline,
blink, strikethrough, and wrapping attributes within the input box. `default` is
not rendered or inserted into the field; it is returned only when the user
leaves the field empty. When
`wrap_text=True`, the field can grow over multiple display lines and supports
vertical cursor movement while editing.

### Boolean Input

```python
form.add_bool_input(
    key="enabled",
    label="Enabled",
    default=True,
)
```

Boolean fields return `True` or `False`.

### Choice Input

```python
form.add_choice_input(
    key="priority",
    label="Priority",
    choices=["Low", "Normal", "High"],
    default="Normal",
)
```

Choice fields return the selected string.

### Checkbox Input

```python
form.add_checkbox_input(
    key="enabled_tools",
    label="Enabled Tools",
    options=["Formatter", "Linter", "Type Checker", "Test Runner"],
    default=["Formatter", "Linter"],
    tick="dot",
)
```

Checkbox groups return a dictionary mapping each option label to `True` or
`False`. Supported tick styles are `x`, `block`, and `dot`.

### Integer Input

```python
form.add_int_input(
    key="max_workers",
    label="Max Workers",
    default=8,
    min=1,
    max=64,
    scrollable=True,
)
```

Integer fields return Python integers. Scrollable integer fields can be adjusted
with the arrow keys and can enforce optional `min` and `max` bounds.

### List Input

```python
form.add_list_input(
    key="environment_files",
    label="Environment Files",
    tools=FilePicker(mode="file", start_dir="."),
)
```

List fields return `list[str]`. Without tools, users can add, edit, and delete
plain text entries. With a `FilePicker`, `Calendar`, or `Clock`, add and edit actions open
the attached tool.

### Static Sections and Separators

```python
form.add_section(
    Text("Developer Settings", color="cyan", bold=True),
    table,
    boxed=True,
)
form.add_separator(Text("Build & Test", color="green"), color="yellow")
```

Static sections accept `Text`, `Table`, and plain strings, then render them in
order. `add_text_section()` remains available for compatibility, but
`add_section()` is the preferred static-content API.

## Controls

The footer shows context-sensitive controls while the form is running. The main
navigation controls are:

- `Up` / `Down`: move between fields.
- `Enter`: edit the selected field.
- `I`: show the selected field description.
- `Ctrl+S`: save and return the form values.
- `Q`: exit without saving and return `None`.

Text editing controls:

- `Left` / `Right`: move the cursor.
- `Home` / `End`: jump to the start or end of the text.
- `Delete` / `Backspace`: remove text.
- `Ctrl+V`: paste clipboard text.
- `Enter` or `Ctrl+E`: finish editing.
- `Esc`: clear the field and finish editing.

List editing controls:

- `A`: add an item.
- `E`: edit or replace the selected item.
- `D`: delete the selected item.
- `Ctrl+E` or `Esc`: leave list editing.

Checkbox controls:

- `Up` / `Down`: move between options.
- `Space` or `Enter`: toggle the selected option.
- `Ctrl+E` or `Esc`: leave checkbox editing.

File picker controls:

- `Up` / `Down`: move between entries.
- `Enter`: open a directory or choose a file.
- `B`: move back to the parent directory.
- `Ctrl+E`: choose the current directory in directory mode.
- `Esc`: close the picker.

## Demo

The bundled developer settings demo exercises the major field types:

```bash
python3 examples/form_demo.py
```

When saved, it prints the returned form dictionary as JSON.
