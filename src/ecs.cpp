/*
 * ID FUNCTIONALITY
 */

inline EntityID CreateEntityId(u32 index, u32 version)
{
    // Shift the index up 32, and put the version in the bottom
    return ((EntityID) index << 32) | ((EntityID) version);
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

ComponentPool::ComponentPool(size_t elementsize)
{
    // We'll allocate enough memory to hold MAX_ENTITIES, each with element size
    elementSize = elementsize;
    pData = new u8[elementSize * MAX_ENTITIES];
}

ComponentPool::~ComponentPool()
{
    delete[] pData;
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

/*
 * SCENE FUNCTIONALITY
 */

// Each component has its own memory pool, to have good memory
// locality. An entity's ID is the index into its own component in the
// component pool.
void Scene::AddSystem(System *sys)
{
    systems.push_back(sys);
}

void Scene::InitSystems()
{
    for (System *sys : systems)
    {
        sys->OnStart(this);
    }
}

void Scene::UpdateSystems(GameInput *input, f32 deltaTime)
{
    for (System *sys: systems)
    {
        sys->OnUpdate(this, input, deltaTime);
    }
}

void Scene::AddComponentPool(size_t size)
{
    componentPools.push_back(new ComponentPool(size));
}

EntityID Scene::NewEntity()
{
    // std::vector::size runs in constant time.
    if (!freeIndices.empty())
    {
        u32 newIndex = freeIndices.back();
        freeIndices.pop_back();
        // Takes in index and incremented EntityVersion at that index
        EntityID newID = CreateEntityId(newIndex, GetEntityVersion(entities[newIndex].id));
        entities[newIndex].id = newID;
        return entities[newIndex].id;
    }
    entities.push_back({CreateEntityId((u32) (entities.size()), 0), ComponentMask()});
    return entities.back().id;
}

Scene::EntityEntry &Scene::GetEntityEntry(EntityID id)
{
    Scene::EntityEntry &result = entities[GetEntityIndex(id)];
    return result;
}

void Scene::DestroyEntity(EntityID id)
{
    // Increments EntityVersion at the deleted index
    EntityID newID = CreateEntityId((u32) (-1), GetEntityVersion(id) + 1);
    EntityEntry &entry = GetEntityEntry(id);
    entry.id = newID;
    entry.mask.reset();
    freeIndices.push_back(GetEntityIndex(id));
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
            return pScene->entities[index].id;
        }

        // Compare two iterators
        bool operator==(const Iterator &other) const
        {
            return index == other.index || index == pScene->entities.size();
        }

        bool operator!=(const Iterator &other) const
        {
            return index != other.index && index != pScene->entities.size();
        }

        bool ValidIndex()
        {
            return
                // It's a valid entity ID
                    IsEntityValid(pScene->entities[index].id) &&
                    // It has the correct component mask
                    (all || mask == (mask & pScene->entities[index].mask));
        }

        // Move the iterator forward
        Iterator &operator++()
        {
            do
            {
                index++;
            } while (index < pScene->entities.size() && !ValidIndex());
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
        int firstIndex = 0;
        while (firstIndex < pScene->entities.size() &&
               (componentMask != (componentMask & pScene->entities[firstIndex].mask)
                || !IsEntityValid(pScene->entities[firstIndex].id)))
        {
            firstIndex++;
        }
        return Iterator(pScene, firstIndex, componentMask, all);
    }

    // Give an iterator to the end of this view
    const Iterator end() const
    {
        return Iterator(pScene, (u32) (pScene->entities.size()), componentMask, all);
    }

    Scene *pScene{nullptr};
    ComponentMask componentMask;
    bool all{false};
};

// Iterates through the components of a given entity.
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

