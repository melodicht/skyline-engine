#include <overlay.h>
#include <meta_definitions.h>
#include <debug.h>
#include <imgui.h>
#include <engine.h>

#if SKL_DEBUG_MEMORY_VIEWER

local void RenderSizesViewerAllocations(DebugAllocations *allocations);

local void RenderSizesViewerRowArena(DebugArena *arena, const char *debugID, const char *name)
{
    ImGui::TableNextColumn();
    std::string memoryString = std::format("{} / {}", arena->used, arena->totalSize);
    b32 open = ImGui::TreeNode(memoryString.c_str());
    ImGui::TableNextColumn();
    ImGui::Text("%.1f%%", static_cast<f32>(arena->used) / static_cast<f32>(arena->totalSize));
    ImGui::TableNextColumn();
    // TODO(marvin): The DebugArena struct should accumulate this information as data.
    ImGui::Text("WIP");
    ImGui::TableNextColumn();
    ImGui::Text("%s", name);
    ImGui::TableNextColumn();
    ImGui::Text("%s", debugID);

    if (open)
    {
        RenderSizesViewerAllocations(&arena->allocations);
        ImGui::TreePop();
    }
}

local void RenderSizesViewerRowRegular(DebugRegularAllocation *regular, const char *debugID)
{
    ImGui::TableNextColumn();
    ImGui::Text("%u + %u", regular->offset, regular->size);
    ImGui::TableNextColumn();
    ImGui::TextDisabled("--");
    ImGui::TableNextColumn();
    ImGui::TextDisabled("--");
    ImGui::TableNextColumn();
    ImGui::TextDisabled("--");
    ImGui::TableNextColumn();
    ImGui::Text("%s", debugID);
}

local void RenderSizesViewerRowAllocation(DebugGeneralAllocation *allocation)
{
    if (allocation->type == allocationType_arena)
    {
        RenderSizesViewerRowArena(&allocation->arena, allocation->debugID, allocation->name);
    }
    else if (allocation->type == allocationType_regular)
    {
        RenderSizesViewerRowRegular(&allocation->regular, allocation->debugID);
    }
}

local void RenderSizesViewerAllocations(DebugAllocations *allocations)
{
    for (DebugGeneralAllocation *allocation = allocations->sentinel->next;
         allocation != allocations->sentinel;
         allocation = allocation->next)
    {
        ImGui::TableNextRow();

        RenderSizesViewerRowAllocation(allocation);
    }
}

#endif

// NOTE(marvin): ECS editor functionality in the editor system.
void RenderOverlay(GameState &gameState)
{
    b32 shouldShowOverlay = gameState.isEditor;

#if SKL_INTERNAL
    shouldShowOverlay |= true;
#endif

    if (shouldShowOverlay)
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

        // NOTE(marvin): Tabs
        ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_None;
        if (ImGui::BeginTabBar("Overlay Options", tabBarFlags))
        {
            if (gameState.isEditor && ImGui::BeginTabItem("ECS Editor"))
            {
                gameState.overlayMode = overlayMode_ecsEditor;
                ImGui::EndTabItem();
            }
#if SKL_DEBUG_MEMORY_VIEWER
            if (ImGui::BeginTabItem("Memory"))
            {
                gameState.overlayMode = overlayMode_memory;
                ImGui::EndTabItem();
            }
#endif
            ImGui::EndTabBar();
        }

#if SKL_DEBUG_MEMORY_VIEWER
        // NOTE(marvin): Tab content.
        if (gameState.overlayMode == overlayMode_memory)
        {
            DebugState *debugState = globalDebugState;

            if (ImGui::BeginTabBar("Memory Options", tabBarFlags))
            {
                if (ImGui::BeginTabItem("Sizes Viewer"))
                {
                    ImGuiTableFlags tableFlags =
                        ImGuiTableFlags_BordersV |
                        ImGuiTableFlags_BordersOuterH |
                        ImGuiTableFlags_Resizable |
                        ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_NoBordersInBody;

                    if (ImGui::BeginTable("Sizes Viewer Table", 5, tableFlags))
                    {
                        // NOTE(marvin): Headers
                        ImGui::TableSetupColumn("MEMORY");
                        ImGui::TableSetupColumn("PERCENT");
                        ImGui::TableSetupColumn("ALLOCATIONS");
                        ImGui::TableSetupColumn("NAME");
                        ImGui::TableSetupColumn("DEBUG ID");
                        ImGui::TableHeadersRow();

                        RenderSizesViewerAllocations(&debugState->targets);

                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
#endif

        ImGui::End();
    }
}
