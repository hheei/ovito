# Cylinder shader variant table.
#
# Vertex shaders: ovito_cylinder_vert(NAME ATTRIBS <file> GEOMETRY <file>)
# Fragment shaders: ovito_cylinder_frag(NAME SURFACE <file> FRAGOUT <file>)
#
# All paths are relative to shaders/cylinders/variants/ (one level up = shaders/cylinders/).

# ---- Cylinder raycast (NormalShading, CylinderShape) — RGB colors ----

ovito_cylinder_vert(cylinder_raycast__vbo_visual_rgb
    ATTRIBS  ../attribs/vbo_visual_rgb.glsl
    GEOMETRY ../geometry/cylinder_raycast_bbox.glsl
)
ovito_cylinder_vert(cylinder_raycast__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted.glsl
    GEOMETRY ../geometry/cylinder_raycast_bbox.glsl
)
ovito_cylinder_vert(cylinder_raycast__vbo_picking
    ATTRIBS  ../attribs/vbo_picking.glsl
    GEOMETRY ../geometry/cylinder_raycast_bbox.glsl
)

ovito_cylinder_frag(cylinder_raycast__color
    SURFACE  ../surface/cylinder_raycast.glsl
    FRAGOUT  ../../particles/fragout/color.glsl
)
ovito_cylinder_frag(cylinder_raycast__oit_accum
    SURFACE  ../surface/cylinder_raycast.glsl
    FRAGOUT  ../../particles/fragout/oit_accum.glsl
)
ovito_cylinder_frag(cylinder_raycast__oit_reveal
    SURFACE  ../surface/cylinder_raycast.glsl
    FRAGOUT  ../../particles/fragout/oit_reveal.glsl
)
ovito_cylinder_frag(cylinder_raycast__picking
    SURFACE  ../surface/cylinder_raycast.glsl
    FRAGOUT  ../../particles/fragout/picking.glsl
)
ovito_cylinder_frag(cylinder_raycast__depth_only
    SURFACE  ../surface/cylinder_raycast.glsl
    FRAGOUT  ../../particles/fragout/depth_only.glsl
)

# ---- Cylinder raycast — pseudo-color scalars ----

ovito_cylinder_vert(cylinder_raycast__vbo_visual_pseudo
    ATTRIBS  ../attribs/vbo_visual_pseudo.glsl
    GEOMETRY ../geometry/cylinder_raycast_bbox.glsl
)

ovito_cylinder_frag_pseudo(cylinder_raycast__pseudo__color
    SURFACE  ../surface/cylinder_raycast.glsl
    FRAGOUT  ../../particles/fragout/color.glsl
)
ovito_cylinder_frag_pseudo(cylinder_raycast__pseudo__oit_accum
    SURFACE  ../surface/cylinder_raycast.glsl
    FRAGOUT  ../../particles/fragout/oit_accum.glsl
)
ovito_cylinder_frag_pseudo(cylinder_raycast__pseudo__oit_reveal
    SURFACE  ../surface/cylinder_raycast.glsl
    FRAGOUT  ../../particles/fragout/oit_reveal.glsl
)

# ---- Cylinder flat billboard (FlatShading, CylinderShape) ----

ovito_cylinder_vert(cylinder_flat__vbo_visual_rgb
    ATTRIBS  ../attribs/vbo_visual_rgb.glsl
    GEOMETRY ../geometry/cylinder_flat_quad.glsl
)
ovito_cylinder_vert(cylinder_flat__vbo_visual_pseudo
    ATTRIBS  ../attribs/vbo_visual_pseudo.glsl
    GEOMETRY ../geometry/cylinder_flat_quad.glsl
)

ovito_cylinder_frag_pseudo(cylinder_flat__pseudo__color
    SURFACE  ../surface/cylinder_flat.glsl
    FRAGOUT  ../../particles/fragout/color_flat.glsl
)
ovito_cylinder_frag_pseudo(cylinder_flat__pseudo__oit_accum
    SURFACE  ../surface/cylinder_flat.glsl
    FRAGOUT  ../../particles/fragout/oit_accum.glsl
)
ovito_cylinder_frag_pseudo(cylinder_flat__pseudo__oit_reveal
    SURFACE  ../surface/cylinder_flat.glsl
    FRAGOUT  ../../particles/fragout/oit_reveal.glsl
)

ovito_cylinder_vert(cylinder_flat__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted.glsl
    GEOMETRY ../geometry/cylinder_flat_quad.glsl
)
ovito_cylinder_vert(cylinder_flat__vbo_picking
    ATTRIBS  ../attribs/vbo_picking.glsl
    GEOMETRY ../geometry/cylinder_flat_quad.glsl
)

ovito_cylinder_frag(cylinder_flat__color
    SURFACE  ../surface/cylinder_flat.glsl
    FRAGOUT  ../../particles/fragout/color_flat.glsl
)
ovito_cylinder_frag(cylinder_flat__oit_accum
    SURFACE  ../surface/cylinder_flat.glsl
    FRAGOUT  ../../particles/fragout/oit_accum.glsl
)
ovito_cylinder_frag(cylinder_flat__oit_reveal
    SURFACE  ../surface/cylinder_flat.glsl
    FRAGOUT  ../../particles/fragout/oit_reveal.glsl
)
ovito_cylinder_frag(cylinder_flat__picking
    SURFACE  ../surface/cylinder_flat.glsl
    FRAGOUT  ../../particles/fragout/picking.glsl
)
ovito_cylinder_frag(cylinder_flat__depth_only
    SURFACE  ../surface/cylinder_flat.glsl
    FRAGOUT  ../../particles/fragout/depth_only.glsl
)

