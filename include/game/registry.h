#pragma once

#ifdef REGISTRY

#include <scene_loader.h>
#include <typeinfo>
#include <typeindex>

template <typename T>
const char* compName;

template <typename T>
s32 WriteFromData(T* dest, DataEntry* data) { return 0; }

template <typename T>
DataEntry* ReadToData(T* src, std::string name)
{
    return new DataEntry(name);
}

template <typename T>
s32 WriteIfPresent(T* dest, std::string name, std::vector<DataEntry*>& data)
{
    for (DataEntry* entry : data)
    {
        if (entry->name == name)
        {
            return WriteFromData<T>(dest, entry);
        }
    }
    return 0;
}

template <typename T>
void AssignComponent(Scene &scene, EntityID entity)
{
    scene.Assign<T>(entity);
}

template <typename T>
void RemoveComponent(Scene &scene, EntityID entity)
{
    scene.Remove<T>(entity);
}

template <typename T>
s32 WriteComponent(Scene &scene, EntityID entity, DataEntry* compData)
{
    T* comp = scene.Get<T>(entity);
    if (comp == nullptr)
    {
        printf("entity must have component but doesn't\n");
        return -1;
    }
    return WriteFromData<T>(comp, compData);
}

template <typename T>
DataEntry* ReadComponent(Scene &scene, EntityID entity)
{
    T* comp = scene.Get<T>(entity);
    if (comp == nullptr)
    {
        return nullptr;
    }
    return ReadToData<T>(comp, compName<T>);
}

template <typename T>
void AddComponent(const char *name)
{
    compName<T> = name;
    CompInfos().push_back({AssignComponent<T>, RemoveComponent<T>, WriteComponent<T>, ReadComponent<T>, sizeof(T), std::type_index(typeid(T)), name});
}

template <typename T>
void AddComponent(const char *name, const char *icon)
{
    compName<T> = name;
    CompInfos().push_back({AssignComponent<T>, RemoveComponent<T>, WriteComponent<T>, ReadComponent<T>, sizeof(T), std::type_index(typeid(T)), name, icon});
}

#define PARENS ()

#define EXPAND(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_FIELDS(f, type, ...) \
    __VA_OPT__(EXPAND(_FOR_FIELDS(f, type, __VA_ARGS__)))
#define _FOR_FIELDS(f, type, a1, ...) \
    f(type, a1) \
    __VA_OPT__(__FOR_FIELDS PARENS (f, type, __VA_ARGS__))
#define __FOR_FIELDS() _FOR_FIELDS

#define WRITE_FIELD(type, field) \
    rv |= WriteIfPresent<decltype(type::field)>(&dest->field, #field, data->structVal);

#define READ_FIELD(type, field) \
    data->structVal.push_back(ReadToData<decltype(type::field)>(&src->field, #field));

#define DECLARE_EXTERNS(type, field) \
    extern template s32 WriteFromData<decltype(type::field)>(decltype(type::field)* dest, DataEntry* data); \
    extern template DataEntry* ReadToData<decltype(type::field)>(decltype(type::field)* src, std::string name);

#define SERIALIZE(name, ...) \
    FOR_FIELDS(DECLARE_EXTERNS, name, __VA_ARGS__) \
    template<> \
    s32 WriteFromData<name>(name* dest, DataEntry* data) \
    { \
        if (data->type != STRUCT_ENTRY) \
        { \
            printf("entry must be struct but instead is %d\n", data->type); \
            return -1; \
        } \
        s32 rv = 0; \
        FOR_FIELDS(WRITE_FIELD, name, __VA_ARGS__) \
        return rv; \
    } \
    template <> \
    DataEntry* ReadToData<name>(name* src, std::string name) \
    { \
        DataEntry* data = new DataEntry(name); \
        FOR_FIELDS(READ_FIELD, name, __VA_ARGS__) \
        return data; \
    }

#define COMPONENT(type, ...) [[maybe_unused]] static int add##type = (AddComponent<type>(#type __VA_OPT__(,) __VA_ARGS__), 0);

#else

#define SERIALIZE(...)
#define COMPONENT(...)

#endif
