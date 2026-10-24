# GPU resource lifetime

How a GPU resource (today, a mesh) is created, drawn, released and freed while the main thread, the render thread
and the GPU all run at different points in time.

## The problem

- The **main thread** creates and releases meshes (through the asset system) and builds a `FramePacket` each frame.
- The **render thread** records packets into command buffers, one frame behind the main thread at most
  `kFramePacketCount` (2) packets.
- The **GPU** executes those command buffers up to `kMaxFramesInFlight` (3) frames later.

So when the main thread releases a mesh, its buffers can't be freed yet: a submitted frame may still be reading them,
and a packet that was built but not yet recorded may still name them. On top of that, both threads read the mesh
table, so it can't be resized or rewritten while the other thread is looking.

## The pieces

| Type | What it is |
|---|---|
| `GPUHandle<Tag>` (`GPUMeshHandle`) | Index + generation. What the simulation and assets hold. 8 bytes; the tag makes a mesh handle and a texture handle different types. |
| `GPUSlotRef` | `{ PoolID, Index, Generation }`: a handle with its kind erased, so one list can carry slots of every pool. |
| `GPUResourcePool<T, Tag>` | Fixed-capacity slot table holding payloads of type `T`. A thin template over `GPUResourcePoolBase`, which holds all the logic (in `GPUResourcePool.cpp`). |
| `GPUResourceRegistry` | Knows every pool by `PoolID`, and moves release requests and freed slots between the threads through the packet. |
| `FramePacket::ReleasedSlots` / `RecycledSlots` | The two mailboxes: releases going main → render, freed slots coming back render → main. |
| `FramePump` | Hands packets from main to render in order. Its mutex is the only synchronization in the whole scheme. |
| `RenderManager` | Calls the registry at the right moments (`BeginFramePacket`, `RetireAndReleasePacket`). |
| `GeometryPool` | The mesh-specific owner: how to make a mesh (buffers, upload, address) plus a private `GPUResourcePool<MeshEntry, MeshTag>`. |
| `DeletionQueue` | Separate, simpler tool for one-off objects that have no handle, such as the uploader's staging buffers. |

### Pool vs registry

- A **pool** stores one kind of resource. It knows its slots, their generations, its free list, and which released
  slots are waiting for which timeline value. It knows nothing about packets or other pools.
- The **registry** is the post office between the threads. It knows which pools exist and routes each `GPUSlotRef`
  to the pool it came from. It never looks inside a slot.

Every pool calls `RegisterPool` on the registry when it's constructed and gets back its `PoolID`, which is just its
index in the registry's list. That ID is how a `GPUSlotRef` in a packet finds its way home.

## A mesh's life

| Step | Thread | What happens |
|---|---|---|
| 1. Create | main | `GeometryPool::CreateMesh` makes the buffers, starts the upload (without waiting), and calls `Insert`. The pool takes an index from its free list (or the next unused one), stores the payload, and returns a handle. |
| 2. Draw | render | The handle is in `packet.Meshes`. `GetMeshInfo` calls `Get`, which checks the handle against the slot and returns the payload. |
| 3. Release | main | `MeshAssetHandler::Unload` → `GeometryPool::ReleaseMesh` → `Release`. The handle is dead at once (`IsAlive` is false), but the payload is untouched. The pool hands a `GPUSlotRef` to `registry.QueueReleasedSlot`. |
| 4. Send | main | The next `BeginFramePacket` calls `registry.MoveReleasedSlotsIntoPacket`, which moves the queued releases into `packet.ReleasedSlots`. |
| 5. Retire | render | After that packet's frame is submitted with timeline value `V`, `registry.RetireReleasedSlots` calls each slot's pool's `RetireSlot`, which notes "free this once the GPU reaches `V`". |
| 6. Free | render | Every frame, `registry.FreeRetiredSlots(completed)`, where `completed` is `RenderDevice::GetCompletedTimelineValue`: each pool destroys the payloads whose `V` the GPU has reached (the buffers are freed here) and writes their slots into `packet.RecycledSlots`. |
| 7. Recycle | main | When that packet is next acquired for writing, `registry.ReturnRecycledSlotsToPools` calls each slot's pool's `RecycleSlot`, putting it back on the free list. |
| 8. Reuse | main | A later `Insert` can take the index again, with the generation one higher, so old handles stay dead. |

The same steps as slot states:

```
              Insert                Release                 RetireSlot
   Free ─────────────► Live ─────────────────► Released ───────────────► Retiring
    ▲        (main)              (main)       handle dead,      (render)  waiting for V
    │                                         payload alive                    │
    │ RecycleSlot (main)                                                       │ FreeRetiredSlots, GPU reached V (render)
    └──────────────────────── Recycled ◄───────────────────────────────────────┘
                     payload destroyed, index riding back in the packet
```

## Per-frame call order

Where each of those calls happens in a frame. Steps 1 to 3 happen whenever the asset system asks; the rest run every
frame whether or not anything was released.

