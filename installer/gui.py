"""The installer window: python -m installer.gui

A front end over the CLI (docs/installer.md): it collects the game files, the keys file path, the game language and the
install folder, runs `python -m installer ...` as a subprocess and shows its JSON events. Decisions live in guilogic.py;
this file only draws. The window is in English only; its text is in installer/text/en.json under "ui.*" and
"launcher.*" ids.

    --selftest [--shots DIR]   build every screen and close by itself (PNG per screen with --shots)
"""
import argparse
import json
import os
import subprocess
import sys
import threading
import tkinter as tk
import webbrowser
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

try:
    from PIL import ImageGrab   # only the screenshots of --selftest --shots need it; the release has no PIL
except ImportError:
    ImageGrab = None

from . import ini
from . import layout as layouts
from . import update
from .guilogic import (DEFAULT_GAME_LANGUAGE, DKB_STEP_ORDER, NUP_GAMES, DONE, FAILED, PENDING, RUNNING, SKIPPED, Catalog,
                       Form, Model, Process, amount, describe_event, parse_line)

POLL_MS = 100
GLYPH = {PENDING: '\u25cb', RUNNING: '\u25b6', DONE: '\u2714', SKIPPED: '\u2013', FAILED: '\u2716'}
# One look per game, drawn with our own colours only. Keys: bg (window), frame (header strip, buttons), border, field
# (entries, log), hover, accent (titles, focus, progress, the main button), accent_hi (main button hovered), text, muted,
# disabled (text of disabled buttons), stripe (the 4 px line under the header), err, ok and warn (status text).
# Every text/background pair is >= 4.5:1 (tests/installer_gui_test.py ThemeTest checks it).
THEMES = {
    # The in-game menu's Donkey Kong look (runtime/ui/menu.cpp style_dk). The menu's red and green are accents and
    # fills; err and ok are their lighter tints, because the originals reach only 3.0:1 and 4.2:1 on the bark.
    'dk': dict(brand='DONKEY KONG BANANZA  ·  PC RECOMP', art='bananas',
               bg='#2a1a0e', frame='#3b2614', border='#6b4a2a', field='#1d1109', hover='#4d331b',
               accent='#f6c52b', accent_hi='#ffd95a', text='#f3e9d2', muted='#b8a888', disabled='#9c8a70',
               stripe='#c8281e', err='#ff8a7a', ok='#6fcf68', warn='#f6c52b'),
    # The in-game menu's Splatoon Raiders look (runtime/ui/menu.cpp style_raiders): deep night blue, cyan ink, coral.
    'raiders': dict(brand='SPLATOON RAIDERS  ·  PC RECOMP', art='splat',
                    bg='#0a161f', frame='#10262f', border='#245964', field='#06111a', hover='#1a4f55',
                    accent='#31e1cf', accent_hi='#7cf0e4', text='#ebf9f7', muted='#9ebebe', disabled='#6f9499',
                    stripe='#ff7057', err='#ff8a73', ok='#8be06a', warn='#ffd34d'),
}
PAD = 14


def theme_for(game):
    return THEMES['raiders' if game == 'raiders' else 'dk']


def status_colors(t):
    return {PENDING: t['muted'], RUNNING: t['accent'], DONE: t['ok'], SKIPPED: t['muted'], FAILED: t['err']}


def apply_theme(root, ui_font, t):
    """A ttk theme on top of clam (portable: clam ships with every Tk), coloured by t (THEMES). Falls back to the
    platform theme if clam is missing."""
    style = ttk.Style(root)
    try:
        style.theme_create('setup', parent='clam')
        style.theme_use('setup')
    except tk.TclError:
        pass
    bg, frame, border, field, hover = t['bg'], t['frame'], t['border'], t['field'], t['hover']
    accent, text, disabled = t['accent'], t['text'], t['disabled']
    root.configure(background=bg)
    root.option_add('*TCombobox*Listbox.background', field)
    root.option_add('*TCombobox*Listbox.foreground', text)
    root.option_add('*TCombobox*Listbox.selectBackground', accent)
    root.option_add('*TCombobox*Listbox.selectForeground', bg)
    font = (ui_font, 10)
    style.configure('.', background=bg, foreground=text, font=font, bordercolor=border, lightcolor=border,
                    darkcolor=border, troughcolor=field, focuscolor=accent, insertcolor=text)
    style.configure('TButton', background=frame, foreground=text, relief='flat', borderwidth=2, padding=(14, 5),
                    bordercolor=border, lightcolor=border, darkcolor=border)
    style.map('TButton', background=[('disabled', bg), ('pressed', accent), ('active', hover)],
              foreground=[('disabled', disabled), ('pressed', bg)], bordercolor=[('focus', accent)])
    style.configure('Accent.TButton', background=accent, foreground=bg, font=(ui_font, 10, 'bold'),
                    bordercolor=accent, lightcolor=accent, darkcolor=accent)
    style.map('Accent.TButton', background=[('disabled', bg), ('pressed', text), ('active', t['accent_hi'])],
              foreground=[('disabled', disabled)])
    for name in ('TEntry', 'TCombobox'):
        style.configure(name, fieldbackground=field, foreground=text, insertcolor=text, bordercolor=border,
                        lightcolor=border, darkcolor=border, arrowcolor=accent, padding=3)
    style.map('TEntry', bordercolor=[('focus', accent)], lightcolor=[('focus', accent)], darkcolor=[('focus', accent)])
    style.map('TCombobox', fieldbackground=[('readonly', field)], foreground=[('readonly', text)],
              selectbackground=[('readonly', field)], selectforeground=[('readonly', text)],
              bordercolor=[('focus', accent)], background=[('active', hover)])
    style.configure('Horizontal.TProgressbar', background=accent, troughcolor=field, bordercolor=border,
                    lightcolor=accent, darkcolor=accent, thickness=14)
    style.configure('TScrollbar', background=frame, troughcolor=field, arrowcolor=accent, bordercolor=border)
    style.configure('Header.TFrame', background=frame)
    style.configure('Brand.TLabel', background=frame, foreground=accent, font=(ui_font, 14, 'bold'))
    style.configure('Title.TLabel', foreground=accent, font=(ui_font, 15, 'bold'))
    style.configure('Sub.TLabel', background=frame, foreground=text, font=(ui_font, 10))
    style.configure('Notice.TLabel', background=frame, foreground=text, padding=8, borderwidth=2, relief='solid',
                    bordercolor=accent)
    style.configure('Err.TLabel', foreground=t['err'])
    style.configure('Warn.TLabel', foreground=t['warn'])
    style.configure('Version.TLabel', background=frame, foreground=t['muted'])
    return style


