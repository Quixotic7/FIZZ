# SPDX-License-Identifier: MPL-2.0
"""The promo's 3D title: an FM-1 running FIZZ in space, in front of Jupiter (two bars, 6.76 s, 1080 x 1920).

    /Applications/Blender.app/Contents/MacOS/Blender -b -P space.py -- SCR_DIR OUT_DIR [FIRST LAST]

SCR_DIR holds the firmware's screen, a 240 x 240 PNG a video frame (0001.png ..; make.py writes them); OUT_DIR gets
the renders (0001.png ..). Everything is made here, procedurally: no model, no texture from anywhere else. The FM-1 is
built from site/js/keymap.js's panel (its 1000 x 640 drawing: the body, the screen, the four knobs, KNOB 1..4, the
two OCT buttons, the twelve buttons, the eleven black keys in the hold modes' colours and the sixteen white keys), a
unit 100 of its pixels. Jupiter is a sphere with banded noise (its belts and zones, the Great Red Spot, turbulence
that flows), the stars a Voronoi field on the world. The camera opens on Jupiter's cloud tops through a long lens and
pulls back to the FM-1 floating in front of it, its screen playing the title. Eevee, 16 samples, a bloom in the
compositor."""
import math
import os
import sys

import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
SCR, OUT = os.path.abspath(argv[0]), os.path.abspath(argv[1])
F0, F1 = (int(argv[2]), int(argv[3])) if len(argv) > 3 else (1, 203)
NF = 203                                                # two bars at 30 fps
W, H = 1080, 1920


def hexcol(h, a=1.0):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple((v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4) for v in c) + (a,)


# ---------------------------------------------------------------- the scene
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.frame_start, sc.frame_end = 1, NF
sc.render.fps = 30
sc.render.resolution_x, sc.render.resolution_y, sc.render.resolution_percentage = W, H, 100
sc.render.engine = "BLENDER_EEVEE_NEXT"
sc.eevee.taa_render_samples = 16
sc.view_settings.view_transform = "AgX"
sc.view_settings.look = "AgX - Punchy"
sc.render.image_settings.file_format = "PNG"
sc.render.image_settings.color_mode = "RGB"
sc.render.filepath = os.path.join(OUT, "")


def mat(name, base=None, metal=0.0, rough=0.5, emit=None, strength=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    if base:
        b.inputs["Base Color"].default_value = hexcol(base)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if emit:
        b.inputs["Emission Color"].default_value = hexcol(emit)
        b.inputs["Emission Strength"].default_value = strength
    return m


def box(name, x, y, z, sx, sy, sz, m, bevel=0.0, parent=None):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z))
    o = bpy.context.object
    o.name = name
    o.scale = (sx, sy, sz)
    bpy.ops.object.transform_apply(scale=True)
    if bevel:
        bv = o.modifiers.new("bevel", "BEVEL")
        bv.width, bv.segments = bevel, 4
        bv.limit_method = "NONE"
    o.data.materials.append(m)
    if parent:
        o.parent = parent
    bpy.ops.object.shade_smooth()
    return o


def cyl(name, x, y, z, r, d, m, parent=None, verts=48):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=d, location=(x, y, z))
    o = bpy.context.object
    o.name = name
    o.data.materials.append(m)
    if parent:
        o.parent = parent
    bv = o.modifiers.new("bevel", "BEVEL")
    bv.width, bv.segments = min(0.03, d / 4), 3
    bpy.ops.object.shade_smooth()
    return o


# ---------------------------------------------------------------- the FM-1 (keymap.js's drawing, 100 px a unit)
def P(px, py):                                          # the drawing's pixel -> the panel's plane (its centre 0, 0)
    return (px - 500) / 100.0, -(py - 298) / 100.0


bpy.ops.object.empty_add(location=(0, 0, 0))
fm1 = bpy.context.object
fm1.name = "FM-1"
TOP = 0.32
body = box("body", 0, 0, 0, 9.84, 5.80, 0.64, mat("plate", "#1C1C20", 0.35, 0.42), bevel=0.22, parent=fm1)
box("rim", 0, 0, -0.05, 9.90, 5.86, 0.5, mat("rim", "#0C0C0E", 0.6, 0.3), bevel=0.24, parent=fm1)

