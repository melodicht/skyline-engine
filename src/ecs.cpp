/*
 * ENTITY FUNCTIONALITY
 */

inline EntityID CreateEntityId(u32 index, u32 version)
{
    // Shift the index up 32, and put the version in the bottom
    return ((EntityID) index << 32) | ((EntityID) version);
}

inline EntityID InvalidateEntityId(EntityID id)
{
    return CreateEntityId(INVALID_ENTITY_ID, GetEntityVersion(id) + 1);
}

inline u32 GetEntityIndex(EntityID id)
{
    // Shift down 32 so we lose the version and get our index
    return id >> 32;
}

inline u32 GetEntityVersion(EntityID id)
{
    // Cast to a 32 bit int to get our version number (losing the top 32 bits)
    return (u32) id;
}

inline bool IsEntityValid(EntityID id)
{
    // Check if the index is our invalid index
    return (id >> 32) != (u32) (-1);
}

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

inline void *ComponentPool::get(size_t index)
{
    // looking up the component at the desired index
    return pData + index * elementSize;
}

inline EntityID ComponentPool::getOwner(u8 *ptr)
{
    return ((size_t)(ptr - pData)) / elementSize;
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

// NOTE(marvin): Remaining arena decreases after each initialization,
// and the components arena takes all the rest, as that's the least
// predictable and most memory-consuming.
Scene::Scene(MemoryArena *remainingArena)
{
    this->entities = InitEntitiesPool(remainingArena);
    this->freeIndices = InitFreeIndicesStack(remainingArena);
    this->systemsBuffer = InitSystemsBuffer(remainingArena);
    this->systemsArena = SubArena(remainingArena, SYSTEMS_MEMORY);
    this->componentPools = ComponentPoolsBuffer(remainingArena);
    this->componentPoolsArena = *remainingArena;

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

// Helps with iterating through a given scene
template<typename... ComponentTypes>
struct SceneView
{
    SceneView(Scene &scene) : pScene(&scene)
    {
        if (sizeof...(ComponentTypes) == 0)
        {
            all = true;
        }
        else
        {
            // Unpack the template parameters into an initializer list
            u32 componentIds[] = {0, GetComponentId<ComponentTypes>()...};
            for (u32 i = 1; i < (sizeof...(ComponentTypes) + 1); i++)
                componentMask.set(componentIds[i]);
        }
    }

    struct Iterator
    {
        Iterator(Scene *pScene, u32 index, ComponentMask mask, bool all)
                : pScene(pScene), index(index), mask(mask), all(all) {}

        // give back the entityID we're currently at
        EntityID operator*() const
        {
            EntityEntry *entityEntry = GetFromEntitiesPool(&pScene->entities, index);
            return entityEntry->id;
        }

        // Compare two iterators
        bool operator==(const Iterator &other) const
        {
            return index == other.index || index == GetEntitiesPoolSize(&pScene->entities);
        }

        bool operator!=(const Iterator &other) const
        {
            return index != other.index && index != GetEntitiesPoolSize(&pScene->entities);
        }

        bool ValidIndex()
        {
            EntityEntry *entityEntry = GetFromEntitiesPool(&pScene->entities, index);
            return
                    // It's a valid entity ID
                    EntityEntryValid(entityEntry) &&
                    // It has the correct component mask
                    (all || mask == (mask & entityEntry->mask));
        }

        // Move the iterator forward
        Iterator &operator++()
        {
            do
            {
                index++;
            } while (index < GetEntitiesPoolSize(&pScene->entities) && !ValidIndex());
            return *this;
        }

        u32 index;
        Scene *pScene;
        ComponentMask mask;
        bool all{false};
    };

    // Give an iterator to the beginning of this view
    const Iterator begin() const
    {
        u32 firstIndex = 0;
        EntityEntry *entityEntry = GetFromEntitiesPool(&pScene->entities, firstIndex);
        while (firstIndex < GetEntitiesPoolSize(&pScene->entities) &&
               (componentMask != (componentMask & entityEntry->mask)
                || !IsEntityValid(entityEntry->id)))
        {
            firstIndex++;
            entityEntry = GetFromEntitiesPool(&pScene->entities, firstIndex);
        }
        return Iterator(pScene, firstIndex, componentMask, all);
    }

    // Give an iterator to the end of this view
    const Iterator end() const
    {
        return Iterator(pScene, (u32) GetEntitiesPoolSize(&pScene->entities), componentMask, all);
    }

    Scene *pScene{nullptr};
    ComponentMask componentMask;
    bool all{false};
};

// Iterates through the components of a given entity.
// NOTE(marvin): Could use GetNumberOfDefinedComponents() instead of MAX_COMPONENTS. Playing the safe route.
struct EntityView
{
private:
    ComponentMask componentMask;

public:
    EntityView(Scene &scene, EntityID entityID)
    {
        this->componentMask = scene.GetEntityEntry(entityID).mask;
    }

    struct Iterator
    {
        Iterator(ComponentID index, ComponentMask mask) : index(index), mask(mask) {}

        ComponentID operator*() const
        {
            return index;
        }

        bool operator==(const Iterator &other) const
        {
            return (index == other.index) || (!ValidIndex() && !other.ValidIndex());
        }

        bool operator!=(const Iterator &other) const
        {
            return (index != other.index) && (ValidIndex() || other.ValidIndex());
        }

        bool ValidIndex() const
        {
            return index < MAX_COMPONENTS;
        }

        Iterator &operator++()
        {
            do
            {
                index++;
            } while (ValidIndex() && !mask.test(index));
            return *this;
        }

        u32 index;
        ComponentMask mask;
    };

    const Iterator begin() const
    {
        return Iterator(0, componentMask);
    }

    const Iterator end() const
    {
        return Iterator(MAX_COMPONENTS, componentMask);
    }
};

// TODO(marvin): GetNumberOfDefinedComponents is currently defined in scene_loader, because it relies on the book-keeping of the scene editor functionality, and the entity view and entity complement view are also really used for scene editor functionality... would they ever be used for non scene editor things?

u32 GetNumberOfDefinedComponents();

// Iterates through the components that don't exist on a given entity.
struct EntityComplementView
{
private:
    ComponentMask componentMask;

public:
    EntityComplementView(Scene &scene, EntityID entityID)
    {
        this->componentMask = scene.GetEntityEntry(entityID).mask;
    }

    struct Iterator
    {
        Iterator(ComponentID index, ComponentMask mask) : index(index), mask(mask) {}

        ComponentID operator*() const
        {
            return index;
        }

        bool operator==(const Iterator &other) const
        {
            return (index == other.index) || (!ValidIndex() && !other.ValidIndex());
        }

        bool operator!=(const Iterator &other) const
        {
            return (index != other.index) && (ValidIndex() || other.ValidIndex());
        }

        bool ValidIndex() const
        {
            return index < GetNumberOfDefinedComponents();
        }

        Iterator &operator++()
        {
            do
            {
                index++;
            } while (ValidIndex() && mask.test(index));
            return *this;
        }

        u32 index;
        ComponentMask mask;
    };

    const Iterator begin() const
    {
        return Iterator(0, componentMask);
    }

    const Iterator end() const
    {
        return Iterator(GetNumberOfDefinedComponents(), componentMask);
    }
};