# ---- Arrow head raycast (NormalShading cone head) ----

ovito_cylinder_vert(arrow_head_raycast__vbo_visual
    ATTRIBS  ../attribs/vbo_visual_rgb.glsl
    GEOMETRY ../geometry/arrow_head_raycast_bbox.glsl
)
ovito_cylinder_vert(arrow_head_raycast__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted.glsl
    GEOMETRY ../geometry/arrow_head_raycast_bbox.glsl
)
ovito_cylinder_vert(arrow_head_raycast__vbo_picking
    ATTRIBS  ../attribs/vbo_picking.glsl
    GEOMETRY ../geometry/arrow_head_raycast_bbox.glsl
)

ovito_cylinder_frag(arrow_head_raycast__color
    SURFACE  ../surface/arrow_head_raycast.glsl
    FRAGOUT  ../../particles/fragout/color.glsl
)
ovito_cylinder_frag(arrow_head_raycast__oit_accum
    SURFACE  ../surface/arrow_head_raycast.glsl
    FRAGOUT  ../../particles/fragout/oit_accum.glsl
)
ovito_cylinder_frag(arrow_head_raycast__oit_reveal
    SURFACE  ../surface/arrow_head_raycast.glsl
    FRAGOUT  ../../particles/fragout/oit_reveal.glsl
)
ovito_cylinder_frag(arrow_head_raycast__picking
    SURFACE  ../surface/arrow_head_raycast.glsl
    FRAGOUT  ../../particles/fragout/picking.glsl
)
ovito_cylinder_frag(arrow_head_raycast__depth_only
    SURFACE  ../surface/arrow_head_raycast.glsl
    FRAGOUT  ../../particles/fragout/depth_only.glsl
)

# ---- Arrow tail raycast (NormalShading cylinder tail) ----

ovito_cylinder_vert(arrow_tail_raycast__vbo_visual
    ATTRIBS  ../attribs/vbo_visual_rgb.glsl
    GEOMETRY ../geometry/arrow_tail_raycast_bbox.glsl
)
ovito_cylinder_vert(arrow_tail_raycast__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted.glsl
    GEOMETRY ../geometry/arrow_tail_raycast_bbox.glsl
)
ovito_cylinder_vert(arrow_tail_raycast__vbo_picking
    ATTRIBS  ../attribs/vbo_picking.glsl
    GEOMETRY ../geometry/arrow_tail_raycast_bbox.glsl
)

ovito_cylinder_frag(arrow_tail_raycast__color
    SURFACE  ../surface/arrow_tail_raycast.glsl
    FRAGOUT  ../../particles/fragout/color.glsl
)
ovito_cylinder_frag(arrow_tail_raycast__oit_accum
    SURFACE  ../surface/arrow_tail_raycast.glsl
    FRAGOUT  ../../particles/fragout/oit_accum.glsl
)
ovito_cylinder_frag(arrow_tail_raycast__oit_reveal
    SURFACE  ../surface/arrow_tail_raycast.glsl
    FRAGOUT  ../../particles/fragout/oit_reveal.glsl
)
ovito_cylinder_frag(arrow_tail_raycast__picking
    SURFACE  ../surface/arrow_tail_raycast.glsl
    FRAGOUT  ../../particles/fragout/picking.glsl
)
ovito_cylinder_frag(arrow_tail_raycast__depth_only
    SURFACE  ../surface/arrow_tail_raycast.glsl
    FRAGOUT  ../../particles/fragout/depth_only.glsl
)

# ---- Arrow flat billboard (FlatShading, ArrowShape) ----

ovito_cylinder_vert(arrow_flat__vbo_visual
    ATTRIBS  ../attribs/vbo_visual_rgb.glsl
    GEOMETRY ../geometry/arrow_flat_triangles.glsl
)
ovito_cylinder_vert(arrow_flat__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted.glsl
    GEOMETRY ../geometry/arrow_flat_triangles.glsl
)
ovito_cylinder_vert(arrow_flat__vbo_picking
    ATTRIBS  ../attribs/vbo_picking.glsl
    GEOMETRY ../geometry/arrow_flat_triangles.glsl
)

ovito_cylinder_frag(arrow_flat__color
    SURFACE  ../surface/arrow_flat.glsl
    FRAGOUT  ../../particles/fragout/color_flat.glsl
)
ovito_cylinder_frag(arrow_flat__oit_accum
    SURFACE  ../surface/arrow_flat.glsl
    FRAGOUT  ../../particles/fragout/oit_accum.glsl
)
ovito_cylinder_frag(arrow_flat__oit_reveal
    SURFACE  ../surface/arrow_flat.glsl
    FRAGOUT  ../../particles/fragout/oit_reveal.glsl
)
ovito_cylinder_frag(arrow_flat__picking
    SURFACE  ../surface/arrow_flat.glsl
    FRAGOUT  ../../particles/fragout/picking.glsl
)
ovito_cylinder_frag(arrow_flat__depth_only
    SURFACE  ../surface/arrow_flat.glsl
    FRAGOUT  ../../particles/fragout/depth_only.glsl
)
