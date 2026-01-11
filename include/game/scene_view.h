#pragma once

#include <meta_definitions.h>
#include <scene.h>

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