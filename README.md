# TForm

TForm is a Python framework for building native terminal forms with an ncurses
interface. It is designed for developer settings, configuration screens, setup
flows, and other CLI workflows that need structured input without making users
answer a long sequence of prompts.

The public API is intentionally small: create a `Form`, add fields, then call
`run()`. TForm handles terminal rendering, field navigation, inline editing,
modal file picking, validation bounds for integers, and conversion back into
plain Python values.

## Features

- Native ncurses UI implemented as a CPython extension.
- Text, boolean, choice, checkbox, integer, and list inputs.
- Static sections, text blocks, separators, and tables for grouping larger forms.
- Styled text with alignment, color, bold, italicized, dim, underline, blink,
  strikethrough, and wrapping attributes.
- Inline text editing with cursor movement and clipboard paste.
- Scrollable integer fields with optional `min` and `max` bounds.
- File and directory picker tools for text and list fields.
- Typed package metadata through `py.typed` and `.pyi` stubs.

## Requirements

TForm builds a native extension and links against wide-character ncurses and the
panel library.

On Debian or Ubuntu:

```bash
sudo apt update
sudo apt install python3-dev build-essential libncursesw5-dev libpanel-dev
```

## Install

From a source checkout:

```bash
python3 -m pip install .
```

For editable development:

```bash
python3 -m pip install -e .
```

## Quick Start

```python
from tform import FilePicker, Form, Table, Text


form = Form(title="Project Setup")
commands = Table(
    title=Text("Commands", align="center", bold=True, color="yellow"),
    color="cyan",
)
commands.add_column("Command")
commands.add_column("Default")
commands.add_row("Build", "python3 setup.py build_ext --inplace")
commands.add_row("Demo", "python3 examples/demo.py")

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

values = form.run()
if values is None:
    print("Canceled")
else:
    print(values)
```

Run the included demo from the repository root:

```bash
python3 examples/demo.py
```

## Core API

Import the framework objects from `tform`:

```python
from tform import FilePicker, Form, Table, Text
```

### `Form`

`Form(title: str)` creates a form screen. Add fields in the order they should
appear, then call `run()`.

`run()` returns a `dict[str, Any]` when the user saves the form, or `None` when
the user exits without saving. Static sections, tables, text sections, and
separators are visual only and are not included in the returned dictionary.

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

### `Table`

`Table` renders static tabular content inside `add_section()`.

```python
table = Table(
    title=Text("Command Defaults", align="center", bold=True, color="yellow"),
    color="cyan",
)
table.add_column("Command")
table.add_column("Default")
table.add_row("Build", "python3 setup.py build_ext --inplace")
table.add_separator_row(Text("Validation", color="yellow", dim=True))
table.add_row("Demo", "python3 examples/demo.py")

form.add_section(Text("Developer Settings", color="cyan", bold=True), table, boxed=True)
```

Rows must have the same number of cells as the table has columns. The table
title accepts `str` or `Text`, renders above the table, and is aligned within
the table bounds. Tables keep an outer border by default and omit the inner
grid. Set `show_lines=True` to draw row and column grid lines for headers and
normal rows. Separators are always one full-width row without column dividers;
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
plain text entries. With a `FilePicker`, add and edit actions open the picker.

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
python3 examples/demo.py
```

When saved, it prints the returned form dictionary as JSON.
