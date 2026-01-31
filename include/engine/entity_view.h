#pragma once

#include <meta_definitions.h>
#include <scene.h>

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
        u32 index = 0;
        while (index < MAX_COMPONENTS && !componentMask.test(index))
        {
            index++;
        }
        return Iterator(index, componentMask);
    }

    const Iterator end() const
    {
        return Iterator(MAX_COMPONENTS, componentMask);
    }
};

// Iterates through the components that don't exist on a given entity.
struct EntityComplementView
{
private:
    ComponentMask componentMask;
    Scene& scene;

public:
    EntityComplementView(Scene &scene, EntityID entityID) : scene(scene)
    {
        this->componentMask = scene.GetEntityEntry(entityID).mask;
    }

    struct Iterator
    {
        Iterator(u32 index, ComponentMask mask, u32 count) : index(index), mask(mask), count(count) {}

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
            return index < count;
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
        u32 count;
    };

    const Iterator begin() const
    {
        u32 index = 0;
        while (index < scene.GetNumCompTypes() && componentMask.test(index))
        {
            index++;
        }
        return Iterator(index, componentMask, scene.GetNumCompTypes());
    }

    const Iterator end() const
    {
        return Iterator(scene.GetNumCompTypes(), componentMask, scene.GetNumCompTypes());
    }
};