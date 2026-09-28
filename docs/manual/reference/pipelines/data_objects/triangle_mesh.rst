.. _scene_objects.triangle_mesh:

Triangle meshes
---------------

.. image:: /images/scene_objects/triangle_mesh_example.jpg
  :width: 30%
  :align: right

Triangle meshes are general polyhedral objects made of vertices connected by triangular faces.
Typically, they are imported as extra objects into the scene using the :menuselection:`File --> Load File`
function of OVITO. You can use triangle meshes, for example, to visualize additional geometry next to your simulation data. 
OVITO supports several common geometry file formats
such as STL, OBJ and VTK. See :ref:`this page <file_formats.input>` for a complete list. 

Either the faces or vertices of a triangle mesh may be associated with color information - if such 
information is present in the imported geometry file. Otherwise, the uniform coloring of the mesh is 
controlled by the user within OVITO. See also the :ref:`visual element for triangle meshes <visual_elements.triangle_mesh>` for more information.

Note that triangle meshes are also used in OVITO to :ref:`give particles a free-form shape <scene_objects.particle_types>`. 

.. seealso::

  :py:class:`ovito.data.TriangleMesh` (Python API)