#pragma once

#include <scene.h>
#include <overlay.h>

struct DataEntry;

class EditorSystem : public System
{
private:
    EntityID editorCam;
    OverlayMode *overlayMode;
    EntityID selectedEntityID = INVALID_ENTITY;
    b32 addingComponent = false;

    enum ComponentDataEntryAction : u32
    {
        NOTHING = 0b0,
        REWRITE = 0b1,
        REMOVE  = 0b10,
    };

    typedef u32 ComponentDataEntryActionOutcome;

    b32 ShouldRewriteComponentDataEntry(ComponentDataEntryActionOutcome outcome);

    b32 ShouldRemoveComponentDataEntry(ComponentDataEntryActionOutcome outcome);

    // Diplays the data entry, and indicates whether any action must
    // be taken on the component root data entry.
    // Only reads the data.
    // To be called inside of ImGui scope.
    ComponentDataEntryActionOutcome ImguiDisplayDataEntry(DataEntry *dataEntry, Scene &scene, EntityID ent, b32 isComponent);

    // Diplays the data entry for a struct, and indicates whether any data has been changed.
    // Only reads from the data.
    ComponentDataEntryActionOutcome ImguiDisplayStructDataEntry(std::string name, std::vector<DataEntry*> dataEntries, Scene &scene, EntityID ent, b32 isComponent);

public:
    EditorSystem(EntityID editorCam, OverlayMode *overlayMode);

    void OnUpdate(Scene *scene, GameInput *input, f32 deltaTime);
};
