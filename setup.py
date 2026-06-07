from setuptools import Extension, setup


sources = [
    "src/tformmodule.c",
    "src/form_object.c",
    "src/form_runtime.c",
    "src/form_helpers.c",
    "src/dialogs.c",
    "src/panel_manager.c",
    "src/ui_footer.c",
    "src/ui_text.c",
    "src/ui_list.c",
    "src/terminal_control.c",
]

setup(
    name="tform",
    version="0.2.0",
    description="Native ncurses forms styled after Ghostcode Config Edit",
    packages=["tform"],
    package_data={"tform": ["py.typed", "*.pyi"]},
    ext_modules=[
        Extension(
            "tform._tform",
            sources=sources,
            include_dirs=["src"],
            libraries=["ncursesw", "panelw"],
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
