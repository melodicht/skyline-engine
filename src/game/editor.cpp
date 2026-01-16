#include <imgui.h>

#include <editor.h>
#include <components.h>
#include <game.h>
#include <scene.h>
#include <scene_loader.h>
#include <utils.h>
#include <scene_view.h>
#include <entity_view.h>

typedef u32 ComponentDataEntryActionOutcome;

b32 EditorSystem::ShouldRewriteComponentDataEntry(ComponentDataEntryActionOutcome outcome)
{
    b32 result = (outcome & REWRITE);
    return result;
}

b32 EditorSystem::ShouldRemoveComponentDataEntry(ComponentDataEntryActionOutcome outcome)
{
    b32 result = (outcome & REMOVE);
    return result;
}

// Diplays the data entry, and indicates whether any action must
// be taken on the component root data entry.
// Only reads the data.
// To be called inside of ImGui scope.
ComponentDataEntryActionOutcome EditorSystem::ImguiDisplayDataEntry(DataEntry *dataEntry, Scene &scene, EntityID ent, b32 isComponent)
{
    // TODO(marvin): Duplicate code between the non-recursive cases, the way to abstract is also not immediately obvious.
    ComponentDataEntryActionOutcome result = NOTHING;
    switch (dataEntry->type)
    {
        case INT_ENTRY:
        {
            Assert(!isComponent);
            const char *fieldName = dataEntry->name.c_str();
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 150);

            ImGui::Text("%s", fieldName);
            ImGui::NextColumn();
            if (ImGui::InputInt(fieldName, &(dataEntry->intVal)))
            {
                result = REWRITE;
            }
            ImGui::NextColumn();
            ImGui::Columns(1);
            break;
        }
        case FLOAT_ENTRY:
        {
            Assert(!isComponent);
            const char *fieldName = dataEntry->name.c_str();
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 150);

            ImGui::Text("%s", fieldName);
            ImGui::NextColumn();
            if (ImGui::InputFloat(fieldName, &(dataEntry->floatVal)))
            {
                result = REWRITE;
            }
            ImGui::NextColumn();
            ImGui::Columns(1);
            break;
        }
        case BOOL_ENTRY:
        {
            Assert(!isComponent);
            const char *fieldName = dataEntry->name.c_str();
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 150);

            ImGui::Text("%s", fieldName);
            ImGui::NextColumn();
            if (ImGui::Checkbox(fieldName, &(dataEntry->boolVal)))
            {
                result = REWRITE;
            }
            ImGui::NextColumn();
            ImGui::Columns(1);
            break;
        }
        case VEC_ENTRY:
        {
            Assert(!isComponent);
            const char *fieldName = dataEntry->name.c_str();
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 150);

            ImGui::Text("%s", fieldName);
            ImGui::NextColumn();

            glm::vec3 vec = dataEntry->vecVal;
            f32 xyz[3] = {vec.x, vec.y, vec.z};
            if (ImGui::InputFloat3(fieldName, xyz))
            {
                dataEntry->vecVal = {xyz[0], xyz[1], xyz[2]};
                result = REWRITE;
            }
            ImGui::NextColumn();
            ImGui::Columns(1);
            break;
        }
        case STR_ENTRY:
        {
            Assert(!isComponent);
            const char *fieldName = dataEntry->name.c_str();
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 150);

            ImGui::Text("%s", fieldName);
            ImGui::NextColumn();

            // TODO(marvin): Need to have temporary string buffer for string value.
            // NOTE(marvin): Pulled that number out of my ass.
            char buf[256];
            const char *fieldStringValue = dataEntry->stringVal.c_str();
            strncpy(buf, fieldStringValue, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText(fieldName, buf, sizeof(buf)))
            {
                dataEntry->stringVal = buf;
                result = REWRITE;
            }
            ImGui::NextColumn();
            ImGui::Columns(1);
            break;
        }
        case STRUCT_ENTRY:
        {
            result = this->ImguiDisplayStructDataEntry(dataEntry->name, dataEntry->structVal, scene, ent, isComponent);
            break;
        }
    }
    return result;
}

// Diplays the data entry for a struct, and indicates whether any data has been changed.
// Only reads from the data.
ComponentDataEntryActionOutcome EditorSystem::ImguiDisplayStructDataEntry(std::string name, std::vector<DataEntry*> dataEntries, Scene &scene, EntityID ent, b32 isComponent)
{
    ComponentDataEntryActionOutcome result = NOTHING;

    if (isComponent)
    {
        std::string entityComponentUniqueName = std::to_string(ent) + name;
        ImGui::PushID(entityComponentUniqueName.c_str());
    }

    const char *nodeName = name.c_str();
    if (ImGui::TreeNode(nodeName))
    {
        if (ImGui::Button("Remove Component"))
        {
            result = REMOVE;
        }

        for (DataEntry *dataEntry : dataEntries)
        {
            result |= this->ImguiDisplayDataEntry(dataEntry, scene, ent, false);
        }
        ImGui::TreePop();
    }

    if (isComponent)
    {
        ImGui::PopID();
    }
    return result;
}

