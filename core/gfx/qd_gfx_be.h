/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/. */
/* P2.6: a drawing backend's names. A backend (qd_gfx_plutovg.c, qd_gfx_fast.c) includes this before qd_gfx.h: its
 * drawing and state calls are then qdb_*, and qd_gfx_rec.c (included at its end) defines the public qd_* in front of
 * them: each call goes straight through, or, while a frame is recorded, into its display list (one instant a frame,
 * every strip drawn from it: qd_gfx_rec.c). The calls inside the backend reach its own qdb_* (never recorded). */
#pragma once
#define qd_save qdb_save
#define qd_restore qdb_restore
#define qd_translate qdb_translate
#define qd_scale qdb_scale
#define qd_rotate qdb_rotate
#define qd_alpha qdb_alpha
#define qd_get_alpha qdb_get_alpha
#define qd_clip_rect qdb_clip_rect
#define qd_line_width qdb_line_width
#define qd_line_cap qdb_line_cap
#define qd_line_join qdb_line_join
#define qd_dash qdb_dash
#define qd_line qdb_line
#define qd_polyline qdb_polyline
#define qd_circle qdb_circle
#define qd_arc qdb_arc
#define qd_ellipse qdb_ellipse
#define qd_ellipse_arc qdb_ellipse_arc
#define qd_rrect qdb_rrect
#define qd_fill_rect qdb_fill_rect
#define qd_path_begin qdb_path_begin
#define qd_move_to qdb_move_to
#define qd_line_to qdb_line_to
#define qd_quad_to qdb_quad_to
#define qd_cubic_to qdb_cubic_to
#define qd_path_arc qdb_path_arc
#define qd_path_close qdb_path_close
#define qd_path_fill qdb_path_fill
#define qd_path_stroke qdb_path_stroke
#define qd_path_stroke_keep qdb_path_stroke_keep
#define qd_path_paint qdb_path_paint
#define qd_path_clip qdb_path_clip
#define qd_text qdb_text
#define qd_box_visible qdb_box_visible
#define qd_layer_begin qdb_layer_begin
#define qd_layer_resume qdb_layer_resume
#define qd_layer_end qdb_layer_end
#define qd_layer_draw qdb_layer_draw
