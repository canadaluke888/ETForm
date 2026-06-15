from setuptools import Extension, setup


sources = [
    "src/tformmodule.c",
    "src/form_object.c",
    "src/form_runtime.c",
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
    name="tform",
    version="0.12.1",
    description="Native ncurses forms styled after Ghostcode Config Edit",
    packages=["tform"],
    package_data={"tform": ["py.typed", "*.pyi"]},
    ext_modules=[
        Extension(
            "tform._tform",
            sources=sources,
            include_dirs=["src"],
            depends=["src/braille_canvas.h", "src/third_party/stb_image.h"],
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
