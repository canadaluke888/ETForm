#!/usr/bin/env python3
"""Developer settings form for testing ETForm from a source checkout."""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from etform import BrailleCanvas, Calendar, Clock, FilePicker, Form, ITable, Image, Table, Text


def build_form() -> Form:
    repository_root = Path(__file__).resolve().parents[1]
    form = Form(title="Developer Settings")
    file_picker = FilePicker(mode="file", start_dir=".")
    directory_picker = FilePicker(mode="directory", start_dir=".")
    calendar = Calendar()
    clock = Clock(format="12h", include_seconds=True)
    canvas = BrailleCanvas(18, 6, color="cyan", padding=1)
    canvas.draw_circle(canvas.dot_width // 2, canvas.dot_height // 2, 9)
    canvas.draw_line(2, canvas.dot_height - 3, canvas.dot_width - 3, 2)
    image = Image(
        repository_root / "assets" / "logo.png",
        width=36,
        color="green",
        padding=1,
    )
    command_table = Table(
        title=Text("Command Defaults", align="center", bold=True, color="yellow"),
        color="cyan",
        padding=1,
        show_lines=True
    )
    command_table.add_column("Command")
    command_table.add_column("Default")
    command_table.add_column("Purpose")
    command_table.add_row("Build", "python3 setup.py build_ext --inplace", "Compile extension")
    command_table.add_row("Demo", "python3 examples/demo.py", "Manual smoke")
    command_table.add_separator_row(Text("Validation", color="yellow", dim=True, align="center"))
    command_table.add_row("Check", "python3 -m py_compile examples/demo.py", "Syntax check")

    form.add_section(
        Text(
            "Configure local project defaults, editor behavior, and development tooling for this workspace.",
            align="center",
            color="cyan",
            bold=True,
            wrap_text=True,
        ),
        command_table,
        canvas,
        image,
        boxed=True,
    )
    form.add_separator(Text("Workspace", color="yellow", bold=True), color="cyan")

    form.add_text_input(
        key="project_name",
        label=Text("Project Name", align="center", bold=True, color="cyan"),
        default="ETForm",
        placeholder=Text("ETForm", align="center", color="blue", italicized=True, underline=True),
        description="Display name used in local development prompts and generated output.",
        field_color="cyan",
    )

    form.add_text_input(
        key="workspace_root",
        label=Text("Workspace Root", align="right", color="green"),
        default=".",
        placeholder=Text("Select workspace root", align="right", color="green", dim=True),
        description="Root directory for source, tests, and generated artifacts.",
        field_color="green",
        tools=directory_picker,
    )

    form.add_text_input(
        key="config_file",
        label=Text("Config File", color="cyan"),
        default="pyproject.toml",
        placeholder=Text("Select config file", color="cyan", italicized=True, dim=True),
        description="Primary project configuration file.",
        field_color="blue",
        tools=file_picker,
    )

    form.add_text_input(
        key="project_notes",
        label=Text("Project Notes", align="left", color="white"),
        default="Use this workspace for local builds, linting, and release prep.",
        placeholder=Text("Add project notes here", color="white", wrap_text=True, italicized=True, dim=True),
        description="Optional notes shown to developers who open this settings profile.",
        field_color="white",
        wrap_text=True,
    )

    form.add_text_input(
        key="release_date",
        label=Text("Release Date", align="center", color="yellow"),
        default="2026-06-08",
        placeholder=Text("YYYY-MM-DD", align="center", color="yellow", dim=True),
        description="Target date for the next local release checkpoint.",
        field_color="yellow",
        tools=calendar,
    )

    form.add_text_input(
        key="release_time",
        label=Text("Release Time", align="center", color="cyan"),
        default="09:30:00 AM",
        placeholder=Text("hh:mm:ss AM", align="center", color="cyan", dim=True),
        description="Target local time for the next release checkpoint.",
        field_color="cyan",
        tools=clock,
    )

    form.add_separator(Text("Editor", align="center", color="green"), color="yellow")

    form.add_choice_input(
        key="editor_theme",
        label=Text("Theme", align="center", color="yellow"),
        choices=["System", "Dark", "Light", "High Contrast"],
        default="Dark",
        description="Preferred editor color theme.",
        field_color="yellow",
    )

    form.add_int_input(
        key="tab_width",
        label=Text("Tab Width", align="center", bold=True, color="magenta"),
        default=4,
        description="Spaces used when rendering tabs.",
        scrollable=True,
        min=2,
        max=8,
        field_color="magenta",
    )

    form.add_bool_input(
        key="format_on_save",
        label=Text("Format On Save", align="right", color="green"),
        default=True,
        description="Run the configured formatter whenever a file is saved.",
        field_color="green",
    )

    form.add_separator(Text("Build & Test", align="right", color="blue"), color="magenta")

    form.add_int_input(
        key="max_workers",
        label=Text("Max Workers", align="right"),
        default=8,
        description="Maximum number of parallel jobs for builds and tests.",
        scrollable=True,
        min=1,
        max=64,
        field_color="green",
    )

    form.add_choice_input(
        key="test_profile",
        label="Test Profile",
        choices=["Unit", "Integration", "Full Suite"],
        default="Unit",
        description="Default test scope for local verification.",
        field_color="yellow",
    )

    form.add_checkbox_input(
        key="enabled_tools",
        label=Text("Enabled Tools", align="center", color="cyan"),
        options=[
            Text("Formatter", color="green", align="center"),
            Text("Linter", color="yellow"),
            Text("Type Checker", color="magenta"),
            Text("Test Runner", color="cyan"),
        ],
        default=["Formatter", "Linter", "Test Runner"],
        description="Developer tools enabled for this workspace.",
        field_color="cyan",
        tick="dot",
    )

    form.add_separator(Text("Paths & Commands", align="center", color="white"), color="blue")

    form.add_list_input(
        key="source_directories",
        label=Text("Source Directories", align="center"),
        default=["src", "tests", "examples"],
        description="Directories scanned by tooling.",
        field_color="white",
    )

    form.add_list_input(
        key="watch_directories",
        label=Text("Watch Directories", align="center", color="green"),
        description="Directories watched by local development commands.",
        field_color="green",
        tools=directory_picker,
    )

    form.add_list_input(
        key="environment_files",
        label=Text("Environment Files", align="center", color="cyan"),
        description="Environment files loaded before running commands.",
        field_color="cyan",
        tools=file_picker,
    )

    form.add_list_input(
        key="milestone_dates",
        label=Text("Milestone Dates", align="center", color="yellow"),
        default=["2026-06-08"],
        description="Important project dates selected with the calendar tool.",
        field_color="yellow",
        tools=calendar,
    )

    form.add_list_input(
        key="reminder_times",
        label=Text("Reminder Times", align="center", color="cyan"),
        default=["09:30:00 AM"],
        description="Local reminder times selected with the clock tool.",
        field_color="cyan",
        tools=clock,
    )

    form.add_list_input(
        key="extra_arguments",
        label=Text("Extra Arguments", align="center", color="white"),
        default=["--verbose", "--color=always"],
        description="Additional arguments appended to local build and test commands.",
        field_color="white",
    )

    dependencies = ITable(
        key="dependencies",
        title=Text("Dependencies", color="cyan", bold=True),
        color="cyan",
        show_lines=True,
    )
    dependencies.add_column("Package")
    dependencies.add_column("Version")
    form.add_table_input(dependencies)

    return form


def main() -> int:
    values = build_form().run()
    if values is None:
        print("Canceled")
        return 1

    print(json.dumps(values, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
