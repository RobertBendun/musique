import argparse
import os
import string
import collections

Directive = collections.namedtuple("Directive", "line_number type content")

HTML_TEMPLATE = string.Template("""<!DOCTYPE html>
<html>
    <head>
        <meta charset="utf-8" />
        <title>$title</title>
        <style>$css</style>
    </head>
    <body>
        <p>$intro</p>
        <table> $table </table>
    </body>
</html>
""")

def parse_table(lines: list):
    previus_column_type = None
    rows, order = [{}], []
    current_row = rows[0]

    # Each nonblank matches this regular expression /(\S*)\s(.*)/
    # where first capture is type of column (essentialy column id) and
    # second capture is given cell content. Where column type repeats not in row
    # we have new row.
    for line in lines:
        if not line:
            continue

        sep = line.find(' ')
        column_type, cell_content = line[:sep].strip(), line[sep:]

        if column_type not in order:
            order.append(column_type)

        if previus_column_type != column_type and column_type in current_row:
            rows.append({})
            current_row = rows[-1]

        cell = current_row.get(column_type, [])
        cell.append(cell_content)
        current_row[column_type] = cell
        previus_column_type = column_type

    # Eliminate common whitespace prefix in given column type (not all whitespace
    # prefix since examples in Python may have significant whitespace)
    for row in rows:
        for cell in row.values():
            prefix_whitespace = min(len(s) - len(s.lstrip()) for s in cell)
            for i, s in enumerate(cell):
                cell[i] = s[prefix_whitespace:]

    return rows, order

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

    rows, columns_order = parse_table(lines=table_source)

    table = ""

    for i, row in enumerate(rows):
        if i == 0:
            line = ["<th>" + ' '.join(row[k]) + "</th>" for k in columns_order]
        else:
            line = []
            for column in columns_order:
                val = row.get(column, [])
                line.append('<td><pre><code>' + '<br/>'.join(val) + '</code></pre></td>')

        table += "<tr>\n" + '\n'.join(line) + "\n</tr>\n"


    final = HTML_TEMPLATE.substitute({
        "title": title,
        "css": "\n".join(css_source),
        "table": table,
        "intro": "\n".join(intro_source)
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
