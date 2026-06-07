#!/usr/bin/env python3
"""Demo form for testing TForm from a source checkout."""

from __future__ import annotations

import json

from tform import FilePicker, Form, Text


def build_form() -> Form:
    form = Form(title="Patient Intake")
    file_picker = FilePicker(mode="file", start_dir="..")
    directory_picker = FilePicker(mode="directory", start_dir="..")

    form.add_text_section(Text("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Quisque consectetur ipsum ante, et vulputate nisl mattis ac. Fusce at est arcu. Integer volutpat dui ut ex scelerisque, ac auctor erat pharetra. Nunc vestibulum gravida luctus. Interdum et malesuada fames ac ante ipsum primis in faucibus. Nullam ultrices magna arcu, in scelerisque velit ultricies eget. Etiam pulvinar dui sit amet mi vestibulum cursus. Sed luctus eros odio, at molestie massa aliquam tincidunt. Maecenas elit risus, dignissim ac faucibus in, lacinia vel magna. Nullam sed orci tincidunt, scelerisque enim sed, imperdiet diam. Aliquam eget pulvinar ante. Sed eu justo velit. Pellentesque et orci vehicula, eleifend ipsum id, euismod metus. Mauris fermentum dolor rhoncus, luctus eros vel, tristique enim. Duis tempus, magna sit amet dictum pharetra, lectus justo tincidunt eros, commodo porta velit nisl ut neque.", align="center", color="red", bold=True, wrap_text=True))
    form.add_separator(Text("Patient", color="yellow", bold=True), color="cyan")

    form.add_text_input(
        key="first_name",
        label=Text("First Name", align="center", bold=True, color="cyan"),
        default="",
        description="Patient first name.",
        field_color="cyan",
    )

    form.add_text_input(
        key="last_name",
        label=Text("Last Name", align="right"),
        default="",
        description="Patient last name.",
        field_color="blue",
    )

    form.add_text_input(
        key="referral_file",
        label=Text("Referral File", color="cyan"),
        default="",
        description="Path to a referral file.",
        field_color="cyan",
        tools=file_picker,
    )

    form.add_optional_text_input(
        key="notes",
        label=Text("Notes", align="left", color="white"),
        default="",
        description="Wrapped patient notes.",
        field_color="white",
        wrap_text=True,
    )

    form.add_separator(Text("Vitals", align="center", color="green"), color="yellow")

    form.add_int_input(
        key="age",
        label=Text("Age", align="center", bold=True, color="magenta"),
        default=1,
        description="Patient age.",
        scrollable=True,
        min=0,
        max=130,
        field_color="magenta",
    )

    form.add_int_input(
        key="visit_count",
        label=Text("Visit Count", align="right"),
        default=0,
        description="Unbounded scrollable integer for testing unlimited scrolling.",
        field_color="green",
    )

    form.add_separator(Text("Demographics", align="right", color="blue"), color="magenta")

    form.add_choice_input(
        key="gender",
        label="Gender",
        choices=["Male", "Female"],
        description="Patient gender.",
        field_color="yellow",
    )

    form.add_checkbox_input(
        key="services",
        label=Text("Services", align="center", color="cyan"),
        options=[
            Text("Labs", color="green", align="center"),
            Text("Imaging", color="yellow"),
            Text("Referral", color="magenta"),
        ],
        default=["Labs"],
        description="Requested services.",
        field_color="cyan",
        tick="dot",
    )

    form.add_list_input(
        key="documents",
        label=Text("Documents", align="center"),
        tools=file_picker,
    )

    form.add_list_input(
        key="directories",
        label=Text("Directories", align="center", color="green"),
        tools=directory_picker,
    )

    form.add_list_input(
        key="medications",
        label=Text("Medications", align="center", color="white"),
    )

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