# the screen: the take's frames, emissive, sharp pixels (240 x 240 on 1.8 x 1.8 units), in a black bezel
sx, sy = P(390, 124)
box("bezel", sx, sy, TOP + 0.005, 2.0, 2.0, 0.04, mat("bezel", "#050506", 0.0, 0.2), bevel=0.06, parent=fm1)
bpy.ops.mesh.primitive_plane_add(size=1.8, location=(sx, sy, TOP + 0.03))
scr = bpy.context.object
scr.name = "screen"
scr.parent = fm1
sm = bpy.data.materials.new("screen")
sm.use_nodes = True
nt = sm.node_tree
nt.nodes.clear()
img = bpy.data.images.load(os.path.join(SCR, "0001.png"))
img.source = "SEQUENCE"
tex = nt.nodes.new("ShaderNodeTexImage")
tex.image = img
tex.interpolation = "Closest"
tex.image_user.frame_duration = NF
tex.image_user.frame_start = 1
tex.image_user.frame_offset = 0
tex.image_user.use_auto_refresh = True
em = nt.nodes.new("ShaderNodeEmission")
em.inputs["Strength"].default_value = 2.4
gl = nt.nodes.new("ShaderNodeBsdfGlossy")
gl.inputs["Roughness"].default_value = 0.08
gl.inputs["Color"].default_value = (0.06, 0.06, 0.07, 1)
add = nt.nodes.new("ShaderNodeAddShader")
out = nt.nodes.new("ShaderNodeOutputMaterial")
nt.links.new(tex.outputs["Color"], em.inputs["Color"])
nt.links.new(em.outputs[0], add.inputs[0])
nt.links.new(gl.outputs[0], add.inputs[1])
nt.links.new(add.outputs[0], out.inputs["Surface"])
scr.data.materials.append(sm)

# the knobs: MASTER, SELECT, PRESETS, ALGORITHM, KNOB 1..4, each with its mark in its colour (keymap.js's KNOBS)
KNOBS = [(84, 82, None), (204, 82, "#FFD23F"), (84, 196, "#28E1FF"), (204, 196, "#FF8FC8"),
         (560, 70, "#3F7DFF"), (660, 70, "#1FD47E"), (760, 70, "#F1F0EA"), (860, 70, "#FF6A2C")]
kmat = mat("knob", "#2A2B30", 0.85, 0.28)
kcap = mat("knobcap", "#141417", 0.5, 0.45)
for i, (cx, cy, col) in enumerate(KNOBS):
    x, y = P(cx, cy)
    cyl("knob%d" % i, x, y, TOP + 0.16, 0.30, 0.32, kmat, fm1)
    cyl("knobcap%d" % i, x, y, TOP + 0.33, 0.26, 0.04, kcap, fm1)
    rmat = mat("ring%d" % i, "#111111", 0, 0.4, emit=col or "#9AA0A8", strength=2.5 if col else 0.6)
    bpy.ops.mesh.primitive_torus_add(major_radius=0.36, minor_radius=0.018, location=(x, y, TOP + 0.01))
    bpy.context.object.parent = fm1
    bpy.context.object.data.materials.append(rmat)
    box("mark%d" % i, x, y + 0.15, TOP + 0.36, 0.035, 0.16, 0.02, rmat, parent=fm1)

# OCT- / OCT+ and the twelve buttons (keymap.js: 2 rows of 6; SHIFT yellow, PLAY green, REC red)
bmat = mat("button", "#2B2D33", 0.2, 0.38)
for x0 in (48, 168):
    x, y = P(x0 + 36, 285)
    box("oct%d" % x0, x, y, TOP + 0.06, 0.72, 0.34, 0.14, bmat, bevel=0.06, parent=fm1)
