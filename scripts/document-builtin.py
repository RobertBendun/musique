#!/usr/bin/env python3
import argparse
import dataclasses
import string
import re
import itertools
import typing
import subprocess

MARKDOWN_CONVERTER = "lowdown -m 'shiftheadinglevelby=3'"
CPP_FUNC_IDENT_ALLOWLIST = string.ascii_letters + string.digits + "_"
PROGRAM_NAME: str = ""

HTML_PREFIX = """<html>
    <head>
        <meta charset="utf-8">
        <title>Dokumentacja funkcji języka Musique</title>
        <style>
            html {
                color: #1a1a1a;
                background-color: #fdfdfd;
            }

            body {
                font-family: sans-serif;
                margin: 10px auto;
                overflow-wrap: break-word;
                text-rendering: optimizeLegibility;
                font-kerning: normal;
                max-width: 50em;
            }

            nav a, nav a:link, nav a:visited {
                color: #07a;
                text-decoration: none;
            }

            h2 > a, h2>a:visited, h2>a:link {
                color: gray;
                text-decoration: none;
                padding-right: 5px;
            }
            h2 > a:hover { color: lightgrey; }

            footer {
                width: 100%;
                border-top: 1px solid black;
            }
        </style>
    </head>
    <body>
    <h1>Dokumentacja funkcji języka Musique</h1>
"""

HTML_SUFFIX = """
    <footer>
        Wszystkie treści podlegają licencji <a href="https://creativecommons.org/licenses/by-sa/4.0/">CC BY-SA 4.0</a>
    </footer>
    </body>
</html>
"""


def warning(*args, prefix: str | None = None):
    if prefix is None:
        prefix = PROGRAM_NAME
    message = ": ".join(itertools.chain([prefix, "warning"], args))
    print(message)


def error(*args, prefix=None):
    if prefix is None:
        prefix = PROGRAM_NAME
    message = ": ".join(itertools.chain([prefix, "error"], args))
    print(message)
    exit(1)


@dataclasses.dataclass
class Builtin:
    implementation: str
    definition_location: tuple[str, int]  # Filename and line number
    names: list[str]
    documentation: str


def builtins_from_file(source_path: str) -> typing.Generator[Builtin, None, None]:
    with open(source_path) as f:
        source = f.readlines()

    builtins: dict[str, Builtin] = {}
    definition = re.compile(
        r"""force_define.*\("([^"]+)"\s*,\s*(builtin_[a-zA-Z0-9_]+)\)"""
    )

    current_documentation = []

    for lineno, line in enumerate(source):
        line = line.strip()

        # Check if line contains force_define with static string and builtin_*
        # thats beeing defined. It's a one of many names that given builtin
        # has in Musique
        if result := definition.search(line):
            musique_name = result.group(1)
            builtin_name = result.group(2)

            if builtin_name in builtins:
                builtins[builtin_name].names.append(musique_name)
            else:
                error(
                    f"tried adding Musique name '{musique_name}' to builtin '{builtin_name}' that has not been defined yet",
                    prefix=f"{source_path}:{lineno}",
                )
            continue

        # Check if line contains special documentation comment.
        # We assume that only documentation comments are in given line (modulo whitespace)
        if line.startswith("//:"):
            line = line.removeprefix("//:").strip()
            current_documentation.append(line)
            continue

        # Check if line contains builtin_* identifier.
        # If contains then this must be first definition of this function
        # and therefore all documentation comments before it describe it
        if (index := line.find("builtin_")) >= 0 and line[
            index - 1
        ] not in CPP_FUNC_IDENT_ALLOWLIST:
            identifier = line[index:]
            for i, char in enumerate(identifier):
                if char not in CPP_FUNC_IDENT_ALLOWLIST:
                    identifier = identifier[:i]
                    break

            if identifier not in builtins:
                builtin = Builtin(
                    implementation=identifier,
                    # TODO Allow redefinition of source path with some prefix
                    # to allow website links
                    definition_location=(source_path, lineno),
                    names=[],
                    documentation="\n".join(current_documentation),
                )
                builtins[identifier] = builtin
                current_documentation = []
            continue

    for builtin in builtins.values():
        builtin.names.sort()
        yield builtin


def filter_builtins(builtins: list[Builtin]) -> typing.Generator[Builtin, None, None]:
    for builtin in builtins:
        if not builtin.documentation:
            warning(f"builtin '{builtin.implementation}' doesn't have documentation")
            continue

        # Testt if builtin is unused
        if not builtin.names:
            continue

        yield builtin


def each_musique_name_occurs_once(
    builtins: typing.Iterable[Builtin],
) -> typing.Generator[Builtin, None, None]:
    names = {}
    for builtin in builtins:
        for name in builtin.names:
            if name in names:
                error(
                    f"'{name}' has been registered as both '{builtin.implementation}' and '{names[name]}'"
                )
            names[name] = builtin.implementation
        yield builtin


def generate_html_document(builtins: list[Builtin], output_path: str):
    with open(output_path, "w") as out:
        out.write(HTML_PREFIX)

        out.write("<nav>")

        names = sorted(
            [
                (name, builtin.implementation)
                for builtin in builtins
                for name in builtin.names
            ]
        )

        out.write(", ".join(f"""<a href="#{id}">{name}</a>""" for name, id in names))

        out.write("</nav>")

        out.write("<main>")

        for builtin in builtins:
            out.write("<p>")
            out.write(
                f"""<h2 id="{builtin.implementation}"><a href="#{builtin.implementation}">&sect;</a>{', '.join(builtin.names)}</h2>"""
            )
            out.write(
                subprocess.check_output(
                    MARKDOWN_CONVERTER,
                    input=builtin.documentation,
                    encoding="utf-8",
                    shell=True,
                )
            )
            out.write("</p>")

        out.write("</main>")

        out.write(HTML_SUFFIX)


def main(source_path: str, output_path: str):
    "Generates documentaiton from file source_path and saves in output_path"

    builtins = builtins_from_file(source_path)
    builtins = filter_builtins(builtins)
    builtins = each_musique_name_occurs_once(builtins)
    builtins = sorted(list(builtins), key=lambda builtin: builtin.names[0])

    generate_html_document(builtins, output_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Builtin functions documentation generator from C++ Musique implementation"
    )
    parser.add_argument(
        "source",
        type=str,
        nargs=1,
        help="C++ source file from which documentation will be generated",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        nargs=1,
        required=True,
        help="path for standalone HTML file containing generated documentation",
    )

    PROGRAM_NAME = parser.prog
    args = parser.parse_args()
    assert len(args.source) == 1
    assert len(args.output) == 1

    main(source_path=args.source[0], output_path=args.output[0])