```
Main thread (Application::Run)
  RenderManager::BeginFramePacket
    FramePump::AquireForWriting                         blocks while the render thread is a full pump behind
    GPUUploader::FreeCompletedStagingBuffers
    GPUResourceRegistry::ReturnRecycledSlotsToPools     step 7
    GPUResourceRegistry::MoveReleasedSlotsIntoPacket    step 4
  fill the packet (Meshes, Time, ...)
  RenderManager::SubmitFramePacket                      FramePump::Submit

Render thread (RenderManager::RenderThreadMain)
  RenderDevice::WaitFor(frame slot's RetireValue)
  FramePump::AquireForReading                           nullptr once closed: return
  acquire the swapchain image, Renderer::Render, RenderDevice::Submit   → V
  RenderManager::RetireAndReleasePacket(packet, V)
    GPUResourceRegistry::RetireReleasedSlots(V)         step 5
    GPUResourceRegistry::FreeRetiredSlots(completed)    step 6
    FramePump::Release                                  the packet goes back to the main thread
  present
```

## Why the retire value is the submit that carried the release

A release made while the main thread builds frame N rides in packet N. The render thread records packets in order and
submits them in order on one queue, so:

- every earlier packet that could still name the mesh was submitted before packet N, and
- packet N itself may still name it, but it's recorded before the release is retired.

When the timeline reaches packet N's value, every earlier submission on the queue has finished too. So nothing the
GPU is running, or will ever run, can reference the mesh.

The obvious alternative, "the value last submitted when `Release` was called", is wrong here: it misses packets that
were built but not yet recorded, which would then draw a freed mesh. The vertex buffer is read by device address, so
validation wouldn't catch it.

## Who touches what

| State | Owner |
|---|---|
| Handle state (generation, live flag), free list, registry's pending releases | Main thread only |
| A slot's payload and generation copy | Main writes it in `Insert`, before the handle is ever in a packet. Render reads it, and destroys it in `FreeRetiredSlots`. The index goes back to main through the packet. One writer at a time. |
| Each pool's retiring list | Render thread only |
| `ReleasedSlots` / `RecycledSlots` | Handed between threads by `FramePump` |

There are no locks. Each piece of state either belongs to one thread, or changes owner at a hand-off that the pump's
mutex already orders. Slot storage is sized once and never resized, so a render-thread read never sees a reallocation.

### Two generations

Each slot has two generation numbers, one per thread:

- **Main side** (`m_Handles[i].Generation`): bumped by `Release`, checked by `IsAlive` through `IsSlotAlive`. The
  simulation sees the handle die immediately.
- **Render side** (`m_SlotGenerations[i]`): copied in `Insert`, checked by `Get` and `RetireSlot` through
  `SlotHoldsGeneration`. The render thread still sees the mesh as valid until `FreeRetiredSlots` destroys it.

If both threads shared one number, the main thread bumping it would race the render thread reading it while it records
a packet that still draws the mesh.

## Edge cases

- **A packet that isn't drawn** (swapchain out of date): its releases still retire. The retire value comes from an
  empty submit, which orders after all earlier work on the queue.
- **Shutdown**: `JoinRenderThread` closes the pump, the render thread wakes with no packet and exits, then the device
  goes idle. When the pools are destroyed they free everything they still hold, including releases that never retired.
- **Pool full**: a fatal log and an abort, in every configuration (`kMaxMeshes` is in `RenderConstants.h`).
- **Staging buffers** aren't in a pool. `GPUUploader` keeps them in its own `DeletionQueue`, keyed on the upload's
  timeline value, and `FreeCompletedStagingBuffers` (called from `BeginFramePacket`) frees them once the GPU has
  finished the copy.

## Adding a new resource kind

1. A payload struct whose members free themselves (`GPUBuffer`, `vk::raii::*`), so destroying it frees everything.
2. A tag and handle alias in `GPUHandles.h`, e.g. `struct TextureTag {}; using GPUTextureHandle = GPUHandle<TextureTag>;`.
3. An owning class (e.g. `TexturePool`) with a private `GPUResourcePool<Payload, TextureTag>`, built in `RenderManager`
   with `m_ResourceRegistry`. It exposes create, release and alive checks, plus whatever view the renderer needs.

The packet, the registry and `RenderManager`'s frame loop don't change.

## Where the code is

| File | Contents |
|---|---|
| `Render/GPUHandles.h` | `GPUHandle<Tag>`, `GPUSlotRef` |
| `Render/GPUResourcePool.{h,cpp}` | `GPUResourcePoolBase`, `GPUResourcePool<T, Tag>` |
| `Render/GPUResourceRegistry.{h,cpp}` | The registry |
| `Render/FramePacket.h`, `Render/FramePump.{h,cpp}` | The packet mailboxes and the hand-off |
| `Render/RenderManager.cpp` | `BeginFramePacket`, `RenderThreadMain`, `RetireAndReleasePacket`, `JoinRenderThread` |
| `Render/GeometryPool.{h,cpp}` | The mesh owner |
| `Render/DeletionQueue.{h,cpp}`, `Render/GPUUploader.cpp` | Staging buffer lifetime |