BCOL = [None, None, None, None, None, "#FFD23F", None, None, None, None, "#1FD47E", "#FF3448"]
for i, col in enumerate(BCOL):
    bx, by = 516 + (i % 6) * 80, 166 + (i // 6) * 92
    x, y = P(bx + 34, by + 22)
    box("btn%d" % i, x, y, TOP + 0.07, 0.68, 0.44, 0.16, bmat, bevel=0.08, parent=fm1)
    led = mat("led%d" % i, "#111111", 0, 0.4, emit=col or "#E8EAED", strength=3.0 if col else 0.9)
    box("btnled%d" % i, x, y + 0.13, TOP + 0.155, 0.40, 0.035, 0.01, led, parent=fm1)

# the keybed: eleven black keys (their LEDs the hold modes' colours) over sixteen white keys
MODES = ["#3F7DFF", "#1FD47E", "#36D6E7", "#A879FF", "#FF6A2C", "#FFD23F", "#FF52C0", "#9FE6FF", "#F1F0EA",
         "#B5E853", "#D8C39A"]
AFTER = [0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14]
W0, WW, KW = 36, 58, 52
kb = mat("blackkey", "#1A1B1F", 0.1, 0.35)
for i, (after, col) in enumerate(zip(AFTER, MODES)):
    x, y = P(W0 + (after + 1) * WW - KW / 2 - 3 + KW / 2, 384 + 43)
    box("bkey%d" % i, x, y, TOP + 0.08, 0.52, 0.86, 0.18, kb, bevel=0.07, parent=fm1)
    led = mat("bled%d" % i, "#111111", 0, 0.4, emit=col, strength=3.5)
    box("bkled%d" % i, x, y + 0.30, TOP + 0.175, 0.30, 0.05, 0.01, led, parent=fm1)
kw = mat("whitekey", "#E9E6DE", 0.0, 0.32)
for i in range(16):
    x, y = P(W0 + i * WW + KW / 2, 482 + 43)
    box("wkey%d" % i, x, y, TOP + 0.08, 0.52, 0.86, 0.18, kw, bevel=0.07, parent=fm1)

# its pose: floating, facing the camera, turning a little
fm1.location = (0.0, 0.0, 0.0)
fm1.rotation_euler = (math.radians(62), 0, math.radians(-14))
fm1.keyframe_insert("rotation_euler", frame=1)
fm1.rotation_euler = (math.radians(58), math.radians(-3), math.radians(4))
fm1.keyframe_insert("rotation_euler", frame=NF)
fm1.location = (0.4, 0.0, -0.9)
fm1.keyframe_insert("location", frame=1)
fm1.location = (0.0, 0.0, 0.0)
fm1.keyframe_insert("location", frame=NF)

# ---------------------------------------------------------------- Jupiter
JC, JR = Vector((9.0, 75.0, 20.0)), 32.0
bpy.ops.mesh.primitive_uv_sphere_add(segments=256, ring_count=128, radius=JR, location=JC)
jup = bpy.context.object
jup.name = "Jupiter"
bpy.ops.object.shade_smooth()
jup.rotation_euler = (math.radians(14), math.radians(-8), math.radians(-6))
jup.keyframe_insert("rotation_euler", frame=1)
jup.rotation_euler = (math.radians(14), math.radians(-8), math.radians(2))
jup.keyframe_insert("rotation_euler", frame=NF)
jm = bpy.data.materials.new("jupiter")
jm.use_nodes = True
nt = jm.node_tree
N, L = nt.nodes, nt.links
N.clear()


def node(kind, **inputs):
    n = N.new(kind)
    for k, v in inputs.items():
        if k.startswith("_"):
            setattr(n, k[1:], v)
        else:
            n.inputs[k].default_value = v
    return n


def math_(op, a, b=None, c=None):
    n = node("ShaderNodeMath", _operation=op)
    for i, v in enumerate((a, b, c)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[i].default_value = v
        else:
            L.new(v, n.inputs[i])
    return n.outputs[0]


def vmath(op, a, b):
    n = node("ShaderNodeVectorMath", _operation=op)
    for i, v in enumerate((a, b)):
        if isinstance(v, tuple):
            n.inputs[i].default_value = v
        else:
            L.new(v, n.inputs[i])
    return n.outputs[0]


tc = N.new("ShaderNodeTexCoord")
# the clouds are stretched along the longitude: the coordinates squeezed in x, y
co = vmath("MULTIPLY", tc.outputs["Object"], (0.45, 0.45, 2.4))
# domain warp: a 4D noise (W the time: the clouds flow) pushes the coordinates about; the bands are read from the
# pushed latitude, so their edges curl into festoons and eddies
warp = node("ShaderNodeTexNoise", _noise_dimensions="4D", Scale=0.09, Detail=7.0, Roughness=0.6, Lacunarity=2.3)
warp.inputs["W"].keyframe_insert("default_value", frame=1)
warp.inputs["W"].default_value = 0.35
warp.inputs["W"].keyframe_insert("default_value", frame=NF)
L.new(co, warp.inputs["Vector"])
wv = vmath("SUBTRACT", warp.outputs["Color"], (0.5, 0.5, 0.5))
co1 = vmath("ADD", co, vmath("MULTIPLY", wv, (9.0, 9.0, 9.0)))
warp2 = node("ShaderNodeTexNoise", Scale=0.4, Detail=8.0, Roughness=0.62)   # a second, finer warp: the eddies
L.new(co1, warp2.inputs["Vector"])
co2 = vmath("ADD", co1, vmath("MULTIPLY", vmath("SUBTRACT", warp2.outputs["Color"], (0.5, 0.5, 0.5)), (3.2, 3.2, 3.2)))
sep = node("ShaderNodeSeparateXYZ")
L.new(co2, sep.inputs["Vector"])
lat = math_("MULTIPLY_ADD", sep.outputs["Z"], 0.5 / (JR * 2.4), 0.5)       # -> 0..1, pole to pole
ramp = node("ShaderNodeValToRGB")
cr = ramp.color_ramp
cr.interpolation = "LINEAR"
BANDS = [(0.00, "#4E4038"), (0.10, "#7E6650"), (0.17, "#D2BC98"), (0.22, "#9C623A"), (0.28, "#F0DFBE"),
         (0.33, "#C2743A"), (0.38, "#FBEFD8"), (0.425, "#B45A2A"), (0.46, "#6E321C"), (0.50, "#E0BE8C"),
         (0.54, "#FFF5E0"), (0.585, "#CC7438"), (0.62, "#88401F"), (0.66, "#EED6AA"), (0.72, "#BE8452"),
         (0.78, "#F2E0BE"), (0.85, "#9E7856"), (0.92, "#6E5A4A"), (1.00, "#4A3E38")]
while len(cr.elements) < len(BANDS):
    cr.elements.new(0.5)
for e, (p, c) in zip(cr.elements, BANDS):
    e.position, e.color = p, hexcol(c)
L.new(lat, ramp.inputs["Fac"])
# fine turbulence in the warped clouds: lighter and darker wisps
fine = node("ShaderNodeTexNoise", Scale=1.1, Detail=12.0, Roughness=0.68)
# the smallest clouds: a fast, strongly warped noise, its own warp from a third noise (the close-up's texture)
warp3 = node("ShaderNodeTexNoise", Scale=2.2, Detail=6.0, Roughness=0.6)
L.new(co2, warp3.inputs["Vector"])
co3 = vmath("ADD", co2, vmath("MULTIPLY", vmath("SUBTRACT", warp3.outputs["Color"], (0.5, 0.5, 0.5)), (1.1, 1.1, 0.5)))
tiny = node("ShaderNodeTexNoise", Scale=7.0, Detail=14.0, Roughness=0.7)
L.new(co3, tiny.inputs["Vector"])
L.new(co2, fine.inputs["Vector"])
# the Great Red Spot: an oval in the southern belt (on the warped coordinates a little, so it swirls), a pale ring
d = vmath("SUBTRACT", tc.outputs["Object"], (-8.0, -JR * 0.86, -JR * 0.36))
d = vmath("ADD", d, vmath("MULTIPLY", vmath("SUBTRACT", warp3.outputs["Color"], (0.5, 0.5, 0.5)), (2.4, 2.4, 1.4)))
d = vmath("MULTIPLY", d, (1 / 6.0, 1 / 6.0, 1 / 3.0))
ln = node("ShaderNodeVectorMath", _operation="LENGTH")
L.new(d, ln.inputs[0])
core = node("ShaderNodeMapRange", **{"From Min": 0.95, "From Max": 0.6})
L.new(ln.outputs["Value"], core.inputs["Value"])
ring = node("ShaderNodeMapRange", **{"From Min": 1.35, "From Max": 1.0})
L.new(ln.outputs["Value"], ring.inputs["Value"])
c1 = node("ShaderNodeMixRGB", Color2=hexcol("#F6E8CC"))
L.new(ring.outputs["Result"], c1.inputs["Fac"])
L.new(ramp.outputs["Color"], c1.inputs["Color1"])
c2 = node("ShaderNodeMixRGB", Color2=hexcol("#C04628"))
L.new(core.outputs["Result"], c2.inputs["Fac"])
L.new(c1.outputs["Color"], c2.inputs["Color1"])
wisp = node("ShaderNodeMixRGB", _blend_type="OVERLAY", Fac=0.6)
L.new(c2.outputs["Color"], wisp.inputs["Color1"])
L.new(fine.outputs["Color"], wisp.inputs["Color2"])
wisp2 = node("ShaderNodeMixRGB", _blend_type="OVERLAY", Fac=0.7)
L.new(wisp.outputs["Color"], wisp2.inputs["Color1"])
L.new(tiny.outputs["Color"], wisp2.inputs["Color2"])
bs = node("ShaderNodeBsdfPrincipled", Roughness=0.95)
L.new(wisp2.outputs["Color"], bs.inputs["Base Color"])
# the atmosphere's rim: a pale blue glow at the limb
lw = node("ShaderNodeLayerWeight", Blend=0.25)
rim = node("ShaderNodeEmission", Color=hexcol("#A9C4FF"))
L.new(math_("MULTIPLY", math_("POWER", lw.outputs["Facing"], 3.0), 0.6), rim.inputs["Strength"])
addj = N.new("ShaderNodeAddShader")
L.new(bs.outputs[0], addj.inputs[0])
L.new(rim.outputs[0], addj.inputs[1])
jo = N.new("ShaderNodeOutputMaterial")
L.new(addj.outputs[0], jo.inputs["Surface"])
jup.data.materials.append(jm)

# ---------------------------------------------------------------- the stars (the world: a Voronoi field, two layers)
world = bpy.data.worlds.new("space")
sc.world = world
world.use_nodes = True
nt = world.node_tree
N, L = nt.nodes, nt.links
N.clear()
wtc = N.new("ShaderNodeTexCoord")
acc = None
for scale, size, gain in ((180.0, 0.06, 2.4), (420.0, 0.05, 1.2)):
    vo = N.new("ShaderNodeTexVoronoi")
    vo.inputs["Scale"].default_value = scale
    vo.inputs["Randomness"].default_value = 1.0
    L.new(wtc.outputs["Generated"], vo.inputs["Vector"])
    mr = N.new("ShaderNodeMapRange")
    mr.inputs["From Min"].default_value = size
    mr.inputs["From Max"].default_value = 0.0
    L.new(vo.outputs["Distance"], mr.inputs["Value"])
    pw = N.new("ShaderNodeMath")                        # sharp points
    pw.operation = "POWER"
    pw.inputs[1].default_value = 3.0
    L.new(mr.outputs["Result"], pw.inputs[0])
    cs = N.new("ShaderNodeSeparateColor")               # each star its own brightness; most faint, a few bright
    L.new(vo.outputs["Color"], cs.inputs["Color"])
    br = N.new("ShaderNodeMath")
    br.operation = "POWER"
    br.inputs[1].default_value = 4.0
    L.new(cs.outputs["Red"], br.inputs[0])
    m1 = N.new("ShaderNodeMath")
    m1.operation = "MULTIPLY"
    L.new(pw.outputs[0], m1.inputs[0])
    L.new(br.outputs[0], m1.inputs[1])
    m2 = N.new("ShaderNodeMath")
    m2.operation = "MULTIPLY"
    m2.inputs[1].default_value = gain * 40.0
    L.new(m1.outputs[0], m2.inputs[0])
    if acc is None:
        acc = m2
    else:
        a2 = N.new("ShaderNodeMath")
        a2.operation = "ADD"
        L.new(acc.outputs[0], a2.inputs[0])
        L.new(m2.outputs[0], a2.inputs[1])
        acc = a2
# a faint violet haze (a nebula's hint) under the stars
neb = N.new("ShaderNodeTexNoise")
neb.inputs["Scale"].default_value = 2.5
neb.inputs["Detail"].default_value = 6.0
L.new(wtc.outputs["Generated"], neb.inputs["Vector"])
nebr = N.new("ShaderNodeValToRGB")
nebr.color_ramp.elements[0].position, nebr.color_ramp.elements[0].color = 0.45, (0, 0, 0, 1)
nebr.color_ramp.elements[1].position, nebr.color_ramp.elements[1].color = 0.85, (0.012, 0.006, 0.03, 1)
L.new(neb.outputs["Fac"], nebr.inputs["Fac"])
starc = N.new("ShaderNodeMixRGB")
starc.blend_type = "ADD"
starc.inputs["Fac"].default_value = 1.0
L.new(nebr.outputs["Color"], starc.inputs["Color1"])
sv = N.new("ShaderNodeCombineColor")
L.new(acc.outputs[0], sv.inputs["Red"])
L.new(acc.outputs[0], sv.inputs["Green"])
L.new(acc.outputs[0], sv.inputs["Blue"])
L.new(sv.outputs["Color"], starc.inputs["Color2"])
bg = N.new("ShaderNodeBackground")
L.new(starc.outputs["Color"], bg.inputs["Color"])
wo = N.new("ShaderNodeOutputWorld")
L.new(bg.outputs[0], wo.inputs["Surface"])

# ---------------------------------------------------------------- the light: the Sun from the side, a cool fill
bpy.ops.object.light_add(type="SUN", location=(0, 0, 0))
sun = bpy.context.object
sun.data.energy = 5.0
sun.data.angle = math.radians(0.6)
sun.rotation_euler = Vector((1.3, 0.8, -0.35)).to_track_quat("-Z", "Y").to_euler()
bpy.ops.object.light_add(type="AREA", location=(-4, -8, 7))
fill = bpy.context.object
fill.data.energy = 900
fill.data.size = 8
fill.data.color = (0.75, 0.82, 1.0)
fill.rotation_euler = (math.radians(50), math.radians(-20), math.radians(-25))
bpy.ops.object.light_add(type="AREA", location=(5, 6, 3))
back = bpy.context.object
back.data.energy = 500
back.data.size = 6
back.data.color = (1.0, 0.8, 0.6)
back.rotation_euler = (math.radians(-100), math.radians(30), 0)

# ---------------------------------------------------------------- the camera: Jupiter's cloud tops, then the FM-1
bpy.ops.object.empty_add(location=(0, 0, 0))
aim = bpy.context.object
aim.name = "aim"
bpy.ops.object.camera_add(location=(0, 0, 0))
cam = bpy.context.object
sc.camera = cam
cam.data.sensor_fit = "VERTICAL"
cam.data.sensor_height = 36.0
cam.data.clip_end = 1000
tt = cam.constraints.new("TRACK_TO")
tt.target = aim
tt.track_axis, tt.up_axis = "TRACK_NEGATIVE_Z", "UP_Y"
JUP_PT = JC + Vector((-8.0, -27.0, -9.0))               # a point on its face, near the Red Spot
for f, loc, at, lens in ((1, (1.5, -15.0, 3.2), JUP_PT, 95.0),
                         (70, (1.0, -15.5, 3.0), JUP_PT + Vector((3.0, 0.0, -1.0)), 80.0),
                         (NF, (0.0, -13.0, 4.2), Vector((0.0, 0.0, 1.4)), 26.0)):
    cam.location = loc
    cam.keyframe_insert("location", frame=f)
    aim.location = at
    aim.keyframe_insert("location", frame=f)
    cam.data.lens = lens
    cam.data.keyframe_insert("lens", frame=f)
for o in (cam, aim, cam.data):
    if o.animation_data and o.animation_data.action:
        for fc in o.animation_data.action.fcurves:
            for k in fc.keyframe_points:
                k.interpolation = "BEZIER"
                k.easing = "AUTO"

# ---------------------------------------------------------------- the compositor: a bloom
sc.use_nodes = True
ct = sc.node_tree
for n in list(ct.nodes):
    ct.nodes.remove(n)
rl = ct.nodes.new("CompositorNodeRLayers")
gl = ct.nodes.new("CompositorNodeGlare")
gl.glare_type = "BLOOM" if "BLOOM" in [i.identifier for i in gl.bl_rna.properties["glare_type"].enum_items] else "FOG_GLOW"
gl.quality = "HIGH"
try:
    gl.threshold = 0.85
    gl.size = 8
except AttributeError:
    pass
co = ct.nodes.new("CompositorNodeComposite")
ct.links.new(rl.outputs["Image"], gl.inputs["Image"])
ct.links.new(gl.outputs["Image"], co.inputs["Image"])

os.makedirs(OUT, exist_ok=True)
for f in range(F0, F1 + 1):
    sc.frame_set(f)
    sc.render.filepath = os.path.join(OUT, "%04d.png" % f)
    bpy.ops.render.render(write_still=True)
