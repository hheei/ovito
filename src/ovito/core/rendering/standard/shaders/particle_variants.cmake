# Particle shader variant table.
#
# Vertex shaders: ovito_particle_vert(NAME ATTRIBS <file> GEOMETRY <file>)
#   NAME     - stub filename stem (e.g. "sphere_raycast__vbo_visual")
#   ATTRIBS  - relative path from variants/ to the attribs snippet
#   GEOMETRY - relative path from variants/ to the geometry snippet
#
# Fragment shaders: ovito_particle_frag(NAME SURFACE <file> FRAGOUT <file>)
#   NAME    - stub filename stem (e.g. "sphere_raycast__color")
#   SURFACE - relative path from variants/ to the surface snippet
#   FRAGOUT - relative path from variants/ to the fragout snippet
#
# All paths are relative to shaders/particles/variants/ (one level up = shaders/particles/).

# ---- Raycast sphere ----

# Vertex shaders: 3 attribute sources (VBO visual, SSBO sorted, VBO picking)
ovito_particle_vert(sphere_raycast__vbo_visual
    ATTRIBS  ../attribs/vbo_spherical.glsl
    GEOMETRY ../geometry/sphere_raycast_quad.glsl
)
ovito_particle_vert(sphere_raycast__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted_spherical.glsl
    GEOMETRY ../geometry/sphere_raycast_quad.glsl
)
ovito_particle_vert(sphere_raycast__vbo_picking
    ATTRIBS  ../attribs/vbo_picking_spherical.glsl
    GEOMETRY ../geometry/sphere_raycast_quad.glsl
)

# Fragment shaders: 5 output targets
ovito_particle_frag(sphere_raycast__color
    SURFACE ../surface/sphere_raycast.glsl
    FRAGOUT ../fragout/color.glsl
)
ovito_particle_frag(sphere_raycast__oit_accum
    SURFACE ../surface/sphere_raycast.glsl
    FRAGOUT ../fragout/oit_accum.glsl
)
ovito_particle_frag(sphere_raycast__oit_reveal
    SURFACE ../surface/sphere_raycast.glsl
    FRAGOUT ../fragout/oit_reveal.glsl
)
ovito_particle_frag(sphere_raycast__picking
    SURFACE ../surface/sphere_raycast.glsl
    FRAGOUT ../fragout/picking.glsl
)
ovito_particle_frag(sphere_raycast__depth_only
    SURFACE ../surface/sphere_raycast.glsl
    FRAGOUT ../fragout/depth_only.glsl
)

# ---- Imposter sphere with depth correction (medium-quality) ----

ovito_particle_vert(sphere_imposter_wd__vbo_visual
    ATTRIBS  ../attribs/vbo_spherical.glsl
    GEOMETRY ../geometry/sphere_imposter_with_depth_quad.glsl
)
ovito_particle_vert(sphere_imposter_wd__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted_spherical.glsl
    GEOMETRY ../geometry/sphere_imposter_with_depth_quad.glsl
)
ovito_particle_vert(sphere_imposter_wd__vbo_picking
    ATTRIBS  ../attribs/vbo_picking_spherical.glsl
    GEOMETRY ../geometry/sphere_imposter_with_depth_quad.glsl
)

ovito_particle_frag(sphere_imposter_wd__color
    SURFACE ../surface/sphere_imposter_with_depth.glsl
    FRAGOUT ../fragout/color.glsl
)
ovito_particle_frag(sphere_imposter_wd__oit_accum
    SURFACE ../surface/sphere_imposter_with_depth.glsl
    FRAGOUT ../fragout/oit_accum.glsl
)
ovito_particle_frag(sphere_imposter_wd__oit_reveal
    SURFACE ../surface/sphere_imposter_with_depth.glsl
    FRAGOUT ../fragout/oit_reveal.glsl
)
ovito_particle_frag(sphere_imposter_wd__picking
    SURFACE ../surface/sphere_imposter_with_depth.glsl
    FRAGOUT ../fragout/picking.glsl
)
ovito_particle_frag(sphere_imposter_wd__depth_only
    SURFACE ../surface/sphere_imposter_with_depth.glsl
    FRAGOUT ../fragout/depth_only.glsl
)

# ---- Imposter sphere without depth correction (low-quality) ----

