#!/usr/bin/env python3
"""Read-only document demo for TForm's Doc API."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tform import (
    BrailleCanvas,
    CodeBlock,
    Doc,
    Group,
    OrderedList,
    Paragraph,
    Table,
    Text,
    UnorderedList,
)


def build_doc() -> Doc:
    doc = Doc(
        title="TForm Manual",
        margin_x=30,
        margin_y=1,
        syntax_highlight=True,
        show_border=True,
    )

    capabilities = Table(
        title=Text("Shared Display Objects", align="center", color="yellow", bold=True),
        color="cyan",
    )
    capabilities.add_column("Object")
    capabilities.add_column("Purpose")
    capabilities.add_row("Text", "Styled inline text")
    capabilities.add_row("Paragraph", "Mixed inline text block")
    capabilities.add_row("OrderedList", "Numbered static list")
    capabilities.add_row("UnorderedList", "Bulleted static list")

    canvas = BrailleCanvas(20, 6, color="green", padding=1)
    canvas.draw_circle(canvas.dot_width // 2, canvas.dot_height // 2, 9)

    doc.add(
        Text("TForm Manual", align="center", bold=True, color="cyan"),
        Paragraph(
            "TForm documents support ",
            Text("granular inline styling", color="yellow", bold=True),
            " without a markup parser."
        ),
        UnorderedList(
            "Plain strings become paragraph-style list items",
            Paragraph("Use ", Text("Text", color="cyan"), " for inline spans."),
            OrderedList(
                "Nested ordered list item",
                Paragraph(
                    "Nested items can contain ",
                    Text("styled text", underline=True),
                ),
            ),
        ),
        capabilities,
        CodeBlock(
            code=(
                "from tform import Doc, Paragraph, Text, UnorderedList\n\n"
                'doc = Doc(title="Manual", margin_x=4)\n'
                "doc.add(\n"
                '    Paragraph("Hello ", Text("from TForm docs", bold=True)),\n'
                '    UnorderedList("Static list item"),\n'
                ")\n"
                "doc.run()"
            ),
            lang="python",
        ),
        canvas,
    )

    controls = Group("Viewer Controls", boxed=True)
    controls.add(
        Paragraph("Use Up and Down to scroll one line at a time."),
        Paragraph("Use Page Up and Page Down to scroll by a viewport."),
        Paragraph("Use Home and End to jump to the document boundaries."),
        Text("Exit", bold=True, color="yellow"),
        Paragraph("Press Q or Escape to close the document."),
    )
    doc.add(controls)
    return doc


def main() -> int:
    build_doc().run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
