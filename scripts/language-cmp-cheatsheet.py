import argparse
import os
import string
import dataclasses

@dataclasses.dataclass
class Section:
    name: str
    rows: list = dataclasses.field(default_factory=lambda: [{}])
    order: list = dataclasses.field(default_factory=list)
    ref: str = ""

@dataclasses.dataclass
class Directive:
    line_number: int
    type: str
    content: str

HTML_TEMPLATE = string.Template("""<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8" />
        <title>$title</title>
        <style>$css</style>
    </head>
    <body>
        <p>$intro</p>
        <nav> <ol> $nav </ol> </nav>
        <section> $body </section>
    </body>
</html>
""")

def parse_table(lines: list[str]) -> list[Section]:
    previus_column_type = None
    sections = []
    current_section = None

    # Each nonblank matches this regular expression /(\S*)\s(.*)/
    # where first capture is type of column (essentialy column id) and
    # second capture is given cell content. Where column type repeats not in row
    # we have new row.
    for line in lines:
        if not line:
            continue

        sep = line.find(' ')
        column_type, cell_content = line[:sep].strip(), line[sep:]

        if column_type == "SECTION":
            sections.append(Section(name=cell_content))
            current_section = sections[-1]
            continue

        assert current_section is not None, "Define SECTION before table entries"

        if column_type not in current_section.order:
            current_section.order.append(column_type)

        if previus_column_type != column_type and column_type in current_section.rows[-1]:
            current_section.rows.append({})

        cell = current_section.rows[-1].get(column_type, [])
        cell.append(cell_content)
        current_section.rows[-1][column_type] = cell
        previus_column_type = column_type

    # Eliminate common whitespace prefix in given column type (not all whitespace
    # prefix since examples in Python may have significant whitespace)
    for section in sections:
        for row in section.rows:
            for cell in row.values():
                prefix_whitespace = min(len(s) - len(s.lstrip()) for s in cell)
                for i, s in enumerate(cell):
                    cell[i] = s[prefix_whitespace:]

    return sections

def compile_template(*, template_path: str, target_path: str):
    # Read template file and separate it into lines
    with open(template_path) as f:
        template = [line.strip() for line in f.readlines()]

    directives = []
    for i, line in enumerate(template):
        s = line.split()
        if s and s[0] in ("BEGIN", "END", "TITLE"):
            directives.append(Directive(i, s[0], line[len(s[0]):].strip()))

    title, css_source, table_source, intro_source = 4 * [None]

    for directive in directives:
        if directive.type == "TITLE":
            title = directive.content
            continue

        if directive.type == "BEGIN":
            start_line = directive.line_number
            for end in directives:
                if end.type == "END" and end.content == directive.content:
                    end_line = end.line_number
                    break
            else:
                assert False, "Begin without matching end"

            span = template[start_line+1:end_line]

            if directive.content == "CSS":
                css_source = span
            elif directive.content == "TABLE":
                table_source = span
            elif directive.content == "INTRO":
                intro_source = span

    assert css_source is not None
    assert table_source is not None
    assert intro_source is not None
    assert title is not None

    sections = parse_table(lines=table_source)

    for section in sections:
        section.ref = section.name.replace(" ", "_")

    nav = ""
    for section in sections:
        nav += f"<li><a href=\"#{section.ref}\">{section.name}</a></li>"

    body = ""
    for section in sections:
        table = f"<p><h2 id=\"{section.ref}\">{section.name}</h2><table>\n"

        for i, row in enumerate(section.rows):
            if i == 0:
                line = ["<th>" + ' '.join(row[k]) + "</th>" for k in section.order]
            else:
                line = []
                for column in section.order:
                    val = row.get(column, [])
                    line.append('<td><pre><code>' + '<br/>'.join(val) + '</code></pre></td>')

            table += "<tr>\n" + '\n'.join(line) + "\n</tr>\n"

        table += "\n</table></p>\n"
        body += table


    final = HTML_TEMPLATE.substitute({
        "body": body,
        "css": "\n".join(css_source),
        "intro": "\n".join(intro_source),
        "nav": nav,
        "title": title,
    })

    with open(target_path, "w") as f:
        f.write(final)


def main():
    parser = argparse.ArgumentParser(description="Build language comparison chart")
    parser.add_argument(nargs='+', metavar="TEMPLATE", dest="templates", help="Template file that will be converted to HTML page")

    args = parser.parse_args()

    for template in args.templates:
        dst, _ = os.path.splitext(template)
        dst += ".html"
        compile_template(template_path=template, target_path=dst)

if __name__ == "__main__":
    main()
