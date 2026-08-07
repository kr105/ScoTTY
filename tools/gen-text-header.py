#!/usr/bin/env python3
"""Empotra un fichero de texto en una cabecera C como literal de cadena.

Sustituye a dos reglas del MAKEFILE.MINGW que hacían esto con tuberías de
printf y sed, y que además escribían el resultado dentro del árbol de fuentes
(../../kitty_ini.h, ../../kitty_help.h). Aquí la salida va al directorio de
build, como cualquier otro artefacto generado.

Se usa para:

  kitty_ini.h   <- kitty_ini.txt
                   char default_init_file_content[]

  kitty_help.h  <- docs/pages/CommandLine.md, solo el tramo entre los
                   marcadores CmdLineOptions_begin y CmdLineOptions_end, sin
                   comentarios HTML ni los ** de negrita de Markdown
                   static char *default_help_file_content

A diferencia del sed original, aquí se escapan también las comillas dobles.
Ninguna de las dos entradas contiene comillas hoy, así que la salida es la
misma; es un seguro por si alguna las gana en el futuro, donde el sed original
habría generado C que no compila.
"""
import argparse
import re
import sys


def extract_between(lines, begin, end):
    out, inside = [], False
    for line in lines:
        if begin in line:
            inside = True
            continue
        if end in line:
            break
        if inside:
            out.append(line)
    if not out:
        sys.exit(f"error: no se encontró contenido entre {begin!r} y {end!r}")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output")
    ap.add_argument("--decl", required=True,
                    help='p.ej. "char default_init_file_content[]"')
    ap.add_argument("--eol", default="\\n",
                    help="terminador de línea dentro de la cadena C")
    ap.add_argument("--between", nargs=2, metavar=("BEGIN", "END"),
                    help="extraer solo el tramo entre estos dos marcadores")
    ap.add_argument("--strip-html-comments", action="store_true")
    ap.add_argument("--strip-markdown-bold", action="store_true")
    args = ap.parse_args()

    with open(args.input, "r", encoding="utf-8", errors="surrogateescape") as fh:
        lines = fh.read().splitlines()

    if args.between:
        lines = extract_between(lines, args.between[0], args.between[1])

    body = []
    for line in lines:
        if args.strip_html_comments:
            line = re.sub(r"<!--.*?-->", "", line)
        if args.strip_markdown_bold:
            line = line.replace("**", "")
        line = line.replace("\\", "\\\\").replace('"', '\\"')
        body.append(line + args.eol)

    with open(args.output, "w", encoding="utf-8", errors="surrogateescape") as fh:
        fh.write("/* Generado por tools/gen-text-header.py desde "
                 f"{args.input}. No editar. */\n")
        fh.write(f"{args.decl} =\n")
        for line in body:
            fh.write(f'\t"{line}"\n')
        fh.write("\t;\n")


if __name__ == "__main__":
    main()
