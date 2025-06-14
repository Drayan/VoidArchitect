# TODO List

# Current

- [x] Implement `VulkanBindingGroupManager`
- [x] Move `RenderState` logic creation into `ResourceFactory`
- [x] Bind everything into `IRenderPass`...
- [x] Refactor `TextureSystem` to use handle
- [x] Refactor `ShaderSystem` to use handle
- [x] Refactor `MeshSystem` to use handle
- [ ] Implement `RenderState` setup from file
- [ ] Allow mesh generator to use a `VertexFormat` input

## Engine

### General

- [ ] Asset UUID => Currently `UUID` is usable at runtime (it uniquely identify an INSTANCE)
  but it can not be used to reference an asset. We will need this kind of system
- [ ] Implement missing method on `Mat4` for `Transform`
- [ ] Implement base `GameObject` with a `Transform` and a simple `MeshComponent`

### Rendering

- [x] Refactor the rendering engine to improve simplicity
    - [x] `Material` refactor => Remove too much tying with `RenderState` and `RenderPass`
    - [x] `RenderPass` refactor
    - [x] `RenderState` refactor
    - [x] `RenderGraph` refactor
    - [x] Renderer creation
- [x] Normal mapping