ovito_particle_vert(sphere_imposter_nd__vbo_visual
    ATTRIBS  ../attribs/vbo_spherical.glsl
    GEOMETRY ../geometry/sphere_imposter_no_depth_quad.glsl
)
ovito_particle_vert(sphere_imposter_nd__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted_spherical.glsl
    GEOMETRY ../geometry/sphere_imposter_no_depth_quad.glsl
)
ovito_particle_vert(sphere_imposter_nd__vbo_picking
    ATTRIBS  ../attribs/vbo_picking_spherical.glsl
    GEOMETRY ../geometry/sphere_imposter_no_depth_quad.glsl
)

# Color pass for no-depth, shaded imposter.
ovito_particle_frag(sphere_imposter_nd__color
    SURFACE ../surface/sphere_imposter_no_depth.glsl
    FRAGOUT ../fragout/color.glsl
)
ovito_particle_frag(sphere_imposter_nd__oit_accum
    SURFACE ../surface/sphere_imposter_no_depth.glsl
    FRAGOUT ../fragout/oit_accum.glsl
)
ovito_particle_frag(sphere_imposter_nd__oit_reveal
    SURFACE ../surface/sphere_imposter_no_depth.glsl
    FRAGOUT ../fragout/oit_reveal.glsl
)
ovito_particle_frag(sphere_imposter_nd__picking
    SURFACE ../surface/sphere_imposter_no_depth.glsl
    FRAGOUT ../fragout/picking.glsl
)
ovito_particle_frag(sphere_imposter_nd__depth_only
    SURFACE ../surface/sphere_imposter_no_depth.glsl
    FRAGOUT ../fragout/depth_only.glsl
)

# ---- Imposter flat-shaded ----

# Color pass for no-depth, flat imposter.
ovito_particle_frag(sphere_imposter_flat__color
    SURFACE ../surface/sphere_imposter_no_depth.glsl
    FRAGOUT ../fragout/color_flat.glsl
)
ovito_particle_frag(sphere_imposter_flat__oit_accum
    SURFACE ../surface/sphere_imposter_no_depth.glsl
    FRAGOUT ../fragout/oit_accum_flat.glsl
)
ovito_particle_frag(sphere_imposter_flat__oit_reveal
    SURFACE ../surface/sphere_imposter_no_depth.glsl
    FRAGOUT ../fragout/oit_reveal.glsl
)

# ---- Cube mesh (SquareCubicShape + NormalShading) ----
# Reuses sphere attribs (no orientation/asphericalShapes needed for uniform cube).

ovito_particle_vert(cube_mesh__vbo_visual
    ATTRIBS  ../attribs/vbo_spherical.glsl
    GEOMETRY ../geometry/box_oriented_mesh.glsl
)
ovito_particle_vert(cube_mesh__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted_spherical.glsl
    GEOMETRY ../geometry/box_oriented_mesh.glsl
)
ovito_particle_vert(cube_mesh__vbo_picking
    ATTRIBS  ../attribs/vbo_picking_spherical.glsl
    GEOMETRY ../geometry/box_oriented_mesh.glsl
)

ovito_particle_frag(cube_mesh__color
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/color.glsl
)
ovito_particle_frag(cube_mesh__oit_accum
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/oit_accum.glsl
)
ovito_particle_frag(cube_mesh__oit_reveal
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/oit_reveal.glsl
)
ovito_particle_frag(cube_mesh__picking
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/picking.glsl
)
ovito_particle_frag(cube_mesh__depth_only
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/depth_only.glsl
)

# ---- Square billboard (SquareCubicShape + FlatShading) ----
# Screen-aligned quad, no circle discard, flat (unlit) output.

ovito_particle_vert(square_billboard__vbo_visual
    ATTRIBS  ../attribs/vbo_spherical.glsl
    GEOMETRY ../geometry/square_billboard_quad.glsl
)
ovito_particle_vert(square_billboard__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted_spherical.glsl
    GEOMETRY ../geometry/square_billboard_quad.glsl
)
ovito_particle_vert(square_billboard__vbo_picking
    ATTRIBS  ../attribs/vbo_picking_spherical.glsl
    GEOMETRY ../geometry/square_billboard_quad.glsl
)

ovito_particle_frag(square_billboard__color
    SURFACE ../surface/square_billboard.glsl
    FRAGOUT ../fragout/color_flat.glsl
)
ovito_particle_frag(square_billboard__oit_accum
    SURFACE ../surface/square_billboard.glsl
    FRAGOUT ../fragout/oit_accum.glsl
)
ovito_particle_frag(square_billboard__oit_reveal
    SURFACE ../surface/square_billboard.glsl
    FRAGOUT ../fragout/oit_reveal.glsl
)
ovito_particle_frag(square_billboard__picking
    SURFACE ../surface/square_billboard.glsl
    FRAGOUT ../fragout/picking.glsl
)
ovito_particle_frag(square_billboard__depth_only
    SURFACE ../surface/square_billboard.glsl
    FRAGOUT ../fragout/depth_only.glsl
)

