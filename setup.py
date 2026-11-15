from pathlib import Path

from setuptools import Extension, setup


sources = [
    "src/etformmodule.c",
    "src/form_object.c",
    "src/form_runtime.c",
    "src/doc_object.c",
    "src/doc_runtime.c",
    "src/syntax_highlighter.c",
    "src/form_helpers.c",
    "src/dialogs.c",
    "src/braille_canvas.c",
    "src/panel_manager.c",
    "src/ui_footer.c",
    "src/ui_text.c",
    "src/ui_list.c",
    "src/terminal_control.c",
]

setup(
    name="etform",
    version=Path("VERSION").read_text(encoding="utf-8").strip(),
    description="Native ncurses forms and read-only terminal documents",
    long_description=Path("README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    python_requires=">=3.11",
    url="https://github.com/canadaluke888/TForm",
    classifiers=[
        "Operating System :: POSIX :: Linux",
        "Programming Language :: C",
        "Programming Language :: Python :: 3 :: Only",
        "Programming Language :: Python :: Implementation :: CPython",
        "Topic :: Software Development :: User Interfaces",
    ],
    packages=["etform"],
    package_data={"etform": ["py.typed", "*.pyi"]},
    ext_modules=[
        Extension(
            "etform._etform",
            sources=sources,
            include_dirs=["src"],
            depends=[
                "src/braille_canvas.h",
                "src/doc_object.h",
                "src/doc_runtime.h",
                "src/form_object.h",
                "src/form_runtime.h",
                "src/syntax_highlighter.h",
                "src/third_party/stb_image.h",
            ],
            libraries=["ncursesw", "panelw", "m"],
            extra_compile_args=[
                "-std=c11",
                "-D_POSIX_C_SOURCE=200809L",
                "-Wall",
                "-Wextra",
                "-Wno-cast-function-type",
            ],
        )
    ],
)
