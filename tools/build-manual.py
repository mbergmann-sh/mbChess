#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds the mbChess manual from docs/manual/src/<lang>/*.md:

    docs/manual/mbChess-Handbuch.pdf / .chm   (German)
    docs/manual/mbChess-Manual.pdf   / .chm   (English)

Needs: pandoc (Markdown -> HTML), chmcmd (Free Pascal "fp-utils", builds the CHM on Linux;
on Windows hhc.exe of the HTML Help Workshop works too) and node with Playwright/Chromium for
the PDF. The version and its name come from common.pri.

    python3 tools/build-manual.py [--no-pdf] [--no-chm] [de] [en]
"""
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "manual", "src")
OUT = os.path.join(ROOT, "docs", "manual")

LANGS = {
    "de": dict(md="handbuch.md", name="mbChess-Handbuch", title="mbChess-Handbuch", lcid="0x407 German (Germany)",
               contents="Inhalt", page="Seite", version="Version"),
    "en": dict(md="manual.md", name="mbChess-Manual", title="mbChess Manual", lcid="0x409 English (United States)",
               contents="Contents", page="Page", version="Version"),
}


def version():
    text = open(os.path.join(ROOT, "common.pri"), encoding="utf-8").read()
    v = re.search(r"^MBCHESS_VERSION\s*=\s*(\S+)", text, re.M).group(1)
    c = re.search(r"^MBCHESS_CODENAME\s*=\s*(.+)$", text, re.M).group(1).strip()
    return v, c


def markdown(lang):
    v, c = version()
    text = open(os.path.join(SRC, lang, LANGS[lang]["md"]), encoding="utf-8").read()
    return text.replace("@VERSION@", v).replace("@CODENAME@", c)


def pandoc_html(md_text):
    # body only; the front matter (title, subtitle) is read here and not by pandoc
    meta = {}
    m = re.match(r"---\n(.*?)\n---\n", md_text, re.S)
    if m:
        for line in m.group(1).splitlines():
            k, _, val = line.partition(":")
            meta[k.strip()] = val.strip()
        md_text = md_text[m.end():]
    body = subprocess.run(["pandoc", "-f", "markdown-smart", "-t", "html5", "--wrap=none"], input=md_text,
                          capture_output=True, text=True, check=True).stdout
    return meta, body


def slug(text, used):
    s = re.sub(r"[^a-z0-9]+", "-", text.lower().translate(str.maketrans("äöüß", "aous"))).strip("-") or "page"
    base, n = s, 2
    while s in used:
        s, n = "%s-%d" % (base, n), n + 1
    used.add(s)
    return s


CSS = """
body { font-family: "Segoe UI", "Noto Sans", Arial, sans-serif; font-size: 10.5pt; line-height: 1.45;
       color: #1d1d1f; margin: 0 auto; max-width: 52em; padding: 0 1.5em 2em; }
h1 { font-size: 20pt; color: #2b4f81; border-bottom: 2px solid #2b4f81; padding-bottom: 4px; margin-top: 1.2em; }
h2 { font-size: 14pt; color: #2b4f81; margin-top: 1.4em; }
table { border-collapse: collapse; margin: 0.8em 0; width: 100%; }
th, td { border: 1px solid #c8ccd2; padding: 4px 8px; text-align: left; vertical-align: top; }
th { background: #e8edf4; }
code { font-family: Consolas, "DejaVu Sans Mono", monospace; font-size: 95%; background: #f1f3f5; padding: 0 3px; }
figure { margin: 1em 0; text-align: center; }
img { max-width: 100%; border: 1px solid #c8ccd2; }
figcaption { font-size: 9pt; color: #555; margin-top: 4px; }
.cover { text-align: center; padding-top: 6em; }
.cover h1 { border: none; font-size: 34pt; margin-bottom: 0.2em; }
.cover .sub { font-size: 15pt; color: #444; }
.cover img { border: none; width: 260px; margin: 2.5em 0; }
.cover .by { font-size: 11pt; color: #444; }
.toc a { text-decoration: none; color: #1d1d1f; }
.toc li { margin: 3px 0; }
.nav { font-size: 9pt; margin: 1em 0; color: #555; }
.nav a { color: #2b4f81; }
"""


def figures(body):
    # pandoc writes a lone image as <figure> with <figcaption> already; nothing to do then
    return body


def write_ascii(path, text):
    # the CHM viewer is old: plain ASCII with character references is safe in every code page
    open(path, "w", encoding="ascii", errors="xmlcharrefreplace").write(text)


def page(title, body, lang, css="style.css"):
    return ('<!DOCTYPE html>\n<html lang="%s"><head><meta charset="utf-8">'
            '<meta http-equiv="X-UA-Compatible" content="IE=edge">'
            '<title>%s</title><link rel="stylesheet" href="%s"></head><body>\n%s\n</body></html>\n'
            % (lang, html.escape(title), css, body))


def copy_images(lang, dest):
    src = os.path.join(SRC, lang, "images")
    os.makedirs(os.path.join(dest, "images"), exist_ok=True)
    for f in os.listdir(src):
        shutil.copy(os.path.join(src, f), os.path.join(dest, "images", f))
    shutil.copy(os.path.join(ROOT, "src", "app", "resources", "images", "about.jpg"),
                os.path.join(dest, "images", "about.jpg"))


def cover(meta):
    return ('<div class="cover"><h1>%s</h1><div class="sub">%s</div>'
            '<img src="images/about.jpg" alt=""><div class="by">%s<br>&copy; 2026</div></div>'
            % (html.escape(meta.get("title", "")), html.escape(meta.get("subtitle", "")),
               html.escape(meta.get("author", ""))))


def build_pdf(lang, meta, body, work):
    cfg = LANGS[lang]
    d = os.path.join(work, "pdf-" + lang)
    copy_images(lang, d)
    open(os.path.join(d, "style.css"), "w", encoding="utf-8").write(
        CSS + "\n@page { size: A4; margin: 18mm 16mm 18mm 16mm; }\n"
              "h1 { page-break-before: always; }\n.cover h1 { page-break-before: avoid; }\n"
              "figure, table, tr { page-break-inside: avoid; }\nbody { max-width: none; padding: 0; }\n")
    # table of contents from the chapters
    toc = ['<div style="page-break-before: always"></div><h2>%s</h2><ol class="toc">' % cfg["contents"]]
    used = set()

    def anchor(m):
        title = re.sub("<[^>]+>", "", m.group(2))
        s = slug(title, used)
        toc.append('<li><a href="#%s">%s</a></li>' % (s, title))
        return '<h1%s id="%s">%s</h1>' % (re.sub(r'\s*id="[^"]*"', "", m.group(1)), s, m.group(2))

    body2 = re.sub(r"<h1([^>]*)>(.*?)</h1>", anchor, body)
    toc.append("</ol>")
    html_file = os.path.join(d, "manual.html")
    open(html_file, "w", encoding="utf-8").write(page(meta.get("title", ""), cover(meta) + "".join(toc) + body2, lang))
    pdf = os.path.join(OUT, cfg["name"] + ".pdf")
    footer = ('<div style="font-size:8px;width:100%%;text-align:center;color:#777">%s &ndash; %s '
              '<span class="pageNumber"></span></div>' % (html.escape(meta.get("title", "")), cfg["page"]))
    script = """
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch();
  const p = await b.newPage();
  await p.goto('file://' + %s, { waitUntil: 'load' });
  await p.pdf({ path: %s, format: 'A4', printBackground: true, displayHeaderFooter: true,
                headerTemplate: '<div></div>', footerTemplate: %s,
                margin: { top: '16mm', bottom: '18mm', left: '16mm', right: '16mm' } });
  await b.close();
})();
""" % (json.dumps(html_file), json.dumps(pdf), json.dumps(footer))
    js = os.path.join(work, "pdf-%s.js" % lang)
    open(js, "w").write(script)
    subprocess.run(["node", js], check=True)
    print("PDF:", os.path.relpath(pdf, ROOT))


def build_chm(lang, meta, body, work):
    cfg = LANGS[lang]
    d = os.path.join(work, "chm-" + lang)
    copy_images(lang, d)
    open(os.path.join(d, "style.css"), "w", encoding="utf-8").write(CSS)
    # one page per chapter (h1); h2 sections become anchors in the contents
    parts = re.split(r"(?=<h1[ >])", body)
    chapters = []
    used = set()
    for part in parts:
        m = re.match(r"<h1[^>]*>(.*?)</h1>", part, re.S)
        if not m:
            continue
        title = html.unescape(re.sub("<[^>]+>", "", m.group(1)))
        name = slug(title, used) + ".html"
        subs = []
        sub_used = set()

        def h2(mm):
            t = html.unescape(re.sub("<[^>]+>", "", mm.group(2)))
            s = slug(t, sub_used)
            subs.append((t, s))
            return '<h2 id="%s">%s</h2>' % (s, mm.group(2))

        part = re.sub(r"<h2([^>]*)>(.*?)</h2>", h2, part, flags=re.S)
        chapters.append(dict(title=title, file=name, body=part, subs=subs))
    # start page: cover and contents
    items = "".join('<li><a href="%s">%s</a></li>' % (c["file"], html.escape(c["title"])) for c in chapters)
    write_ascii(os.path.join(d, "index.html"),
                page(meta.get("title", ""), cover(meta) + '<h2>%s</h2><ol class="toc">%s</ol>' % (cfg["contents"], items), lang))
    for i, c in enumerate(chapters):
        prev = chapters[i - 1]["file"] if i > 0 else "index.html"
        nxt = chapters[i + 1]["file"] if i + 1 < len(chapters) else None
        nav = '<div class="nav"><a href="%s">&laquo;</a> | <a href="index.html">%s</a>%s</div>' % (
            prev, cfg["contents"], (' | <a href="%s">&raquo;</a>' % nxt) if nxt else "")
        write_ascii(os.path.join(d, c["file"]), page(c["title"], nav + c["body"] + nav, lang))

    def obj(name, local):
        return ('<LI> <OBJECT type="text/sitemap">\n<param name="Name" value="%s">\n'
                '<param name="Local" value="%s">\n</OBJECT>\n' % (html.escape(name), local))

    hhc = ['<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">\n<HTML><HEAD></HEAD><BODY>\n'
           '<OBJECT type="text/site properties"><param name="ImageType" value="Folder"></OBJECT>\n<UL>\n']
    hhc.append(obj(cfg["contents"], "index.html"))
    keywords = []
    for c in chapters:
        hhc.append(obj(c["title"], c["file"]))
        keywords.append((c["title"], c["file"]))
        if c["subs"]:
            hhc.append("<UL>\n")
            for t, s in c["subs"]:
                hhc.append(obj(t, "%s#%s" % (c["file"], s)))
                keywords.append((t, "%s#%s" % (c["file"], s)))
            hhc.append("</UL>\n")
    hhc.append("</UL>\n</BODY></HTML>\n")
    open(os.path.join(d, "toc.hhc"), "w", encoding="cp1252", errors="replace").write("".join(hhc))
    hhk = ['<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">\n<HTML><HEAD></HEAD><BODY>\n<UL>\n']
    for t, local in sorted(keywords, key=lambda k: k[0].lower()):
        hhk.append(obj(t, local))
    hhk.append("</UL>\n</BODY></HTML>\n")
    open(os.path.join(d, "index.hhk"), "w", encoding="cp1252", errors="replace").write("".join(hhk))

    files = ["index.html", "style.css"] + [c["file"] for c in chapters] + \
        ["images/" + f for f in sorted(os.listdir(os.path.join(d, "images")))]
    chm_name = cfg["name"] + ".chm"
    title = "%s %s" % (cfg["title"], version()[0])
    hhp = ("[OPTIONS]\nCompatibility=1.1 or later\nCompiled file=%s\nContents file=toc.hhc\n"
           "Index file=index.hhk\nDefault Window=main\nDefault topic=index.html\nDisplay compile progress=No\n"
           "Full-text search=Yes\nLanguage=%s\nTitle=%s\n\n"
           "[WINDOWS]\nmain=\"%s\",\"toc.hhc\",\"index.hhk\",\"index.html\",\"index.html\",,,,,0x63520,240,0x10184e,"
           "[60,40,1100,800],,,,,,,0\n\n[FILES]\n%s\n"
           % (chm_name, cfg["lcid"], title, title, "\n".join(files)))
    open(os.path.join(d, "manual.hhp"), "w", encoding="cp1252", errors="replace").write(hhp)
    r = subprocess.run(["chmcmd", "manual.hhp"], cwd=d, capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(os.path.join(d, chm_name)):
        sys.exit("chmcmd failed:\n" + r.stdout + r.stderr)
    shutil.copy(os.path.join(d, chm_name), os.path.join(OUT, chm_name))
    print("CHM:", os.path.relpath(os.path.join(OUT, chm_name), ROOT))


def main():
    args = sys.argv[1:]
    do_pdf = "--no-pdf" not in args
    do_chm = "--no-chm" not in args
    langs = [a for a in args if a in LANGS] or list(LANGS)
    with tempfile.TemporaryDirectory() as work:
        for lang in langs:
            meta, body = pandoc_html(markdown(lang))
            body = figures(body)
            if do_chm:
                build_chm(lang, meta, body, work)
            if do_pdf:
                build_pdf(lang, meta, body, work)


if __name__ == "__main__":
    main()
