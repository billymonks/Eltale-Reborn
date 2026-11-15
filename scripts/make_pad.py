# Writes the Controls page's pad drawing (ModernPad.svg), one highlight layer per control, and the RML for the layers.
# usage: python scripts/make_pad.py assets/icons/VizMap <rml out>
# Then paste the RML over the pad-map__lit layers in assets/config_menu/controls.rml.
import os, sys

W, H = 400, 312
LIT = '#B97DF2'
LIT_OPACITY = 0.65
GAP = 1.5

BODY = ("M 118 54 C 150 46, 250 46, 282 54 C 330 60, 356 78, 368 112 C 386 150, 392 220, 384 268 "
        "C 378 298, 346 310, 326 292 C 310 278, 296 248, 278 238 C 256 228, 144 228, 122 238 "
        "C 104 248, 90 278, 74 292 C 54 310, 22 298, 16 268 C 8 220, 14 150, 32 112 C 44 78, 70 60, 118 54 Z")

# rects: x, y, w, h, rx, stroke width
RECTS = {
    'lt': (70, 4, 60, 24, 10, 3), 'rt': (270, 4, 60, 24, 10, 3),
    'lb': (58, 34, 86, 13, 6.5, 3), 'rb': (256, 34, 86, 13, 6.5, 3),
    'view': (168, 98, 20, 12, 6, 3), 'menu': (212, 98, 20, 12, 6, 3),
}
# D-pad arms: x, y, w, h
DPAD = "M 142 152 h 20 v 16 h 16 v 20 h -16 v 16 h -20 v -16 h -16 v -20 h 16 Z"
DPAD_ARMS = {
    'dpad_up': (142, 152, 20, 16), 'dpad_down': (142, 188, 20, 16),
    'dpad_left': (126, 168, 16, 20), 'dpad_right': (162, 168, 16, 20),
}
STICKS = {'l': (112, 112), 'r': (248, 178)}
STICK_R, CAP_R = 25, 15
FACE = {'y': (288, 88), 'x': (264, 112), 'b': (312, 112), 'a': (288, 136)}
FACE_R = 12
HOME = (200, 74, 11)


def letter(name, cx, cy):
    t, b = cy - 5.5, cy + 5.5
    if name == 'a':
        return f'M {cx - 4.5} {b} L {cx} {t} L {cx + 4.5} {b} M {cx - 2.6} {cy + 1.6} H {cx + 2.6}'
    if name == 'x':
        return f'M {cx - 4} {t} L {cx + 4} {b} M {cx + 4} {t} L {cx - 4} {b}'
    if name == 'y':
        return f'M {cx - 4.5} {t} L {cx} {cy} L {cx + 4.5} {t} M {cx} {cy} V {b}'
    x0 = cx - 3.5
    return (f'M {x0} {t} V {b} H {cx + 0.8} A 2.75 2.75 0 0 0 {cx + 0.8} {cy} H {x0} '
            f'M {x0} {cy} H {cx + 0.2} A 2.75 2.75 0 0 0 {cx + 0.2} {t} H {x0}')


def letter_el(name, colour):
    cx, cy = FACE[name]
    return (f'<path d="{letter(name, cx, cy)}" stroke="{colour}" stroke-width="1.8" stroke-linecap="round" '
            f'stroke-linejoin="round"/>')


def doc(body):
    return (f'<svg width="{W}" height="{H}" viewBox="0 0 {W} {H}" fill="none" xmlns="http://www.w3.org/2000/svg">\n'
            + '\n'.join('    ' + line for line in body) + '\n</svg>\n')


def outline():
    s = [f'<path d="{BODY}" fill="#FFFFFF" fill-opacity="0.04" stroke="#FFFFFF" stroke-width="3.5" stroke-linejoin="round"/>']
    for name, (x, y, w, h, rx, sw) in RECTS.items():
        s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" stroke="#FFFFFF" stroke-width="{sw}"/>')
    s.append(f'<path d="{DPAD}" stroke="#FFFFFF" stroke-width="3" stroke-linejoin="round"/>')
    for cx, cy in STICKS.values():
        s.append(f'<circle cx="{cx}" cy="{cy}" r="{STICK_R}" stroke="#FFFFFF" stroke-width="3"/>')
        s.append(f'<circle cx="{cx}" cy="{cy}" r="{CAP_R}" stroke="#FFFFFF" stroke-width="2.5"/>')
    for name, (cx, cy) in FACE.items():
        s.append(f'<circle cx="{cx}" cy="{cy}" r="{FACE_R}" stroke="#FFFFFF" stroke-width="2.5"/>')
        s.append(letter_el(name, '#FFFFFF'))
    cx, cy, r = HOME
    s.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" stroke="#FFFFFF" stroke-width="2.5"/>')
    return doc(s)


def highlights():
    fill = f'fill="{LIT}" fill-opacity="{LIT_OPACITY}"'
    out = {}
    for name, (x, y, w, h, rx, sw) in RECTS.items():
        e = sw / 2 + GAP
        out[name] = [f'<rect x="{x + e}" y="{y + e}" width="{w - 2 * e}" height="{h - 2 * e}" rx="{max(rx - e, 0)}" {fill}/>']
    e = 1.5 + GAP
    for name, (x, y, w, h) in DPAD_ARMS.items():
        if name == 'dpad_up':
            r = (x + e, y + e, w - 2 * e, h - e)
        elif name == 'dpad_down':
            r = (x + e, y, w - 2 * e, h - e)
        elif name == 'dpad_left':
            r = (x + e, y + e, w - e, h - 2 * e)
        else:
            r = (x, y + e, w - e, h - 2 * e)
        out[name] = [f'<rect x="{r[0]}" y="{r[1]}" width="{r[2]}" height="{r[3]}" {fill}/>']
    for side, (cx, cy) in STICKS.items():
        out[side + 's'] = [f'<circle cx="{cx}" cy="{cy}" r="{STICK_R - 1.5 - GAP}" {fill}/>']
        out[side + '3'] = [f'<circle cx="{cx}" cy="{cy}" r="{CAP_R - 1.25 - GAP}" {fill}/>']
    for name, (cx, cy) in FACE.items():
        out[name] = [f'<circle cx="{cx}" cy="{cy}" r="{FACE_R - 1.25 - GAP}" {fill}/>', letter_el(name, '#FFFFFF')]
    return out


def rml(names):
    ind = ' ' * 36
    return '\n'.join(f'{ind}<svg class="pad-map__lit" src="icons/VizMap/ModernPad_{n}.svg" '
                     f'data-if="inputs.pad.spots.{n}" />' for n in names) + '\n'


icons, rml_out = sys.argv[1], sys.argv[2]
open(os.path.join(icons, 'ModernPad.svg'), 'w', encoding='utf-8', newline='\n').write(outline())
layers = highlights()
for name, body in layers.items():
    open(os.path.join(icons, f'ModernPad_{name}.svg'), 'w', encoding='utf-8', newline='\n').write(doc(body))
open(rml_out, 'w', encoding='utf-8', newline='\n').write(rml(layers))
print('layers:', ' '.join(layers))
