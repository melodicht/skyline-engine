#pragma once

#include <bitset>
#include <typeinfo>
#include <typeindex>

#include <memory.h>

/*
 * TYPE DEFINITIONS AND CONSTANTS
 */

// Comparison via ==
// Note the INVALID_ENTITY
typedef u64 EntityID;
typedef u32 ComponentID;

constexpr u32 MAX_COMPONENTS = 32;
typedef std::bitset<MAX_COMPONENTS> ComponentMask;
constexpr u32 MAX_ENTITIES = 32768;
constexpr u32 MAX_SYSTEMS = 128;

constexpr u32 SYSTEMS_MEMORY = Kilobytes(16);

/*
 * ID FUNCTIONALITY
 */

#define INVALID_ENTITY_ID (u32)(-1)
#define INVALID_ENTITY CreateEntityId(INVALID_ENTITY_ID, 0)

// Allows the storage of EntityID's such that they store both the Index and their Version number
// This allows for deletion without overlapping slots.
// Index and Version number are both u32s that will be combined into EntityID.

inline EntityID CreateEntityId(u32 index, u32 version)
{
    // Shift the index up 32, and put the version in the bottom
    return ((EntityID) index << 32) | ((EntityID) version);
}

inline u32 GetEntityVersion(EntityID id)
{
    // Cast to a 32 bit int to get our version number (losing the top 32 bits)
    return (u32) id;
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

inline bool IsEntityValid(EntityID id)
{
    // Check if the index is our invalid index
    return (id >> 32) != (u32) (-1);
}

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
// User of the struct is responsible for managing memory.
struct ComponentPool
{
    u8 *pData{nullptr};
    size_t elementSize{0};

    ComponentPool();
    
    ComponentPool(void *base, size_t elementsize);

    // Gets the component in this pData at the given index.
    inline void *get(size_t index)
    {
        // looking up the component at the desired index
        return pData + index * elementSize;
    }

    inline EntityID getOwner(u8 *ptr)
    {
        return ((size_t)(ptr - pData)) / elementSize;
    }
};

/*
 * SYSTEM
 */

struct Scene;
struct GameInput;

#define SYSTEM_VTABLE_ON_START_PARAMS Scene *scene
#define SYSTEM_VTABLE_ON_START_PASS scene
#define SYSTEM_VTABLE_ON_START(name) void name(void *self, SYSTEM_VTABLE_ON_START_PARAMS)
typedef SYSTEM_VTABLE_ON_START(system_vtable_on_start_t);
#define SYSTEM_ON_START(...) void __VA_ARGS__ __VA_OPT__(::) OnStart(SYSTEM_VTABLE_ON_START_PARAMS)

// NOTE(marvin): The SYSTEM_ON_X(...) family of macros should only
// take in 0 or 1 argument, where the 1 argument is the name of the
// derived System.

#define SYSTEM_VTABLE_ON_UPDATE_PARAMS Scene *scene, GameInput *input, f32 deltaTime
#define SYSTEM_VTABLE_ON_UPDATE_PASS scene, input, deltaTime
#define SYSTEM_VTABLE_ON_UPDATE(name) void name(void *self, SYSTEM_VTABLE_ON_UPDATE_PARAMS)
typedef SYSTEM_VTABLE_ON_UPDATE(system_vtable_on_update_t);
#define SYSTEM_ON_UPDATE(...) void __VA_ARGS__ __VA_OPT__(::) OnUpdate(SYSTEM_VTABLE_ON_UPDATE_PARAMS)

#define SYSTEM_UPDATE_VTABLE(...) VALUE_IFNOT(__VA_OPT__(1), static) void __VA_ARGS__ __VA_OPT__(::) UpdateVTable()


// NOTE(marvin): Makes a derived system a singleton, with essential
// features that allow the ECS code to interact with the manual
// vtable, and for game load to update the manual
// vtables. Unfortunately anything after this macaro is in the
// "public:".
#define MAKE_SYSTEM_DECLARATIONS(T)                                     \
    private:                                                            \
    static T* instance;                                                 \
public:                                                                 \
 T(const T&) = delete;                                                  \
 T& operator=(const T&) = delete;                                       \
 T(T&&) = delete;                                                       \
 T& operator=(T&&) = delete;                                            \
 template<typename... Args>                                             \
 static T *Initialize(void* base, Args&&... args)                       \
 {                                                                      \
     PrintAssert(instance == nullptr, #T " has already been initialized."); \
     instance = new (base) T(std::forward<Args>(args) ...);             \
     return instance;                                                   \
 }                                                                      \
 static T& Get()                                                        \
 {                                                                      \
     PrintAssert(instance != nullptr, #T " hasn't been initialized.");  \
     return *instance;                                                  \
 }                                                                      \
 SYSTEM_UPDATE_VTABLE();                                                \
 

struct SystemVTable
{
    system_vtable_on_start_t *onStart;
    system_vtable_on_update_t *onUpdate;
};

// A system in our ECS, which defines operations on a subset of
// entities, using scene view.

// NOTE(marvin): Due to the manual vtable shenanigans, the methods
// that correspond to entries in the manual vtable must be public so
// that the manual vtable code can call into them.

class System
{
public:
    SystemVTable *vtable;
    
    virtual ~System() = default;

    void OnStart(SYSTEM_VTABLE_ON_START_PARAMS)
    {
        
    }
    
    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime)
    {
        
    }

    static void UpdateVTable();
};

#define MAKE_SYSTEM_MANUAL_VTABLE(T)                        \
    T::instance = nullptr;                                  \
    SYSTEM(T);                                              \
    SYSTEM_VTABLE_ON_START(NameConcat(T, _OnStart))         \
    {                                                       \
        T *sys = static_cast<T *>(self);                    \
        sys->OnStart(SYSTEM_VTABLE_ON_START_PASS);          \
    }                                                       \
    SYSTEM_VTABLE_ON_UPDATE(NameConcat(T, _OnUpdate))       \
    {                                                       \
        T *sys = static_cast<T *>(self);                    \
        sys->OnUpdate(SYSTEM_VTABLE_ON_UPDATE_PASS);        \
    }                                                       \
    SystemVTable NameConcat3(global, T, VTable) =           \
    {                                                       \
        .onStart = NameConcat(T, _OnStart),                 \
        .onUpdate = NameConcat(T, _OnUpdate),               \
    };                                                      \
    void T::UpdateVTable()                                  \
    {                                                       \
        T& instance = T::Get();                             \
        instance.vtable = &NameConcat3(global, T, VTable);  \
    }

extern std::unordered_map<std::type_index, ComponentID> typeToId;

template<typename T>
ComponentID GetComponentId()
{
    if (auto search = typeToId.find(std::type_index(typeid(T)));
            search != typeToId.end())
    {
        u64 count = search->second;
        return count;
    }

    Assert(false && "Invalid component ID");
    exit(1);
}

/*
 * SCENE DEFINITION
 */

struct EntityEntry
{
    EntityID id; // though redundent with index in array, required
    // for deleting entities,

    // NOTE(marvin): Only to be used within ECS-specific code.
    ComponentMask mask;
};

inline EntityEntry InitEntityEntryWithIndex(u32 entityIndex)
{
    EntityEntry result = {};
    result.id = CreateEntityId(entityIndex, 0);
    result.mask = ComponentMask();
    return result;
}

inline void InvalidateEntityEntry(EntityEntry *entityEntry)
{
    entityEntry->id = InvalidateEntityId(entityEntry->id);
    entityEntry->mask.reset();
}

inline b32 EntityEntryValid(EntityEntry *entityEntry)
{
    b32 result = IsEntityValid(entityEntry->id);
    return result;
}

inline b32 EntityEntryInvalid(EntityEntry *entityEntry)
{
    u32 entityIndex = GetEntityIndex(entityEntry->id);
    b32 result = entityIndex == INVALID_ENTITY_ID;
    return result;
}

// Subs in the given entity index into the given entity entry.
// The entity entry should be marked as invalid.
inline void ValidateEntityEntryWithIndex(EntityEntry *entityEntry, u32 index)
{
    Assert(EntityEntryInvalid(entityEntry) &&
           "A free entity entry should be marked as an invalid entity.");

    u32 entityVersion = GetEntityVersion(entityEntry->id);
    EntityID newID = CreateEntityId(index, entityVersion);
    entityEntry->id = newID;
}

inline b32 EntityEntryMismatch(EntityEntry *entityEntry, EntityID id)
{
    b32 result = entityEntry->id != id;
    return result;
}

inline void ClearComponentFromEntityEntry(EntityEntry *entityEntry, ComponentID componentId)
{
    ComponentMask &mask = entityEntry->mask;
    mask.reset(componentId);
}

// NOTE(marvin): Max size given by MAX_ENTITIES.
struct EntitiesPool
{
    EntityEntry *entries;
    u32 count;
};

// Initializes an entities pool, allocating from the given memory arena.
inline EntitiesPool InitEntitiesPool(MemoryArena *remainingArena)
{
    EntitiesPool result = {};
    result.entries = PushArray(remainingArena, MAX_ENTITIES, EntityEntry);
    result.count = 0;
    return result;
}

inline EntityEntry *GetFromEntitiesPool(EntitiesPool *pool, u32 index)
{
    Assert(index < MAX_ENTITIES);
    EntityEntry *result = pool->entries + index;
    return result;
}

inline EntityEntry *GetFromEntitiesPoolWithEntityID(EntitiesPool *pool, EntityID id)
{
    u32 index = GetEntityIndex(id);
    EntityEntry *result = GetFromEntitiesPool(pool, index);
    return result;
}

inline EntityEntry *AddNewEntityEntry(EntitiesPool *pool)
{
    Assert(pool->count < MAX_ENTITIES);
    EntityEntry *nextEntityEntry = pool->entries + pool->count;
    *nextEntityEntry = InitEntityEntryWithIndex(pool->count);
    ++pool->count;
    return nextEntityEntry;
}

inline u32 GetEntitiesPoolSize(EntitiesPool *pool)
{
    u32 result = pool->count;
    return result;
}

// Assumes that the entity id and the index, and the entity entry at the index all correspond.
inline void DestroyEntityEntryInEntitiesPool(EntitiesPool *entities, u32 index, EntityID id)
{
    EntityEntry *entityEntry = GetFromEntitiesPool(entities, index);
    Assert((GetEntityIndex(id) == index) &&
           (entityEntry->id == id) &&
           (GetEntityIndex(entityEntry->id) == index));

    InvalidateEntityEntry(entityEntry);
}

// Indicates whether the entity already has been deleted, and also
// fills in the given entity entry.
inline b32 EntityAlreadyDeleted(EntitiesPool *pool, EntityID id, EntityEntry **entityEntry)
{
    *entityEntry = GetFromEntitiesPoolWithEntityID(pool, id);
    b32 result = (*entityEntry)->id != id;
    return result;
}

inline b32 EntityAlreadyDeleted(EntitiesPool *pool, EntityID id)
{
    EntityEntry *entityEntry;
    return EntityAlreadyDeleted(pool, id, &entityEntry);
}

// NOTE(marvin): Max size given by MAX_COMPONENTS.
struct ComponentsPool
{
    ComponentPool *componentPools;
};

// Holds pointers to the addresses of systems in the memory arena.
struct SystemsBuffer
{
    System **base;
    u32 count;
};

inline SystemsBuffer InitSystemsBuffer(MemoryArena *remainingArena)
{
    SystemsBuffer result = {};
    result.base = PushArray(remainingArena, MAX_SYSTEMS, System *);
    return result;
}

struct ComponentPoolsBuffer
{
    ComponentPool *base;
    u32 count;

    ComponentPoolsBuffer();

    ComponentPoolsBuffer(MemoryArena *remainingArena);

    ComponentPool *operator[](u32);

    void Push(ComponentPool componentPool);
};

// Each component has its own memory pool, to have good memory
// locality. An entity's ID is the index into its own component in the
// component pool.
struct Scene
{
public:
    EntitiesPool entities;
    FreeIndicesStack freeIndices;

    SystemsBuffer systemsBuffer;
    MemoryArena systemsArena;

    ComponentPoolsBuffer componentPools;
    MemoryArena componentPoolsArena;

private:
    void *GetComponentAddress(EntityID entityId, ComponentID componentId)
    {
        ComponentPool *componentPool = componentPools[componentId];
        u32 entityIndex = GetEntityIndex(entityId);
        void *result = componentPool->get(entityIndex);
        return result;
    }
public:
    Scene(MemoryArena *remainingArena);

    ~Scene();
    
    void AddSystem(System *system);
    
    void InitSystems();

    void UpdateSystems(GameInput *input, f32 deltaTime);

    void AddComponentPool(size_t componentSize);

    // Adds a new entity to this vector of entities, and returns its
    // ID. Can only support 2^64 entities without ID conflicts.
    EntityID NewEntity();

    EntityEntry &GetEntityEntry(EntityID id);

    // Removes a given entity from the scene and signals to the scene the free space that was left behind
    void DestroyEntity(EntityID id);

    u32 GetNumCompTypes()
    {
        return componentPools.count;
    }

    // Removes a component from the entity with the given EntityID
    // if the EntityID is not already removed.
    template<typename T>
    void Remove(EntityID id)
    {
        EntityEntry *entityEntry;
        
        if (EntityAlreadyDeleted(&entities, id, &entityEntry))
        {
            printf("Entity has been deleted, can't remove component.");
            return;
        }

        ComponentID componentId = GetComponentId<T>();
        ClearComponentFromEntityEntry(entityEntry, componentId);
    }

    // Assigns the entity associated with the given entity ID in this
    // vector of entities a new instance of the given component. Then,
    // adds it to its corresponding memory pool, and returns a pointer
    // to it.
    template<typename T>
    T *Assign(EntityID id)
    {
        T *result = nullptr;
        ComponentID componentId = GetComponentId<T>();

        if (GetNumCompTypes() <= componentId) // Invalid component
        {
            printf("Invalid component. Components must be defined in components.\n");
            exit(1);
        }

        // Verify that the component doesn't already exist.
        EntityEntry &entityEntry = GetEntityEntry(id);
        ComponentMask &componentMask = entityEntry.mask;
        b32 componentAlreadyExists = componentMask.test(componentId);
        if (componentAlreadyExists)
        {
            puts("Attempted to add a component to an entity that already has the component, ignoring.");
        }
        else
        {
            void *componentAddress = GetComponentAddress(id, componentId);
            result = new(componentAddress) T();
            componentMask.set(componentId);
        }
        return result;
    }

    void *Get(EntityID entityId, ComponentID componentId)
    {
        if (!GetEntityEntry(entityId).mask.test(componentId))
            return nullptr;

        void *pComponent = GetComponentAddress(entityId, componentId);
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
        T *result = static_cast<T *>(Get(id, componentId));
        return result;
    }

    b32 Has(EntityID entityId, ComponentID componentId)
    {
        EntityEntry *entityEntry = GetFromEntitiesPoolWithEntityID(&entities, entityId);
        return entityEntry->mask.test(componentId);
    }

    template <typename T>
    b32 Has(EntityID id)
    {
        ComponentID componentId = GetComponentId<T>();
        return Has(id, componentId);
    }

    template<typename T>
    EntityID GetOwner(T* component)
    {
        s32 componentId = GetComponentId<T>();
        EntityID id = componentPools[componentId]->getOwner((u8*)component);
        return id;
    }
};

#define PushSystem(scene, T) (PushStruct(&((scene)->systemsArena), T))
#define RegisterSystem(scene, T, ...) \
    ({ \
        Scene *_scene = (scene); \
        T *_system = T::Initialize(PushSystem(_scene, T)__VA_OPT__(,) __VA_ARGS__); \
        _scene->AddSystem(_system); \
        _system->UpdateVTable();     \
        _system; })
