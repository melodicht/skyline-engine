#pragma once

#include <vector>
#include <typeinfo>

/*
 * TYPE DEFINITIONS AND CONSTANTS
 */

typedef u64 EntityID;
constexpr u32 MAX_COMPONENTS = 32;
typedef std::bitset<MAX_COMPONENTS> ComponentMask;
constexpr u32 MAX_ENTITIES = 32768;

/*
 * ID FUNCTIONALITY
 */

// Allows the storage of EntityID's such that they store both the Index and their Version number
// This allows for deletion without overlapping slots.
// Index and Version number are both u32s that will be combined into EntityID.

local inline EntityID CreateEntityId(u32 index, u32 version);

// This should represent the index an element has within the "entities" vector inside a Scene
local inline u32 GetEntityIndex(EntityID id);

local inline u32 GetEntityVersion(EntityID id);

// Checks if the EntityID has not been deleted
local inline bool IsEntityValid(EntityID id);

#define INVALID_ENTITY CreateEntityId((u32)(-1), 0)

//////////////// COMPONENTS ////////////////

// NOTE(marvin): GetId has been moved to plaform layer, so that the
// IDs don't get reset after a hot reload.

/*
 * COMPONENT POOL
 */

// Responsible for allocating contiguous memory for the components
// such that `MAX_ENTITIES` can be stored, and components be accessed
// via index.
// NOTE: The memory pool is an array of bytes, as the size of one
// component isn't known at compile time.
struct ComponentPool
{
    u8 *pData{nullptr};
    size_t elementSize{0};

    ComponentPool(size_t elementsize);

    ~ComponentPool();

    // Gets the component in this pData at the given index.
    inline void *get(size_t index);
};

/*
 * SYSTEM
 */

struct Scene;

// A system in our ECS, which defines operations on a subset of
// entities, using scene view.
class System
{
public:
    virtual void OnStart(Scene *scene) {};
    virtual void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime) {};
    virtual ~System() = default;
};

local u32 numComponents = 0;

template<typename T>
const char *compName;

// NOTE(marvin): The reason why this is separated out from the struct
// is to mirror the prior implementation where it was also separated
// out from the struct. Honestly, could just integrate it.
local std::unordered_map<std::string, u32> stringToId;

local u32 MakeComponentId(std::string name)
{
    stringToId[name] = numComponents;
    return numComponents++;
}

template<typename T>
local u32 GetComponentId()
{
    if (auto search = stringToId.find(compName<T>);
            search != stringToId.end())
    {
        u64 count = search->second;
        return count;
    }

    printf("Invalid component ID\n");
    exit(1);
}

/*
 * SCENE DEFINITION
 */

// Each component has its own memory pool, to have good memory
// locality. An entity's ID is the index into its own component in the
// component pool.
struct Scene
{
    struct EntityEntry
    {
        EntityID id; // though redundent with index in vector, required
        // for deleting entities,
        ComponentMask mask;
    };

    std::vector<EntityEntry> entities;
    std::vector<ComponentPool *> componentPools;
    std::vector<u32> freeIndices;
    std::vector<System *> systems;

    void AddSystem(System *sys);
      
    void InitSystems();

    void UpdateSystems(GameInput *input, f32 deltaTime);

    void AddComponentPool(size_t size);

    // Adds a new entity to this vector of entities, and returns its
    // ID. Can only support 2^64 entities without ID conflicts.
    EntityID NewEntity();

    // Removes a given entity from the scene and signals to the scene the free space that was left behind
    void DestroyEntity(EntityID id);

    // Removes a component from the entity with the given EntityID
    // if the EntityID is not already removed.
    template<typename T>
    void Remove(EntityID id)
    {
        // ensures you're not accessing an entity that has been deleted
        if (entities[GetEntityIndex(id)].id != id)
            return;

        int componentId = GetComponentId<T>();
        // Finds location of component data within the entity component pool and
        // resets, thus removing the component from the entity
        entities[GetEntityIndex(id)].mask.reset(componentId);
    }

    // Assigns the entity associated with the given entity ID in this
    // vector of entities a new instance of the given component. Then,
    // adds it to its corresponding memory pool, and returns a pointer
    // to it.
    template<typename T>
    T *Assign(EntityID id)
    {
        int componentId = GetComponentId<T>();

        if (numComponents <= componentId) // Invalid component
        {
            printf("Invalid component. Components must be defined in components.h\n");
            exit(1);
        }

        // Looks up the component in the pool, and initializes it with placement new
        T *pComponent = new(componentPools[componentId]->get(GetEntityIndex(id))) T();

        entities[GetEntityIndex(id)].mask.set(componentId);
        return pComponent;
    }

    // Returns the pointer to the component instance on the entity
    // associated with the given ID in this vector of entities, with the
    // given component type. Returns nullptr if that entity doesn't have
    // the given component type.
    template<typename T>
    T *Get(EntityID id)
    {
        int componentId = GetComponentId<T>();
        if (!entities[GetEntityIndex(id)].mask.test(componentId))
            return nullptr;

        T *pComponent = static_cast<T *>(componentPools[componentId]->get(GetEntityIndex(id)));
        return pComponent;
    }
};
