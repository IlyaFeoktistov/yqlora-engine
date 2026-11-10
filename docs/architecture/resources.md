# Resources

[Русская версия](resources.ru.md)

Files of a project that the game uses: scenes, models, pictures, shaders
and materials now, sounds with their step. How a model gets into a
scene is in the guide [importing a model](../guide/import-model.md). Where they lie and the file
formats are in [project and file formats](project.md).

Status marks: see [README](README.md).

## Resource IDs [done]

- A resource is referred to by a `ResourceId`, a 64-bit number, not by its
  path: a renamed or moved file keeps all references to it (the startup
  scene, the mesh of a `MeshRenderer`).
- The ID lives in a `.meta` file next to the resource (`level.yscene.meta`
  holds `{"id": "...", "version": 1}`), so it travels with the file in
  version control. A `.meta` without a format version or with a broken ID
  is not read, and the file gets a new ID.
- `ResourceDatabase` keeps both directions, ID to path and path to ID, all
  paths relative to the project folder.
- A new ID is 64 random bits, so two projects practically never give the
  same ID; it is always at least `builtin::kFirstProjectId`.
- **Built-in resources** (the cube, sphere and plane meshes, the built-in
  shaders and materials) have small constant IDs below `kFirstProjectId` and
  no file; a project ID never collides with them. They are one table,
  `builtinResourceTable()`: ID, kind, name and how to make the data; the
  cache makes them from it and the editor lists and names them from it.
- In a property the reference is a `ResourceRef`; 0 means "none".

## Keeping IDs right [done]

- `scan` walks the project folder when the project opens. A file without a
  `.meta` gets one with a new ID. If an ID repeats (a file copied together
  with its `.meta`), the copy gets a new one. Hidden files and the reserved
  names (`project.yqlora`, `settings/`) are not resources.
- The editor tells the database about its own changes: `add` for a new file,
  `moved` after a rename or a move (the `.meta` follows the file, IDs stay),
  `removed` after a delete (the `.meta` goes away, the ID is forgotten).
- Changes made outside the editor are picked up by the next `scan`.

## Meshes and textures from files [done]

### Decisions

- **The source file is the resource**: `.glb`,
  `.gltf`, `.png`, `.jpg` lie anywhere in the project and are seen in Files.
  The engine reads the source file itself; an import cache and a binary
  format come with step 9, between the source file and `ResourceCache`,
  without changing the resource interface.
- **Libraries**: cgltf (MIT) for glTF 2.0, stb_image (public domain or MIT)
  for PNG, JPG, TGA, BMP and HDR (`.hdr`), from a file and from memory (images inside a GLB
  later). No FBX (closed format) and no OBJ (glTF covers it).
- **Three levels, not mixed**: the source file, the loaded data in
  `ResourceCache` (`MeshData`, `TextureData`), the GPU copies in the graphics
  backend. Only the cache reads files, only the backend knows the GPU.
- **Loaders by extension.** A new kind of resource is a new loader, nothing
  else changes. The built-in meshes are in the same cache with their constant
  IDs; `findBuiltinMesh` goes away.
