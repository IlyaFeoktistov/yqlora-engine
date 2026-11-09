# Importing a model

[Русская версия](import-model.ru.md)

Models and pictures are project files, like scenes and scripts. The engine
reads the source file itself: export it from Blender again and the editor
reads it again, references in scenes stay.

## 1. Put the file into the project

Copy a `.glb` or `.gltf` (with its `.bin` and pictures next to it) into any
folder of the project, for example `assets/models/`. glTF 2.0 and PNG, JPG,
TGA, BMP pictures are supported. FBX and OBJ are not read: export to glTF
2.0 from Blender (`File > Export > glTF 2.0`).

Go back to the editor: it walks the project folder again when its window
gets focus. A `.meta` with the ID of the file appears next to it - keep it in
git together with the file.

## 2. See what is inside

In Files a model opens into its meshes, materials and textures. Clicking the model shows the number
of nodes in the inspector and the meshes with their vertex, triangle and
submesh counts (parts with different materials).

Every mesh has its own ID in the `.meta` of the model. It is made once and
follows the name of the mesh, so name the meshes in Blender: a mesh without
a name is known by its number in the file and gets a new ID after meshes
before it are removed.

## 3. Put it into the scene

Drag the model from Files:

- into the viewport - the model lands on the surface under the cursor or on
  the ground;
- onto a node of the scene tree - it becomes its child;
- onto the empty place under the tree - it becomes a child of the root.

The model appears as a node named after the file, with the nodes of the file
under it with their position, rotation and scale, and a `MeshRenderer` on
the nodes with a mesh, its slots filled with the materials of the file.
Doors with one mesh refer to one resource, the
geometry is not copied. Ctrl+Z removes the whole model.

A single mesh is dragged the same way, from the opened model or from the
mesh table in the inspector, and also onto the Mesh field of a
`MeshRenderer`. The same mesh is chosen in the list of the field too: it
holds the built-in meshes and the meshes of every model, by file.

## 4. Import settings

A model has a scale (set 0.01 if the file is in centimetres). A picture has
a colour space (sRGB for colour, Linear for normal and roughness maps), a
filter (Nearest for pixel art), wrapping and mipmaps. Change the values in
the inspector and press Apply: the settings are written to `.meta`, the file
is read again, and the scene shows the result at once. This is not in the
edit history.

## 5. Place it exactly

While moving a node:

- the magnet button (or Ctrl while dragging) - to the grid, the step is
  chosen next to it: 1 m, 0.5 m, 0.25 m, 0.1 m or a custom one;
- V held - the vertex closest to the cursor is dragged to a vertex of
  another mesh;
- Ctrl+Shift - the node lands on the surface under the cursor, and the
  button next to the magnet also turns it along the surface.

## If something is wrong

- **The model is not in Files**: switch to another program and back, or
  press Refresh in Files.
- **The inspector says Not loaded**: the error is next to it; details are in
  the Log panel. Primitives that are not triangles (lines, points) are
  skipped with a warning.
- **The meshes have no textures**: the pictures of a glTF file must lie
  where the file says (next to it) or inside the GLB; a missing one is in
  the Log. Textures go onto meshes through materials, see
  [your own shader, material and sky](shader-material-sky.md).
- **A material of the model does not change**: it is read from the file;
  "Make a copy" in its inspector gives a material file you can edit.
