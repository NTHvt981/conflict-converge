"""Canonical team colors for Conflict Converge asset generators.

Single source of truth for "team blue" / "team red" across the tools/
pipeline (gen_sprites, gen_voxels, extract_team_mask, convert_vox_to_sprite).

Values are the voxel palette's armor slot (58, 130, 200) / (200, 70, 60):
extract_team_mask matches rendered shades EXACTLY against these, so changing
them silently breaks mask extraction — update the mask tool in the same
change if these ever move.

Separate concern, do NOT unify here: the game's runtime tint
(Art::TeamTint — raylib BLUE/RED, Okabe-Ito in color-blind mode) is applied
at draw time over whatever the PNGs contain.
"""

TEAM_BLUE_RGB = (58, 130, 200)
TEAM_RED_RGB = (200, 70, 60)

TEAM_BLUE_RGBA = (58, 130, 200, 255)
TEAM_RED_RGBA = (200, 70, 60, 255)