- **Loading is synchronous**, on first use. Background loading is later.
- **Not read from glTF**: skinning, animation, morph targets, cameras and
  lights, compressed textures (KTX2). Materials of glTF come in as nested
  resources (see [materials](materials.md#gltf-materials-done)).

### Nested resource IDs

A file may hold several resources (meshes of a `.glb`). Each one gets its
own `ResourceId`, so a `ResourceRef` stays a single 64-bit number.

```json
{
    "id": "9bf87a121dc2ad5c",
    "import": {"scale": 1.0},
    "subresources": {
        "mesh:Door": "4c1e07a95b3d2f60",
        "mesh:Door#1": "a07f3b9e21c8d415",
        "mesh:#2": "e51d90c4b7a2f386"
    },
    "version": 1
}
```

- A nested ID is 64 random bits, made once and kept in `.meta`, never
  computed from the name. The name is only the key that matches an element
  of the file to its entry.
- Key: kind, name and the number of the element among elements of the same
  name (`Door`, `Door#1`); an element without a name is keyed by its index
  in the file (`#2`), so removing an element before it gives it a new key.
  A key already given in the file gets the next number, so keys never repeat
  even with `#` in a name (`SubresourceKeys`).
- Re-export from Blender: elements whose key is found keep their IDs, new
  elements get new ones. The entry of an element removed from the file stays
  in `.meta`, references to it show "missing in file", and the ID comes back
  if the element comes back.
- Rename and move: `.meta` follows the file, all IDs stay. A copy (a repeated
  file ID found by `scan`) gets a new file ID and new nested IDs.
- `ResourceDatabase` knows, for every ID, the file and the key inside it.
- A loader names the kinds of nested resources its files may hold
  (`ResourceLoader::nestedKinds`, meshes for glTF): by them the editor knows
  which files to read to show nested resources, without reading the rest.
  The cache keeps the resources of a file in file order
  (`ResourceCache::resourcesOf`).
- `.meta` gets a format version (`FileFormat`), version 1.

### Cache and revisions

- A loaded resource has a revision. The GPU cache in the backend keys copies
  by ID and revision, so a new revision replaces the old copy, and the old
  one is released once nothing draws with it.
- A reload (the file changed on disk, or "Apply" of import settings) loads
  the new data first, then checks it, then replaces the entry and raises the
  revision. The old data is never destroyed before the new data is loaded.
- A failed load leaves the resource in the "error" state with its message:
  the old data is not shown as if it were current. A deleted file puts the
  resource in the "missing" state; references stay and work again when the
  file comes back.
- A reference to an empty, missing or broken resource draws nothing; the
  log gets one message per resource, the inspector shows the state.
- Data handed out by the cache is not kept across frames by its users:
  `FrameData` and picking take it anew every frame, so a reload never leaves
  a dangling pointer. Whoever builds something from data and remembers what
  from (a prefab instance) takes it with a share of ownership
  (`ResourceCache::share`): after a reload the old data lives while it is
  held.
- Scenes and prefabs are cache resources too: `SceneData` and `PrefabData`
  (the parsed tree in the file format); nodes are built from them by whoever
  places them (see [prefabs](prefabs.md#prefab-data-done)).

### Meshes

- `Vertex`: position, normal, `uv0`, tangent (from `TANGENT` when present;
  computed with MikkTSpace when a material of the mesh has a normal map).
- `MeshData` has submeshes (index ranges, one per glTF primitive) and bounds
  (AABB, for picking and culling). A submesh has a stable index: it is
  the material slot of a `MeshRenderer`, and it keeps the index of the glTF
  material of its primitive (or "none") and its name, the name of the slot.
- The glTF loader reads data through cgltf accessors, so interleaved
  buffers, sparse accessors, normalized integers, external buffers, GLB
  chunks, missing indices and all component types work. Only the TRIANGLES
  mode is accepted, others are rejected with a message. Missing normals are
  computed smooth, by summing face normals over shared vertices; vertices
  that the file splits stay split.
- The model description keeps the node tree of the file with local
  transforms and references to meshes. One mesh used by several nodes is one
  resource referenced several times, never copied.
- Import setting: scale.

### Textures

- `TextureData`: RGBA, width, height, `PixelFormat`: `Rgba8`, or
  `Rgba16Float` for HDR pictures (half floats, always linear, may exceed
  1: sky panoramas). `displayPixels` gives RGBA8 in sRGB for previews and
  the window icon. Import settings: color space (sRGB
  for color, Linear for data: normal, roughness, metallic), filter, wrap,
  mipmaps. The settings are a record, so new ones are added without changing
  the loader.
- Textures go on meshes only through materials; a texture is also the
  game icon (General section) and a sky panorama.

### Phases of step 4 [done]

0. Step 3 leftovers. A game module registers only scripts, so its reload
   has no systems or settings sections to recreate: the contract is written
   down, and recreating them moves to [plugins](plugins.md), whose modules
   will register them **[done]**.
1. `ResourceCache` and the loader interface, the built-in meshes in it;
   `FrameBuilder` and picking take meshes from the cache; `findBuiltinMesh`
   goes away; `uv0`, submeshes, AABB; the GPU cache by ID and revision.
   Nothing changes from the outside **[done]**. The cache belongs to
   `EngineServices` (`services.resources()`), and opening a project there
   hands the project to the cache too; the Play copy has its own input but
   shares the cache, so files are not read twice. The GPU cache is one
   template for every backend and resource (`GpuResourceCache`): a copy not
   drawn for 120 frames is released.
2. Nested IDs and the `.meta` format (version, `import`, `subresources`)
   **[done]**. `ResourceDatabase::assignSubresources` gives the IDs of the
   keys a loader returned and writes `.meta` once; `fileOf`, `key`,
   `subresources`; import settings are a JSON object kept as text
   (`importSettings`, `setImportSettings`) and reach the loader in
   `LoadRequest::import`. The `.meta` format itself (reading, atomic
   writing through `platform::writeFileAtomically`) is `ResourceMeta`;
   `ResourceDatabase` is only the index of IDs and paths.
   Loading only reads `.meta` (`MetaPolicy::ReadOnly`, the default): a
   built game does not change the files of its project, and a nested
   resource without an ID in `.meta` is skipped with a message. The editor
   imports (`MetaPolicy::Import`, set by `EditorSession`): new nested
   resources get IDs, the fields a loader writes (`LoadResult::importUpdate`,
   the shader profile version) go into the import settings, and `.meta` is
   written. That is why the cache holds the project as changeable.
3. glTF meshes: the loader, the model description, scale **[done]**.
   `GltfLoader` (`yqlora/import/`) gives `ModelData` for the file (nodes
   with local position, rotation, scale, a mesh index and children; the
   keys of the meshes) and one `MeshData` per mesh of the file. The engine
   loaders are added by `addEngineLoaders` when `EngineServices` is made.
   cgltf reads external buffers through a read function of the engine, so
   paths outside ASCII work on Windows too. External buffer files are
   dependencies of the model (`LoadResult::dependencies`, also when one is
   missing): a changed or added `.bin` reloads it. The loader is split by
   files: `GltfDocument` (parsing, buffers, validation), `readGltfMesh`,
   `readGltfNode` and `gltfRoots`; `GltfLoader` puts them together.
4. Textures: the loader, import settings, the GPU texture, the game icon
   **[done]**. `ImageLoader` (stb_image, the engine reads the bytes itself)
   gives `TextureData`; a loader also names the kind of its file
   (`ResourceLoader::kind`, `ResourceCache::kindOf`), so the editor lists
   textures without reading files. Import settings and their names live in
   `TextureImport`. `GlTexture` takes rows from the top, so `uv0 = (0, 0)` is
   the top left corner with no flip. `Renderer::uiTexture` shows a texture
   in the interface with the colours of the file (no sRGB decoding, ImGui
   draws into a plain buffer); the icon field in General shows a preview.
5. Editor: icons in Files; the inspector of a resource file (meshes with
   vertex, triangle and submesh counts; texture preview and size; import
   settings with "Apply"); project meshes in the `MeshRenderer` list grouped
   by file; dragging a model from Files into the tree or the viewport creates
   its node tree in one undo step; dragging a mesh onto the mesh field;
   changed files are reloaded when the editor window gets focus **[done]**.
   See [editor](editor.md), "Resources in the editor".
6. Dragging nodes in the tree with "Keep the world position?" (see
   [scene](scene.md)), one undo step for parent and transform **[done]**.
   A node dropped onto another becomes its last child, onto the empty place
   under the tree - the last child of the root; one node at a time.
7. Snapping while moving: to the grid, to vertices, onto a surface, with
   viewport buttons and keys (see [editor](editor.md)) **[done]**.
8. Documents: this one, project (`.meta`), rendering (vertex format,
   submeshes), editor, the guide "Importing a model", README **[done]**.

Tests, besides unit tests of each part:

- 1: a reload raises the revision; the old GPU copy is released; no
  dangling data after a reload (asan).
- 2: elements with the same name; rename keeps IDs; a copy gets new IDs;
  re-export keeps IDs; a removed element keeps its entry.
- 3: interleaved buffers; GLB with several meshes; one mesh used by several
  nodes; missing normals; broken data and a non-triangle mode give an error,
  not a crash.
- 4: PNG with alpha; JPG; sRGB and Linear; applying import settings again
  raises the revision.
- 5: a model with nested nodes into a scene; re-export keeps references;
  save and reopen the scene; delete the source file while references exist.

## Shaders and materials from files [done]

The formats and the rules of values are in
[materials](materials.md#shader-files-done); here is how they are resources.

- **Loaders**: `ShaderLoader` (`.yshader`) compiles the file into
  `ShaderData`, `MaterialLoader` (`.ymat`) reads `MaterialData`. A material
  refers to its shader and textures by ID: a renamed shader or texture
  keeps its materials, a changed one gets a new revision and the material
  takes it at the next draw (pipelines are keyed by shader revision).
- **Dependencies**: `LoadResult::dependencies` lists the files a loader
  read besides the resource file, also on error (includes of a shader,
  images next to a glTF model, external buffers). `reloadChanged` looks at
  them too: a changed include recompiles every shader using it.
- **Fields written on import**: `LoadResult::importUpdate` puts what a
  loader learned into the import settings of `.meta` (the profile version
  of a shader), only in the editor (`MetaPolicy::Import`).
- **References inside a file**: a nested resource may refer to another
  nested resource of the same file (a material of a model to its texture).
  The loader does not know their IDs, so it names the key
  (`LoadedResource::links`, `NestedLink`), and the cache writes the IDs in
  before the data is published. Without an ID in `.meta` (read-only
  `.meta`) the reference stays empty.
- **Built-in** shaders (`Standard`, `Error`, the sky shaders) and materials
  (default, error, the default sky and the gradient sky) are rows of
  `builtinResourceTable()` like the meshes, made on first use.
