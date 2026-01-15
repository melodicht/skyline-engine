#include <meta_definitions.h>
#include <scene.h>

/*
 * ENTITY FUNCTIONALITY
 */

std::unordered_map<std::type_index, ComponentID> typeToId;

/*
 * COMPONENT POOL
 */

ComponentPool::ComponentPool()
{
    elementSize = 0;
    pData = nullptr;
}

ComponentPool::ComponentPool(void *base, size_t elementsize)
{
    // We'll allocate enough memory to hold MAX_ENTITIES, each with element size
    elementSize = elementsize;
    pData = static_cast<u8 *>(base);
}

ComponentsPool InitComponentsPool(MemoryArena *remainingArena)
{
    ComponentsPool result = {};
    result.componentPools = PushArray(remainingArena, MAX_COMPONENTS, ComponentPool);
    return result;
}

ComponentPoolsBuffer::ComponentPoolsBuffer()
{
    this->base = nullptr;
    this->count = 0;
}

ComponentPoolsBuffer::ComponentPoolsBuffer(MemoryArena *remainingArena)
{
    this->base = PushArray(remainingArena, MAX_COMPONENTS, ComponentPool);
    this->count = 0;
}

ComponentPool *ComponentPoolsBuffer::operator[](u32 index)
{
    Assert(index < count);
    ComponentPool *result = base + index;
    return result;
}

void ComponentPoolsBuffer::Push(ComponentPool componentPool)
{
    Assert(count < MAX_COMPONENTS);
    ComponentPool *address = base + count;
    *address = componentPool;
    ++count;
}

/*
 * SCENE FUNCTIONALITY
 */

void PushSystemsBuffer(SystemsBuffer *systemsBuffer, System *system)
{
    Assert(systemsBuffer->count < MAX_SYSTEMS);
    System **systemAddress = systemsBuffer->base + systemsBuffer->count;
    *systemAddress = system;
    ++systemsBuffer->count;
}

void StartAllSystems(SystemsBuffer *systemsBuffer, Scene *scene)
{
    for (u32 i = 0; i < systemsBuffer->count; ++i)
    {
        System *system = systemsBuffer->base[i];
        system->OnStart(scene);
    }
}

void UpdateAllSystems(SystemsBuffer *systemsBuffer, Scene *scene, GameInput *input, f32 deltaTime)
{
    for (u32 i = 0; i < systemsBuffer->count; ++i)
    {
        System *system = systemsBuffer->base[i];
        system->OnUpdate(scene, input, deltaTime);
    }
}

// NOTE(marvin): Remaining arena decreases after each initialization.
Scene::Scene(MemoryArena *remainingArena)
{
    this->entities = InitEntitiesPool(remainingArena);
    this->freeIndices = InitFreeIndicesStack(remainingArena, MAX_ENTITIES);
    this->systemsBuffer = InitSystemsBuffer(remainingArena);
    this->systemsArena = SubArena(remainingArena, SYSTEMS_MEMORY);
    this->componentPools = ComponentPoolsBuffer(remainingArena);
    this->componentPoolsArena = SubArena(remainingArena, COMPONENT_POOLS_MEMORY);
}

Scene::~Scene()
{
    // TODO(marvin): The scene deconstructor doesn't free the memory arenas because the scene is presumed to exist for the entire lifetime of the program. But when we do have multiple scenes and can switch between them while the game is running, we could just zero out the entire permanent storage... no need for an explicit free. We'll see.
}

void Scene::AddSystem(System *system)
{
    PushSystemsBuffer(&systemsBuffer, system);
}

void Scene::InitSystems()
{
    StartAllSystems(&systemsBuffer, this);
}

void Scene::UpdateSystems(GameInput *input, f32 deltaTime)
{
    UpdateAllSystems(&systemsBuffer, this, input, deltaTime);
}

void Scene::AddComponentPool(size_t componentSize)
{
    void *base = PushSize(&componentPoolsArena, MAX_ENTITIES * componentSize);
    ComponentPool componentPool = ComponentPool(base, componentSize);
    componentPools.Push(componentPool);
}

EntityID Scene::NewEntity()
{
    if (!FreeIndicesStackIsEmpty(&freeIndices))
    {
        u32 newIndex = PopFreeIndicesStack(&freeIndices);
        EntityEntry *entityEntry = GetFromEntitiesPool(&entities, newIndex);
        ValidateEntityEntryWithIndex(entityEntry, newIndex);
        return entityEntry->id;
    }
    else
    {
        EntityEntry *entityEntry = AddNewEntityEntry(&entities);
        return entityEntry->id;
    }
}

EntityEntry &Scene::GetEntityEntry(EntityID id)
{
    u32 index = GetEntityIndex(id);
    EntityEntry *entityEntry = GetFromEntitiesPool(&entities, index);
    return *entityEntry;
}

void Scene::DestroyEntity(EntityID id)
{
    // TODO(marvin): Maybe there should be a structure that encapsulates the entities pool and the free indices stack?
    u32 index = GetEntityIndex(id);
    DestroyEntityEntryInEntitiesPool(&entities, index, id);
    PushFreeIndicesStack(&freeIndices, index);
}
