# TODO List

# Current

- [x] Implement VulkanBindingGroupManager
- [ ] Move RenderState logic creation into ResourceFactory
- [ ] Bind everything into IRenderPass...
- [ ] Refactor TextureSystem to use handle
- [ ] Refactor ShaderSystem to use handle
- [x] Refactor MeshSystem to use handle

## Engine

### General

- [ ] Asset UUID => Currently UUID is usable at runtime (it uniquely identify an INSTANCE)
  but it can not be used to reference an asset. We will need this kind of system

### Rendering

- [ ] Refactor the rendering engine to improve simplicity
    - [x] Material refactor => Remove too much tying with RenderState and RenderPass
    - [ ] RenderPass refactor
    - [ ] RenderState refactor
    - [ ] RenderGraph refactor
    - [ ] Renderer creation
- [ ] Normal mapping