def splat_outline(cx, cy, r, steps=48):
    """An ink splat's outline: a circle whose radius ripples with a few sines, so it bulges into rounded lobes."""
    import math
    pts = []
    for i in range(steps):
        a = 2 * math.pi * i / steps
        k = 1 + 0.16 * math.sin(5 * a + 0.4) + 0.07 * math.sin(9 * a + 1.3) + 0.04 * math.sin(13 * a)
        pts.append((cx + r * k * math.cos(a), cy + r * k * math.sin(a)))
    return pts


# The splat's droplets around the blob, as (dx, dy, radius) in units of the blob radius, and which colour each takes.
SPLAT_DROPS = ((1.32, -0.78, 0.22, 'accent'), (-1.28, 0.70, 0.17, 'accent'), (1.05, 1.02, 0.14, 'stripe'),
               (-0.95, -1.10, 0.11, 'stripe'))


def draw_splat(canvas, cx, cy, r, t):
    """A cyan ink splat with two cyan and two coral droplets, centred at (cx, cy) with blob radius r."""
    canvas.create_polygon([c for p in splat_outline(cx, cy, r) for c in p], fill=t['accent'], outline='', smooth=True)
    for dx, dy, dr, key in SPLAT_DROPS:
        x, y, rr = cx + dx * r, cy + dy * r, dr * r
        canvas.create_oval(x - rr, y - rr, x + rr, y + rr, fill=t[key], outline='')
    # A highlight: a small light oval on the upper left of the blob, as on wet ink.
    canvas.create_oval(cx - 0.55 * r, cy - 0.55 * r, cx - 0.15 * r, cy - 0.25 * r, fill=t['accent_hi'], outline='')


def draw_bananas(canvas, x, y, s):
    """A small banana bunch: three crescents (polygons between two arcs) fanning from a brown stem at (x, y)."""
    import math
    shades = (('#dea51e', -0.5), ('#f6c52b', 0.5), ('#ffd650', 0.0))   # the middle one is drawn last, on top
    for fill, rot in shades:
        cr, sr = math.cos(rot), math.sin(rot)
        outer, inner = [], []
        for i in range(15):
            t = i / 14
            a = math.pi - t * 1.45
            w = 0.34 * s * (0.2 + 0.8 * math.sin(math.pi * (0.1 + 0.8 * t)))
            px, py = s * 0.95 * (1 + math.cos(a)), s * 0.95 * math.sin(a)
            for pts, k in ((outer, 0.5), (inner, -0.5)):
                ox, oy = px + math.cos(a) * w * k, py + math.sin(a) * w * k
                pts.append((x + ox * cr - oy * sr, y + ox * sr + oy * cr))
        canvas.create_polygon([c for p in outer + inner[::-1] for c in p], fill=fill, outline='#8a5a1a')
        tx, ty = (outer[-1][0] + inner[-1][0]) / 2, (outer[-1][1] + inner[-1][1]) / 2
        canvas.create_oval(tx - 2, ty - 2, tx + 2, ty + 2, fill='#4a2f18', outline='')
    canvas.create_rectangle(x - 4, y - 6, x + 4, y + 2, fill='#5c3a1a', outline='')


def fix_tcl_paths():
    """The embeddable Python keeps Tcl/Tk next to python.exe (python/tcl); point Tcl at it when it is not found."""
    tcl = Path(sys.executable).resolve().parent / 'tcl'
    if tcl.is_dir() and 'TCL_LIBRARY' not in os.environ:
        for lib, var in (('tcl9.0', 'TCL_LIBRARY'), ('tk9.0', 'TK_LIBRARY')):
            if (tcl / lib).is_dir():
                os.environ[var] = str(tcl / lib)