# ---- Oriented box mesh (BoxShape + NormalShading) ----

ovito_particle_vert(box_mesh__vbo_visual
    ATTRIBS  ../attribs/vbo_oriented.glsl
    GEOMETRY ../geometry/box_oriented_mesh.glsl
)
ovito_particle_vert(box_mesh__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted_oriented.glsl
    GEOMETRY ../geometry/box_oriented_mesh.glsl
)
ovito_particle_vert(box_mesh__vbo_picking
    ATTRIBS  ../attribs/vbo_picking_oriented.glsl
    GEOMETRY ../geometry/box_oriented_mesh.glsl
)

ovito_particle_frag(box_mesh__color
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/color.glsl
)
ovito_particle_frag(box_mesh__oit_accum
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/oit_accum.glsl
)
ovito_particle_frag(box_mesh__oit_reveal
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/oit_reveal.glsl
)
ovito_particle_frag(box_mesh__picking
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/picking.glsl
)
ovito_particle_frag(box_mesh__depth_only
    SURFACE ../surface/mesh_surface.glsl
    FRAGOUT ../fragout/depth_only.glsl
)

# ---- Ellipsoid raycast (EllipsoidShape) ----

ovito_particle_vert(ellipsoid_raycast__vbo_visual
    ATTRIBS  ../attribs/vbo_oriented.glsl
    GEOMETRY ../geometry/ellipsoid_raycast_box.glsl
)
ovito_particle_vert(ellipsoid_raycast__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted_oriented.glsl
    GEOMETRY ../geometry/ellipsoid_raycast_box.glsl
)
ovito_particle_vert(ellipsoid_raycast__vbo_picking
    ATTRIBS  ../attribs/vbo_picking_oriented.glsl
    GEOMETRY ../geometry/ellipsoid_raycast_box.glsl
)

ovito_particle_frag(ellipsoid_raycast__color
    SURFACE ../surface/ellipsoid_raycast.glsl
    FRAGOUT ../fragout/color.glsl
)
ovito_particle_frag(ellipsoid_raycast__oit_accum
    SURFACE ../surface/ellipsoid_raycast.glsl
    FRAGOUT ../fragout/oit_accum.glsl
)
ovito_particle_frag(ellipsoid_raycast__oit_reveal
    SURFACE ../surface/ellipsoid_raycast.glsl
    FRAGOUT ../fragout/oit_reveal.glsl
)
ovito_particle_frag(ellipsoid_raycast__picking
    SURFACE ../surface/ellipsoid_raycast.glsl
    FRAGOUT ../fragout/picking.glsl
)
ovito_particle_frag(ellipsoid_raycast__depth_only
    SURFACE ../surface/ellipsoid_raycast.glsl
    FRAGOUT ../fragout/depth_only.glsl
)

# ---- Superquadric raycast (SuperquadricShape) ----

ovito_particle_vert(superquadric_raycast__vbo_visual
    ATTRIBS  ../attribs/vbo_superquadric.glsl
    GEOMETRY ../geometry/superquadric_raycast_box.glsl
)
ovito_particle_vert(superquadric_raycast__ssbo_sorted
    ATTRIBS  ../attribs/ssbo_sorted_superquadric.glsl
    GEOMETRY ../geometry/superquadric_raycast_box.glsl
)
ovito_particle_vert(superquadric_raycast__vbo_picking
    ATTRIBS  ../attribs/vbo_picking_superquadric.glsl
    GEOMETRY ../geometry/superquadric_raycast_box.glsl
)

ovito_particle_frag(superquadric_raycast__color
    SURFACE ../surface/superquadric_raycast.glsl
    FRAGOUT ../fragout/color.glsl
)
ovito_particle_frag(superquadric_raycast__oit_accum
    SURFACE ../surface/superquadric_raycast.glsl
    FRAGOUT ../fragout/oit_accum.glsl
)
ovito_particle_frag(superquadric_raycast__oit_reveal
    SURFACE ../surface/superquadric_raycast.glsl
    FRAGOUT ../fragout/oit_reveal.glsl
)
ovito_particle_frag(superquadric_raycast__picking
    SURFACE ../surface/superquadric_raycast.glsl
    FRAGOUT ../fragout/picking.glsl
)
ovito_particle_frag(superquadric_raycast__depth_only
    SURFACE ../surface/superquadric_raycast.glsl
    FRAGOUT ../fragout/depth_only.glsl
)