void EditorSystem::OnUpdate(Scene *scene, GameInput *input, f32 deltaTime)
{
    if (input->keysDown.contains("Mouse 3"))
    {
        FlyingMovement *f = scene->Get<FlyingMovement>(editorCam);
        Transform3D *t = scene->Get<Transform3D>(editorCam);

        t->AddLocalRotation({0, 0, input->mouseDeltaX * f->turnSpeed});
        t->AddLocalRotation({0, input->mouseDeltaY * f->turnSpeed, 0});
        t->SetLocalRotation({t->GetLocalRotation().x, std::min(std::max(t->GetLocalRotation().y, -90.0f), 90.0f), t->GetLocalRotation().z});

        glm::vec3 movementDirection = GetMovementDirection(input, t);
        t->AddLocalPosition(movementDirection * f->moveSpeed * deltaTime);
    }

    // TODO(marvin): Maybe the ecs editor functionality shouldn't be within the EditorSystem? Feels strange that the overlay GUI is split in two places. Maybe an EditorState? Which could hold some of the stuff defined at a global level in scene_loader.cpp.
    // NOTE(marvin): ECS editor.
    if (*overlayMode == overlayMode_ecsEditor)
    {
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings;

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize({viewport->Size.x, 0});

        ImGui::Begin("Overlay", nullptr, window_flags);

        if (input->keysDown.contains("Mouse 1"))
        {
            u32 cursorEntityIndex = globalPlatformAPI.renderer.GetIndexAtCursor();
            selectedEntityID = CreateEntityId(cursorEntityIndex, 0);
        }

        // TODO(marvin): Make name editable.
        // NOTE(marvin): Entities list
        if (ImGui::BeginListBox("Entities"))
        {
            for (EntityID entityID : SceneView(*scene))
            {
                NameComponent *maybeNameComponent = scene->Get<NameComponent>(entityID);
                if (maybeNameComponent)
                {
                    NameComponent *nameComponent = maybeNameComponent;
                    const char *entityName = (nameComponent->name).c_str();
                    std::string entityIDString = std::to_string(entityID);

                    ImGui::PushID(entityIDString.c_str());
                    const bool isSelected = (entityID == this->selectedEntityID);

                    if (isSelected)
                    {
                        // NOTE(marvin): Pulled this number out of my ass.
                        char buf[256];
                        strncpy(buf, entityName, sizeof(buf) - 1);
                        buf[sizeof(buf) - 1] = '\0';

                        if (ImGui::InputText(("##" + entityIDString).c_str(), buf, sizeof(buf)))
                        {
                            nameComponent->name = buf;
                        }
                    }
                    else
                    {
                        if (ImGui::Selectable(entityName, isSelected))
                        {
                            selectedEntityID = entityID;
                        }

                        ImGui::IsItemHovered();
                    }
                    ImGui::PopID();
                }

            }
            ImGui::EndListBox();
        }

        // NOTE(marvin): Destroy selected entity.
        if (IsEntityValid(selectedEntityID) && ImGui::Button("Destroy Selected Entity"))
        {
            scene->DestroyEntity(selectedEntityID);
            selectedEntityID = INVALID_ENTITY;
        }

        // NOTE(marvin): Add new entity.
        if (ImGui::Button("New Entity"))
        {
            // TODO(marvin): Probably want a helper that creates the entity through this ritual. Common with the one in scene_loader::LoadScene, but that one doesn't assign a Transform3D.
            EntityID newEntityID = scene->NewEntity();

            // TODO(marvin): Will there be problems if there is an entity with that name already?
            std::string entityName = "New Entity";
            entityIds[entityName] = newEntityID;
            NameComponent* nameComp = scene->Assign<NameComponent>(newEntityID);
            nameComp->name = entityName;
            scene->Assign<Transform3D>(newEntityID);

            selectedEntityID = newEntityID;
        }

        if (IsEntityValid(selectedEntityID))
        {
            // NOTE(marvin): Component interactive tree view
            for (ComponentID componentID : EntityView(*scene, selectedEntityID))
            {
                ComponentInfo compInfo = compInfos[componentID];
                if (compInfo.name == NAME_COMPONENT)
                {
                    continue;
                }

                // NOTE(marvin): It's not possible to rewrite and
                // remove at the same time, but rewrite is
                // prioritised. Allowing both at the same time (if
                // somehow possible) seems risky.
                DataEntry *dataEntry = compInfo.readFunc(*scene, selectedEntityID);
                ComponentDataEntryActionOutcome componentOutcome = ImguiDisplayDataEntry(dataEntry, *scene, selectedEntityID, true);
                if (this->ShouldRewriteComponentDataEntry(componentOutcome))
                {
                    s32 val = compInfo.writeFunc(*scene, selectedEntityID, dataEntry);
                    if (val != 0)
                    {
                        printf("failed to write component");
                    }
                }
                else if (this->ShouldRemoveComponentDataEntry(componentOutcome))
                {
                    compInfo.removeFunc(*scene, selectedEntityID);
                }
                delete dataEntry;
            }
            // NOTE(marvin): Add component to current entity.
            if (!this->addingComponent)
            {
                if (ImGui::Button("Add Component"))
                {
                    this->addingComponent = true;
                }
            }
            else
            {
                if (ImGui::BeginListBox("Add which component?"))
                {
                    for (ComponentID componentID : EntityComplementView(*scene, selectedEntityID))
                    {
                        ComponentInfo compInfo = compInfos[componentID];
                        if (ImGui::Button(compInfo.name.c_str()))
                        {
                            compInfo.assignFunc(*scene, selectedEntityID);
                        }
                    }
                    ImGui::EndListBox();
                }

                if (ImGui::Button("Cancel"))
                {
                    this->addingComponent = false;
                }
            }

        }

        // NOTE(marvin): Save scene button
        if (ImGui::Button("Save Scene"))
        {
            SaveCurrentScene(*scene);
        }

        ImGui::End();
    }
}

EditorSystem::EditorSystem(EntityID editorCam, OverlayMode *overlayMode)
{
    this->editorCam = editorCam;
    this->overlayMode = overlayMode;
}
