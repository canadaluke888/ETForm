#!/usr/bin/env python3
"""Read-only document demo for TForm's Doc API."""

from __future__ import annotations

from tform import BrailleCanvas, CodeBlock, Doc, Section, Table, Text


def build_doc() -> Doc:
    doc = Doc(
        title="TForm Manual",
        margin_x=30,
        margin_y=1,
        syntax_highlight=True,
        show_border=True,
    )

    intro = Section("Introduction")
    intro.add_h1(Text("TForm Manual", align="center", bold=True, color="cyan"))
    intro.add_p(
        "TForm documents are structured, read-only terminal documents. "
        "Content is rendered vertically in the order it is added."
    )

    capabilities = Table(
        title=Text("Shared Display Objects", align="center", color="yellow", bold=True),
        color="cyan",
    )
    capabilities.add_column("Object")
    capabilities.add_column("Purpose")
    capabilities.add_row("Text", "Styled document text")
    capabilities.add_row("Table", "Structured rows and columns")
    capabilities.add_row("BrailleCanvas", "Compact terminal graphics")
    intro.add(capabilities)

    usage = Section("Usage", boxed=True, padding=1)
    usage.add_h2("Usage")
    usage.add_p(
        "Sections create hierarchy while preserving predictable top-to-bottom rendering."
    )
    usage.add_code_block(
        CodeBlock(
            code=(
                "from tform import Doc, Section\n\n"
                'doc = Doc(title="Manual", margin_x=4)\n'
                'section = Section("Example")\n'
                'section.add_p("Hello from TForm docs.")\n'
                "doc.add_section(section)\n"
                "doc.run()"
            ),
            lang="python",
        )
    )

    canvas = BrailleCanvas(20, 6, color="green", padding=1)
    canvas.draw_circle(canvas.dot_width // 2, canvas.dot_height // 2, 9)
    usage.add(canvas)
    intro.add_section(usage)

    controls = Section("Controls", boxed=True)
    controls.add_h2("Viewer Controls")
    controls.add_p("Use Up and Down to scroll one line at a time.")
    controls.add_p("Use Page Up and Page Down to scroll by a viewport.")
    controls.add_p("Use Home and End to jump to the document boundaries.")
    controls.add_separator("Exit")
    controls.add_p("Press Q or Escape to close the document.")

    doc.add_section(intro)
    doc.add_section(controls)
    return doc


def main() -> int:
    build_doc().run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
