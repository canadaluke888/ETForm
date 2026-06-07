from collections.abc import Sequence
from typing import Any, Literal

LabelAlign = Literal["left", "center", "right"]
TextColor = Literal["default", "white", "cyan", "yellow", "green", "blue", "magenta", "red"]
FilePickerMode = Literal["file", "directory"]
CheckboxTick = Literal["x", "block", "dot"]
Label = str | "Text"


class Text:
    def __init__(
        self,
        value: str,
        align: LabelAlign = "left",
        bold: bool = False,
        color: TextColor = "default",
        wrap_text: bool = False,
    ) -> None: ...

    @property
    def value(self) -> str: ...

    @property
    def align(self) -> LabelAlign: ...

    @property
    def bold(self) -> bool: ...

    @property
    def color(self) -> TextColor: ...

    @property
    def wrap_text(self) -> bool: ...


class FilePicker:
    def __init__(self, mode: FilePickerMode = "file", start_dir: str = ".") -> None: ...

    @property
    def mode(self) -> FilePickerMode: ...

    @property
    def start_dir(self) -> str: ...


class Form:
    def __init__(self, title: str) -> None: ...

    def add_text_input(
        self,
        key: str,
        label: Label,
        default: str = "",
        description: str = "",
        field_color: TextColor = "default",
        wrap_text: bool = False,
        tools: FilePicker | Sequence[FilePicker] | None = None,
        padding: int = 1,
    ) -> None: ...

    def add_optional_text_input(
        self,
        key: str,
        label: Label,
        default: str = "",
        description: str = "",
        field_color: TextColor = "default",
        wrap_text: bool = False,
        tools: FilePicker | Sequence[FilePicker] | None = None,
        padding: int = 1,
    ) -> None: ...

    def add_separator(
        self,
        label: Label = "",
        color: TextColor = "default",
        padding: int = 1,
    ) -> None: ...

    def add_text_section(
        self,
        text: Label,
        padding: int = 1,
    ) -> None: ...

    def add_bool_input(
        self,
        key: str,
        label: Label,
        default: bool = False,
        description: str = "",
        field_color: TextColor = "default",
        padding: int = 1,
    ) -> None: ...

    def add_choice_input(
        self,
        key: str,
        label: Label,
        choices: Sequence[str],
        default: str | None = None,
        description: str = "",
        field_color: TextColor = "default",
        padding: int = 1,
    ) -> None: ...

    def add_checkbox_input(
        self,
        key: str,
        label: Label,
        options: Sequence[Label],
        default: Sequence[str] | None = None,
        description: str = "",
        field_color: TextColor = "default",
        tick: CheckboxTick = "x",
        padding: int = 1,
    ) -> None: ...

    def add_int_input(
        self,
        key: str,
        label: Label,
        default: int = 0,
        description: str = "",
        scrollable: bool = False,
        min: int | None = None,
        max: int | None = None,
        field_color: TextColor = "default",
        padding: int = 1,
    ) -> None: ...

    def add_list_input(
        self,
        key: str,
        label: Label,
        default: Sequence[Label] | None = None,
        description: str = "",
        field_color: TextColor = "default",
        tools: FilePicker | Sequence[FilePicker] | None = None,
        padding: int = 1,
    ) -> None: ...

    def run(self) -> dict[str, Any] | None: ...