def fix_dpi_awareness():
    """Use physical window coordinates before Tk starts, so its geometry agrees with screenshot coordinates."""
    if os.name != 'nt':
        return
    import ctypes
    set_context = getattr(ctypes.windll.user32, 'SetProcessDpiAwarenessContext', None)
    if set_context:
        set_context.argtypes = [ctypes.c_void_p]
        set_context.restype = ctypes.c_int
        set_context(ctypes.c_void_p(-4))   # PER_MONITOR_AWARE_V2; an already configured process keeps its context


class App:
    def __init__(self, root, layout, run_args=None, game='totk', check_updates=False):
        self.root, self.layout = root, layout
        self.game = game
        self.cat = Catalog(game)
        self.model = Model(DKB_STEP_ORDER if game in NUP_GAMES else None)
        self.process = None
        self.closing = False
        self.run_args = run_args or {}   # extra CLI options for tests: temp, verify
        self.log_entries = []            # events and raw lines of the run, rendered by render_log
        self.vars = {n: tk.StringVar() for n in ('base', 'update', 'keys', 'nup', 'exefs', 'romfs', 'package', 'dest')}
        self.vars['lang'] = tk.StringVar()
        self.lang_code = DEFAULT_GAME_LANGUAGE
        self.screen = None
        self.fitted = None               # the (width, height) fit last gave the window

        self.t = theme_for(game)
        self.status_color = status_colors(self.t)
        root.title(self.cat('ui.title'))
        icon = layout.runtime / ('spr.ico' if game == 'raiders' else 'dkb.ico')
        png = layout.runtime / 'totk.png'   # Tk loads no .ico off Windows; the Linux release converts it
        if os.name == 'nt' and icon.is_file():
            root.iconbitmap(default=str(icon))
        elif png.is_file():
            self.icon = tk.PhotoImage(file=str(png))
            root.iconphoto(True, self.icon)
        root.protocol('WM_DELETE_WINDOW', self.on_close)
        self.ui_font = 'Segoe UI' if os.name == 'nt' else 'DejaVu Sans'
        self.mono_font = 'Consolas' if os.name == 'nt' else 'DejaVu Sans Mono'
        apply_theme(root, self.ui_font, self.t)

        top = ttk.Frame(root, style='Header.TFrame', padding=(PAD, 10, PAD, 10))
        top.pack(fill='x')
        art = tk.Canvas(top, width=46, height=42, bg=self.t['frame'], highlightthickness=0)
        if self.t['art'] == 'splat':
            draw_splat(art, 22, 21, 11, self.t)
        else:
            draw_bananas(art, 12, 16, 17)
        art.pack(side='left', padx=(0, 10))
        names = ttk.Frame(top, style='Header.TFrame')
        names.pack(side='left')
        ttk.Label(names, text=self.t['brand'], style='Brand.TLabel').pack(anchor='w')
        self.title = self.label(names, 'ui.title', style='Sub.TLabel')
        self.title.pack(anchor='w')
        version = ttk.Label(top, style='Version.TLabel')
        self.bind_text(version, 'text', 'ui.version', version=layout.release)
        version.pack(side='left', padx=(10, 0), pady=(6, 0))
        tk.Frame(root, height=4, bg=self.t['stripe']).pack(fill='x')   # DK's tie red, or Raiders' coral ink
        self.body = ttk.Frame(root, padding=PAD)
        self.body.pack(fill='both', expand=True)
        self.frames = {'form': self.build_form(), 'run': self.build_run(), 'finish': self.build_finish()}
        self.vars['lang'].set(self.lang_label(self.lang_code))
        self.vars['dest'].trace_add('write', lambda *_: self.refresh_form())
        self.show('form')
        if check_updates:
            self.update_label = ttk.Label(self.body, style='Warn.TLabel', cursor='hand2')
            threading.Thread(target=self.check_update, daemon=True).start()

    def check_update(self):
        """Background lookup of a newer release; the label appears only when there is one (never blocks the window)."""
        found = update.newer_release(self.layout.release)
        if found and not self.closing:
            try:
                self.root.after(0, self.show_update, *found)
            except RuntimeError:   # Tk is already gone
                pass

    def show_update(self, tag, url):
        self.bind_text(self.update_label, 'text', 'ui.update_available', tag=tag)
        self.update_label.bind('<Button-1>', lambda _e: webbrowser.open(url))
        self.update_label.pack(before=self.body.winfo_children()[0], anchor='w', pady=(0, 8))
        self.fit()

    # ---- text -----------------------------------------------------------------------------------------------------

    def bind_text(self, widget, option, id, **params):
        widget.configure(**{option: self.cat(id, **params)})

    def label(self, parent, id, **kw):
        w = ttk.Label(parent, **kw)
        self.bind_text(w, 'text', id)
        return w

    def button(self, parent, id, command, **kw):
        w = ttk.Button(parent, command=command, **kw)
        self.bind_text(w, 'text', id)
        return w

    def lang_label(self, code):
        return self.cat('lang.' + code)

    def fill_languages(self):
        code = self.selected_lang()
        self.lang_box.configure(values=[self.lang_label(c) for c in ini.LANGUAGES])
        self.vars['lang'].set(self.lang_label(code))

    def selected_lang(self):
        """The game language code behind the combobox text (kept when it is not one of the labels)."""
        shown = self.vars['lang'].get()
        for code in ini.LANGUAGES:
            if shown == self.cat('lang.' + code):
                return code
        return self.lang_code

    # ---- screens --------------------------------------------------------------------------------------------------

    def show(self, name):
        for f in self.frames.values():
            f.pack_forget()
        self.frames[name].pack(fill='both', expand=True)
        self.screen = name
        self.fit()

    def fit(self):
        """Size the window to its largest screen (an unpacked frame still reports the size its children ask for), so
        a screen or language switch neither cuts text nor moves the buttons. The window only grows: opening the log
        makes it taller, and a size the user dragged it to stays. Every text change that can add lines calls it again,
        or pack takes the missing height from the button bars."""
        self.root.update_idletasks()
        sizes = [(f.winfo_reqwidth(), f.winfo_reqheight()) for f in self.frames.values()]
        chrome = self.root.winfo_reqheight() - self.body.winfo_reqheight()   # the title row above the body
        w = max(self.root.winfo_reqwidth(), max(s[0] for s in sizes) + 2 * PAD)
        h = chrome + max(s[1] for s in sizes) + 2 * PAD
        if (w, h) == self.fitted:   # refresh_run calls it on every poll: leave the window alone while nothing grew
            return
        self.fitted = (w, h)
        self.root.minsize(w, h)
        self.root.geometry(f'{max(w, self.root.winfo_width())}x{max(h, self.root.winfo_height())}')

    def notice(self, parent):
        w = ttk.Label(parent, style='Notice.TLabel', wraplength=620, justify='left')
        self.bind_text(w, 'text', 'ui.notice')
        return w

    def build_form(self):
        f = ttk.Frame(self.body)
        bar = ttk.Frame(f)   # packed first so pack shrinks the text above it, never the buttons
        bar.pack(side='bottom', fill='x', pady=(12, 0))
        self.start_button = self.button(bar, 'ui.start', self.start, style='Accent.TButton')
        self.start_button.pack(side='right')
        self.form_intro = self.label(f, 'ui.intro', wraplength=620, justify='left')
        self.form_intro.pack(anchor='w', pady=(0, 8))
        self.notice(f).pack(fill='x', pady=(0, 10))
        grid = ttk.Frame(f)
        grid.pack(fill='x')
        grid.columnconfigure(1, weight=1)
        rows = (('base', 'ui.base', 'ui.pick_game'), ('update', 'ui.update', 'ui.pick_update'),
                ('keys', 'ui.keys', 'ui.pick_keys'))
        if self.game in NUP_GAMES:
            rows = (('nup', 'dkb.ui.nup', 'dkb.ui.pick_nup'), ('exefs', 'dkb.ui.exefs', 'dkb.ui.pick_exefs'),
                    ('romfs', 'dkb.ui.romfs', 'dkb.ui.pick_romfs'), ('package', 'dkb.ui.package', 'dkb.ui.pick_package'))
        self.form_rows = {}
        for i, (name, label_id, pick_id) in enumerate(rows):
            lab = self.label(grid, label_id)
            lab.grid(row=i, column=0, sticky='w', pady=4, padx=(0, 10))
            ttk.Entry(grid, textvariable=self.vars[name]).grid(row=i, column=1, sticky='ew', pady=4)
            b = self.button(grid, 'ui.browse', lambda n=name, p=pick_id: self.pick_file(n, p))
            b.grid(row=i, column=2, padx=(8, 0))
            self.form_rows[name] = lab
        i = len(rows)
        self.label(grid, 'ui.game_language').grid(row=i, column=0, sticky='w', pady=4)
        self.lang_box = ttk.Combobox(grid, textvariable=self.vars['lang'], state='readonly', width=30)
        self.lang_box.grid(row=i, column=1, sticky='w', pady=4)
        self.lang_touched = False
        self.lang_box.bind('<<ComboboxSelected>>', lambda _e: setattr(self, 'lang_touched', True))
        self.fill_languages()
        self.label(grid, 'ui.dest').grid(row=i + 1, column=0, sticky='w', pady=4)
        ttk.Entry(grid, textvariable=self.vars['dest']).grid(row=i + 1, column=1, sticky='ew', pady=4)
        self.button(grid, 'ui.browse', self.pick_dest).grid(row=i + 1, column=2, padx=(8, 0))
        self.update_hint = ttk.Label(f, wraplength=620, justify='left')
        self.update_hint.pack(anchor='w', pady=(8, 0))
        self.form_error = ttk.Label(f, style='Err.TLabel', wraplength=620, justify='left')
        self.form_error.pack(anchor='w', pady=(8, 0))
        if self.game not in NUP_GAMES:
            self.label(f, 'ui.keys_private', wraplength=620, justify='left').pack(anchor='w', pady=(8, 0))
        return f

    def build_run(self):
        f = ttk.Frame(self.body)
        bar = ttk.Frame(f)
        bar.pack(side='bottom', fill='x', pady=(8, 0))
        self.cancel_button = self.button(bar, 'ui.cancel', self.cancel)
        self.cancel_button.pack(side='right')
        self.log_toggle = self.button(bar, 'ui.show_log', self.toggle_log)
        self.log_toggle.pack(side='left')
        self.run_header = ttk.Label(f, font=(self.ui_font, 11, 'bold'))
        self.run_header.pack(anchor='w')
        self.step_rows = {}
        self.steps_box = ttk.Frame(f)
        self.steps_box.pack(fill='x', pady=(8, 8))
        self.phase_label = ttk.Label(f, wraplength=620, justify='left')
        self.phase_label.pack(anchor='w')
        self.bar = ttk.Progressbar(f, mode='determinate', maximum=1000)
        self.bar.pack(fill='x', pady=(4, 6))
        self.space_label = ttk.Label(f, wraplength=620, justify='left')
        self.space_label.pack(anchor='w')
        self.warn_label = ttk.Label(f, wraplength=620, justify='left', style='Warn.TLabel')
        self.warn_label.pack(anchor='w')
        self.log_frame = ttk.Frame(f)
        self.log = tk.Text(self.log_frame, height=10, wrap='word', state='disabled', font=(self.mono_font, 9), bg=self.t['field'], fg=self.t['text'], insertbackground=self.t['text'],
                            relief='flat', highlightthickness=2, highlightbackground=self.t['border'], highlightcolor=self.t['border'])
        scroll = ttk.Scrollbar(self.log_frame, command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        scroll.pack(side='right', fill='y')
        self.log.pack(side='left', fill='both', expand=True)
        self.log_open = False
        return f

    def build_finish(self):
        f = ttk.Frame(self.body)
        bar = ttk.Frame(f)
        bar.pack(side='bottom', fill='x', pady=(12, 0))
        self.button(bar, 'ui.close', self.on_close).pack(side='right')
        self.open_button = self.button(bar, 'ui.open_folder', self.open_folder)
        self.open_button.pack(side='right', padx=(0, 8))
        self.back_button = self.button(bar, 'ui.back', lambda: self.show('form'))
        self.back_button.pack(side='left')
        self.finish_title = ttk.Label(f, style='Title.TLabel', wraplength=620, justify='left')
        self.finish_title.pack(anchor='w')
        self.finish_text = ttk.Label(f, wraplength=620, justify='left')
        self.finish_text.pack(anchor='w', pady=(8, 8))
        self.finish_steps = ttk.Frame(f)
        self.finish_steps.pack(fill='x', pady=(0, 8))
        self.finish_notice = self.notice(f)
        self.finish_notice.pack(fill='x', pady=(8, 0))
        return f

    # ---- form -----------------------------------------------------------------------------------------------------

    def form(self):
        return Form(base=self.vars['base'].get(), update=self.vars['update'].get(), keys=self.vars['keys'].get(),
                    lang=self.selected_lang(), dest=self.vars['dest'].get(), game=self.game,
                    exefs=self.vars['exefs'].get(), romfs=self.vars['romfs'].get(), package=self.vars['package'].get(),
                    nup=self.vars['nup'].get())

    def refresh_form(self):
        form = self.form()
        state = form.installed_state()
        if state is not None:
            self.update_hint.configure(text=self.cat('ui.update_mode'))
            recorded = ini.canonical_language(state.get('lang'))
            if recorded in ini.LANGUAGES and not self.lang_touched:
                self.vars['lang'].set(self.lang_label(recorded))
        else:
            self.update_hint.configure(text='')
        self.fit()

    def pick_file(self, name, pick_id):
        if self.game in NUP_GAMES and name == 'nup':
            types = [(self.cat('dkb.ui.type_nup'), '*.nup'), (self.cat('ui.type_all'), '*.*')]
            path = filedialog.askopenfilename(title=self.cat(pick_id), filetypes=types)
            if path:
                self.vars[name].set(os.path.normpath(path))
            return
        if self.game in NUP_GAMES:
            path = filedialog.askdirectory(title=self.cat(pick_id), mustexist=True)
            if path:
                self.vars[name].set(os.path.normpath(path))
            return
        types = [(self.cat('ui.type_game'), '*.nsp *.xci'), (self.cat('ui.type_all'), '*.*')]
        if name == 'update':
            types = [(self.cat('ui.type_nsp'), '*.nsp'), (self.cat('ui.type_all'), '*.*')]
        if name == 'keys':
            types = [(self.cat('ui.type_keys'), '*.keys'), (self.cat('ui.type_all'), '*.*')]
        path = filedialog.askopenfilename(title=self.cat(pick_id), filetypes=types)
        if path:
            self.vars[name].set(os.path.normpath(path))

    def pick_dest(self):
        path = filedialog.askdirectory(title=self.cat('ui.pick_dest'), mustexist=False)
        if path:
            self.vars['dest'].set(os.path.normpath(path))

    # ---- run ------------------------------------------------------------------------------------------------------

    def start(self):
        form = self.form()
        problems = form.problems()
        if problems:
            self.form_error.configure(text=' '.join(self.cat(p) for p in problems))
            self.fit()
            return
        self.form_error.configure(text='')
        command = form.command(self.layout.python, root=self.layout.root if self.layout.dev else None,
                               temp=self.run_args.get('temp'), verify=self.run_args.get('verify', False),
                               tools=self.run_args.get('tools'))
        self.begin(command, self.layout.root)

    def begin(self, command, cwd):
        self.model = Model(DKB_STEP_ORDER if self.game in NUP_GAMES else None)
        self.model.dest = self.vars['dest'].get().strip()
        self.clear_log()
        self.build_step_rows(self.model.steps)
        self.cancel_button.configure(state='normal')
        self.bind_text(self.cancel_button, 'text', 'ui.cancel')
        self.show('run')
        self.refresh_run()
        self.process = Process(command, cwd=cwd, helper_python=self.layout.python)
        self.root.after(POLL_MS, self.poll)

    def build_step_rows(self, steps):
        for w in self.steps_box.winfo_children():
            w.destroy()
        self.step_rows = {}
        for s in steps:
            row = ttk.Frame(self.steps_box)
            row.pack(fill='x', pady=1)
            glyph = ttk.Label(row, width=3)
            glyph.pack(side='left')
            name = ttk.Label(row)
            name.pack(side='left')
            self.step_rows[s] = (glyph, name)

    def poll(self):
        if self.process is None:
            return
        try:
            while True:
                line = self.process.lines.get_nowait()
                if line is not None:
                    self.feed(line)
        except Exception:
            pass
        code = self.process.poll()
        self.refresh_run()
        if code is None:
            self.root.after(POLL_MS, self.poll)
            return
        self.model.finish(code)
        self.process = None
        self.refresh_run()
        self.refresh_finish()
        self.show('finish')
        if self.closing:
            self.root.destroy()

    def feed(self, line):
        """One stdout line of the CLI: an event updates the model and the log, anything else goes to the log as is."""
        event = parse_line(line)
        if event is None:
            if line.strip():
                self.log_entries.append(line.rstrip())
                self.append_log(line.rstrip())
            return
        before = list(self.model.steps)
        self.model.apply(event)
        if self.model.steps != before:
            self.build_step_rows(self.model.steps)
        self.log_entries.append(event)
        self.append_log(describe_event(self.cat, event))

    def refresh_run(self):
        m = self.model
        for s, (glyph, name) in self.step_rows.items():
            state = m.states.get(s, PENDING)
            glyph.configure(text=GLYPH[state], foreground=self.status_color[state])
            name.configure(text=self.cat('step.' + s) + (f'  ({self.cat("ui.state_" + state)})'
                                                          if state in (SKIPPED, FAILED) else ''),
                           font=(self.ui_font, 10, 'bold') if state == RUNNING else (self.ui_font, 10))
        mode = m.mode or 'install'
        self.run_header.configure(text=self.cat('ui.running_' + mode if mode in ('install', 'resume', 'update')
                                                else 'ui.running_install'))
        if m.current and m.phase:
            text = self.cat(f'phase.{m.current}.{m.phase}')
            if m.total:
                text += '  ' + amount(self.cat, m.done, m.total, m.unit)
        elif m.current:
            text = self.cat('step.' + m.current)
        else:
            text = ''
        self.phase_label.configure(text=text)
        frac = m.fraction()
        if frac is None:
            if str(self.bar.cget('mode')) != 'indeterminate':
                self.bar.configure(mode='indeterminate')
                self.bar.start(15)
        else:
            if str(self.bar.cget('mode')) == 'indeterminate':
                self.bar.stop()
                self.bar.configure(mode='determinate')
            self.bar.configure(value=frac * 1000)
        self.space_label.configure(text=m.need_text(self.cat))
        self.warn_label.configure(text='\n'.join(self.cat(i, **p) for i, p in m.warnings))
        self.fit()   # new step rows and warnings make the screen taller

    def toggle_log(self):
        self.log_open = not self.log_open
        if self.log_open:
            self.log_frame.pack(fill='both', expand=True, pady=(8, 0))
        else:
            self.log_frame.pack_forget()
        self.bind_text(self.log_toggle, 'text', 'ui.hide_log' if self.log_open else 'ui.show_log')
        self.fit()

    def clear_log(self):
        self.log_entries = []
        self.render_log()

    def render_log(self):
        """The whole log from the kept events in the current language (raw CLI lines stay as they are)."""
        self.log.configure(state='normal')
        self.log.delete('1.0', 'end')
        for entry in self.log_entries:
            text = entry if isinstance(entry, str) else describe_event(self.cat, entry)
            if text:
                self.log.insert('end', text + '\n')
        self.log.see('end')
        self.log.configure(state='disabled')

    def append_log(self, text):
        if text:
            self.log.configure(state='normal')
            self.log.insert('end', text + '\n')
            self.log.see('end')
            self.log.configure(state='disabled')

    def cancel(self):
        if self.process:
            self.cancel_button.configure(state='disabled')
            self.bind_text(self.cancel_button, 'text', 'ui.cancelling')
            self.process.cancel()

    def on_close(self):
        if self.process:
            if not messagebox.askyesno(self.cat('ui.title'), self.cat('ui.confirm_cancel')):
                return
            self.closing = True
            self.cancel()
            return
        self.root.destroy()

    # ---- finish ---------------------------------------------------------------------------------------------------

    def refresh_finish(self):
        m = self.model
        result = m.result or 'ok'
        self.finish_title.configure(text=self.cat('ui.result_' + result))
        if result == 'ok':
            text = self.cat('ui.done_text', dest=m.dest)
        elif result == 'cancelled':
            text = self.cat('ui.cancelled_text')
        else:
            err = m.error or ('ui.process_died', {'code': m.exit_code}, None)
            text = self.cat(err[0], **err[1])
            if m.log_path:
                text += '\n\n' + self.cat('ui.log_file', path=m.log_path)
        self.finish_text.configure(text=text)
        for w in self.finish_steps.winfo_children():
            w.destroy()
        for s in m.steps:
            state = m.states.get(s, PENDING)
            ttk.Label(self.finish_steps, text=f'{GLYPH[state]}  {self.cat("step." + s)}',
                      foreground=self.status_color[state]).pack(anchor='w')
        if result == 'ok':
            self.open_button.state(['!disabled'])
        else:
            self.open_button.state(['disabled' if not Path(m.dest or '.').is_dir() else '!disabled'])

    def open_folder(self):
        dest = self.model.dest
        if dest and Path(dest).is_dir():
            if os.name == 'nt':
                os.startfile(dest)
            else:
                subprocess.Popen(['xdg-open', dest], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 start_new_session=True)


# ---- self test ---------------------------------------------------------------------------------------------------

SAMPLE_START = {'type': 'start', 'release': 'selftest', 'mode': 'install', 'dest': 'Z:/selftest/TotK', 'log': 'D:\\log.txt',
                'steps': ['check', 'extract', 'lift', 'compile', 'prepare', 'cleanup']}
SAMPLE_RUN = [
    SAMPLE_START,
    {'type': 'plan', 'run': ['check', 'extract'], 'skip': {}},
    {'type': 'step', 'step': 'check', 'state': 'started'},
    {'type': 'progress', 'step': 'check', 'phase': 'inputs'},
    {'type': 'space', 'dest': 'Z:/selftest/TotK', 'need': 36_400_000_000, 'free': 210_000_000_000, 'temp': 'C:\\t',
     'temp_need': 0, 'temp_free': 0, 'same_disk': True},
    {'type': 'warning', 'id': 'warn.lang_needs_fonts', 'params': {'lang': 'ko'}},
    {'type': 'step', 'step': 'check', 'state': 'done', 'seconds': 12.0},
    {'type': 'step', 'step': 'extract', 'state': 'started'},
    {'type': 'progress', 'step': 'extract', 'phase': 'copy', 'done': 6_000_000_000, 'total': 16_000_000_000,
     'unit': 'bytes'},
]


SAMPLE_OK = [SAMPLE_START, {'type': 'plan', 'run': ['check', 'extract'], 'skip': {'lift': 'current', 'compile': 'current',
                                                                       'prepare': 'current', 'cleanup': 'not_needed'}},
             {'type': 'step', 'step': 'check', 'state': 'started'},
             {'type': 'step', 'step': 'check', 'state': 'done', 'seconds': 1.0},
             {'type': 'step', 'step': 'extract', 'state': 'started'},
             {'type': 'step', 'step': 'extract', 'state': 'done', 'seconds': 1.0},
             {'type': 'step', 'step': 'lift', 'state': 'skipped', 'reason': 'current'},
             {'type': 'step', 'step': 'compile', 'state': 'skipped', 'reason': 'current'},
             {'type': 'step', 'step': 'prepare', 'state': 'skipped', 'reason': 'current'},
             {'type': 'step', 'step': 'cleanup', 'state': 'done', 'seconds': 0.1},
             {'type': 'end', 'result': 'ok', 'seconds': 3}]


def grab(root, path):
    if ImageGrab is None:
        raise SystemExit('--shots needs Pillow')
    root.attributes('-topmost', True)
    root.update()
    x, y, w, h = root.winfo_rootx(), root.winfo_rooty(), root.winfo_width(), root.winfo_height()
    ImageGrab.grab(bbox=(x, y, x + w, y + h), all_screens=True).save(path)


def selftest(layout, shots=None, game='totk'):
    """Every screen; the window closes itself. Returns 0 when nothing raised."""
    fix_tcl_paths()
    fix_dpi_awareness()
    root = tk.Tk()
    root.geometry('+60+40')
    app = App(root, layout, game=game)
    steps = []
    destination = {'bananza': 'Z:/selftest/Bananza', 'raiders': 'Z:/selftest/SPR'}.get(game, 'Z:/selftest/TotK')

    def shot(name):
        root.update()
        if shots:
            Path(shots).mkdir(parents=True, exist_ok=True)
            grab(root, Path(shots) / f'{name}.png')

    def run_all():
        app.vars['dest'].set(destination)
        app.vars['base'].set('Z:/selftest/totk.xci')
        app.vars['update'].set('Z:/selftest/totk-1.2.1.nsp')
        app.vars['keys'].set('Z:/selftest/prod.keys')
        if game in NUP_GAMES:
            app.vars['exefs'].set(destination + '/exefs')
            app.vars['romfs'].set(destination + '/romfs')
        app.show('form')
        app.start()   # missing files: shows the problems
        steps.append(app.form_error.cget('text'))
        shot('form')
        app.model = Model()
        app.clear_log()
        app.build_step_rows(app.model.steps)
        app.show('run')
        for e in samples(SAMPLE_RUN, game):
            app.feed(json.dumps(e))
        app.log_open = False
        app.toggle_log()
        app.refresh_run()
        shot('run')
        app.model = Model()
        for e in samples(SAMPLE_OK, game):
            app.model.apply(e)
        app.model.dest = destination
        app.model.finish(0)
        app.refresh_finish()
        app.show('finish')
        shot('finish-ok')
        app.model = Model()
        for e in samples(SAMPLE_RUN[:3], game) + [{'type': 'error', 'id': 'check.disk_space', 'step': 'check',
                                    'params': {'path': destination, 'need_gb': 36.4, 'free_gb': 20.1}},
                                   {'type': 'end', 'result': 'failed', 'seconds': 3}]:
            app.model.apply(e)
        app.model.finish(1)
        app.refresh_finish()
        shot('finish-failed')
        app.model = Model()
        for e in samples(SAMPLE_RUN[:3], game):
            app.model.apply(e)
        app.model.finish(3)
        app.refresh_finish()
        shot('finish-cancelled')
        app.toggle_log()
        root.destroy()

    root.after(200, run_all)
    root.after(60000, root.destroy)   # never leaves a window open
    root.mainloop()
    return 0 if len(steps) == 1 and all(steps) else 1


def samples(events, game):
    """Keep the same screen fixtures for both profiles, with each profile's actual step names."""
    if game not in NUP_GAMES:
        return events
    mapping = {'extract': 'dkb_data', 'prepare': 'dkb_verify'}
    result = []
    for original in events:
        e = dict(original)
        if 'dest' in e:
            e['dest'] = 'Z:/selftest/SPR' if game == 'raiders' else 'Z:/selftest/Bananza'
        if e.get('step') in mapping:
            e['step'] = mapping[e['step']]
        if e['type'] == 'start':
            e['steps'] = list(DKB_STEP_ORDER)
            e['dest'] = 'Z:/selftest/SPR' if game == 'raiders' else 'Z:/selftest/Bananza'
        if e['type'] == 'plan':
            e['run'] = [mapping.get(s, s) for s in e.get('run', [])]
            e['skip'] = {mapping.get(s, s): reason for s, reason in e.get('skip', {}).items()}
        if e['type'] == 'progress':
            e['phase'] = 'dkb_run' if e['step'] != 'check' else 'dkb_base'
        result.append(e)
    if any(e['type'] == 'end' and e.get('result') == 'ok' for e in result):
        # A fresh successful Bananza install completed every real stage; show no pending or fictitious skipped work.
        start = next(e for e in result if e['type'] == 'start')
        result = [start, {'type': 'plan', 'run': list(DKB_STEP_ORDER), 'skip': {}}]
        for step in DKB_STEP_ORDER:
            result += [{'type': 'step', 'step': step, 'state': 'started'},
                       {'type': 'step', 'step': step, 'state': 'done', 'seconds': 1.0}]
        result.append({'type': 'end', 'result': 'ok', 'seconds': float(len(DKB_STEP_ORDER))})
    return result


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--shots', type=Path)
    ap.add_argument('--root', type=Path)
    ap.add_argument('--game', choices=('raiders', 'bananza', 'totk'), default='raiders')
    for name in ('package', 'toolchain', 'deps', 'ninja', 'dll-dir', 'translator', 'dotnet'):
        ap.add_argument('--' + name, type=Path)
    ap.add_argument('--jobs', type=int, default=6)
    ap.add_argument('--no-update-check', action='store_true', help='do not look for a newer release (or DKB_NO_UPDATE_CHECK=1)')
    args = ap.parse_args(argv)
    layout = layouts.find(args.root)
    if args.selftest:
        return selftest(layout, args.shots, args.game)
    fix_tcl_paths()
    fix_dpi_awareness()
    root = tk.Tk()
    tools = {name: getattr(args, name) for name in ('toolchain', 'deps', 'ninja', 'dll_dir', 'translator', 'dotnet', 'jobs')}
    app = App(root, layout, run_args={'tools': tools}, game=args.game,
              check_updates=not update.disabled(args.no_update_check))
    if args.package:
        app.vars['package'].set(str(args.package))
    root.mainloop()
    return 0


if __name__ == '__main__':
    sys.exit(main())
