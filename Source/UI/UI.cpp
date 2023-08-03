#include "UI.h"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include <stdio.h>          // printf, fprintf
#include <stdlib.h>         // abort

#include "../VulkanRenderer.h"
#include "../VulkanUtils.h"

static PhysicalDeviceInfo g_PhysicalDeviceInfo;

void UI::Init(VulkanRenderer* pRenderer)
{
    ASSERT(pRenderer, "Invalid vulkan renderer");
    m_pRenderer = pRenderer;

    m_UIRenderPass = m_pRenderer->GetRenderPass();

    g_PhysicalDeviceInfo = m_pRenderer->GetPhysicalDeviceInfo();

    m_UISurfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
    m_UISurfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    m_UIPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    CreateUIDescriptorPool();
    CreateUIDescriptorSetLayout();

    //设置ImGui上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;

    //设置ImGui风格
    ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForVulkan(m_pRenderer->GetWindow(), true);

    CreateUIShaderModule();
    CreateUIPipelineLayout();
    CreateUIPipeline();

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    io.Fonts->AddFontFromFileTTF("./Submodule/ImGui/misc/fonts/Roboto-Medium.ttf", 26.0f);
    //io.Fonts->AddFontFromFileTTF("./Submodule/ImGui/misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("./Submodule/ImGui/misc/fonts/DroidSans.ttf", 16.0f);
    //io.FontDefault = io.Fonts->AddFontFromFileTTF("./Submodule/ImGui/misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);
    // 
    // Upload Fonts
    //{
        // Use any command queue
    CreateUIFontSampler();
    CreateUIFontImageAndBindMemory();
    CreateUIFontImageView();
    CreateUIFontDescriptorSet();

    UploadFont();

    io.Fonts->SetTexID((ImTextureID)m_UIFontDescriptorSet);
    //}
}

void UI::StartNewFrame()
{
    // Start the Dear ImGui frame
    //ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

static void setStyle(uint32_t index)
{
    switch (index)
    {
    case 0:
    {
        ImGuiStyle& style = ImGui::GetStyle();

        auto vulkanStyle = ImGui::GetStyle();
        vulkanStyle.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
        vulkanStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
        vulkanStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        vulkanStyle.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        vulkanStyle.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

        style = vulkanStyle;
        break;
    }
    case 1:
        ImGui::StyleColorsClassic();
        break;
    case 2:
        ImGui::StyleColorsDark();
        break;
    case 3:
        ImGui::StyleColorsLight();
        break;
    }
}

void UI::Draw()
{
    //static int selectedStyle = 0;
    //ImGui::Begin("Select Mode");
    //if (ImGui::Combo("UI style", &selectedStyle, "Vulkan\0Classic\0Dark\0Light\0")) {
    //    setStyle(selectedStyle);
    //}
    //ImGui::End();

    ImGui::Begin("Physical Device");
    ImGui::Text("Name: %s", g_PhysicalDeviceInfo.properties.deviceName);
    ImGui::Text("Type: %s", g_PhysicalDeviceInfo.strDeviceTypeName.c_str());
    ImGui::End();

    ImGui::Begin("Stat");
    ImGui::Text("FPS: %d", m_pRenderer->GetFPS());

    ImGui::Text("Camera Pos: %.2f, %.2f, %.2f", m_pRenderer->GetCameraPosition().x, m_pRenderer->GetCameraPosition().y, m_pRenderer->GetCameraPosition().z);

    static float fLod = 0.f;
    if (ImGui::SliderFloat("Texture Lod", &fLod, 0.f, static_cast<float>(m_pRenderer->GetTextureMaxLod()), "%.1f"))
        m_pRenderer->SetTextureLod(fLod);

    if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enable", m_pRenderer->GetSkyboxEnable());
        ImGui::DragFloat("Rotate Speed", m_pRenderer->GetSkyboxRotateSpeed(), 0.01f, 1.f, 10.f, "%.1f");
    }

    if (ImGui::CollapsingHeader("Mesh Grid", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Enable", m_pRenderer->GetMeshGridEnable());
        ImGui::DragFloat("Size", m_pRenderer->GetMeshGridSize(), 1.f, 100.f, 1000.f, "%.1f");
        ImGui::DragFloat("Split", m_pRenderer->GetMeshGridSplit(), 1.f, 1.f, 100.f, "%.1f");
        ImGui::DragFloat("Line Width", m_pRenderer->GetMeshGridLineWidth(), 1.f, 1.f, 20.f, "%.1f");
    }
    ImGui::End();

    ImGui::Begin("Blinn Phong");
    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto pLight = m_pRenderer->GetBlinnPhongPointLight();
        ImGui::SeparatorText("Color");
        ImGui::ColorPicker4("Ambient", (float*)&(pLight->ambient), ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoInputs);
        ImGui::ColorPicker4("Diffuse", (float*)&(pLight->diffuse), ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoInputs);
        ImGui::ColorPicker4("Specular", (float*)&(pLight->specular), ImGuiColorEditFlags_NoOptions | ImGuiColorEditFlags_NoInputs);
        
        ImGui::SeparatorText("Attenuation");

    }

    ImGui::ShowDemoWindow();

    ImGui::End();
}

void UI::Render(UINT uiIdx)
{
    Draw();

    VkCommandBuffer& command_buffer = m_pRenderer->GetCommandBuffer(uiIdx);
    
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();

    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    if (draw_data->TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        if (m_UIVertexBuffer == VK_NULL_HANDLE || m_UIVertexBufferSize < vertex_size)
            CreateOrResizeBuffer(m_UIVertexBuffer, m_UIVertexBufferMemory, m_UIVertexBufferSize, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        if (m_UIIndexBuffer == VK_NULL_HANDLE || m_UIIndexBufferSize < index_size)
            CreateOrResizeBuffer(m_UIIndexBuffer, m_UIIndexBufferMemory, m_UIIndexBufferSize, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        // Upload vertex/index data into a single contiguous GPU buffer
        ImDrawVert* vtx_dst = nullptr;
        ImDrawIdx* idx_dst = nullptr;
        VULKAN_ASSERT(vkMapMemory(m_pRenderer->GetLogicalDevice(), m_UIVertexBufferMemory, 0, m_UIVertexBufferSize, 0, (void**)(&vtx_dst)));
        VULKAN_ASSERT(vkMapMemory(m_pRenderer->GetLogicalDevice(), m_UIIndexBufferMemory, 0, m_UIIndexBufferSize, 0, (void**)(&idx_dst)));

        for (int n = 0; n < draw_data->CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = draw_data->CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }
        VkMappedMemoryRange range[2] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = m_UIVertexBufferMemory;
        range[0].size = VK_WHOLE_SIZE;
        range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[1].memory = m_UIIndexBufferMemory;
        range[1].size = VK_WHOLE_SIZE;
        VULKAN_ASSERT(vkFlushMappedMemoryRanges(m_pRenderer->GetLogicalDevice(), 2, range));
        vkUnmapMemory(m_pRenderer->GetLogicalDevice(), m_UIVertexBufferMemory);
        vkUnmapMemory(m_pRenderer->GetLogicalDevice(), m_UIIndexBufferMemory);
    }

    // Setup desired Vulkan state
    SetupRenderState(draw_data, m_UIPipeline, command_buffer, fb_width, fb_height);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    SetupRenderState(draw_data, m_UIPipeline, command_buffer, fb_width, fb_height);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                VkRect2D scissor;
                scissor.offset.x = (int32_t)(clip_min.x);
                scissor.offset.y = (int32_t)(clip_min.y);
                scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
                scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
                vkCmdSetScissor(command_buffer, 0, 1, &scissor);

                // Bind DescriptorSet with font or user texture
                VkDescriptorSet desc_set[1] = { (VkDescriptorSet)pcmd->TextureId };
                if (sizeof(ImTextureID) < sizeof(ImU64))
                {
                    // We don't support texture switches if ImTextureID hasn't been redefined to be 64-bit. Do a flaky check that other textures haven't been used.
                    ASSERT(pcmd->TextureId == (ImTextureID)m_UIFontDescriptorSet);
                    desc_set[0] = m_UIFontDescriptorSet;
                }
                vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_UIPipelineLayout, 0, 1, desc_set, 0, nullptr);

                // Draw
                vkCmdDrawIndexed(command_buffer, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Note: at this point both vkCmdSetViewport() and vkCmdSetScissor() have been called.
    // Our last values will leak into user/application rendering IF:
    // - Your app uses a pipeline with VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR dynamic state
    // - And you forgot to call vkCmdSetViewport() and vkCmdSetScissor() yourself to explicitly set that state.
    // If you use VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR you are responsible for setting the values before rendering.
    // In theory we should aim to backup/restore those values but I am not sure this is possible.
    // We perform a call to vkCmdSetScissor() to set back a full viewport which is likely to fix things for 99% users but technically this is not perfect. (See github #4644)
    VkRect2D scissor = { { 0, 0 }, { (uint32_t)fb_width, (uint32_t)fb_height } };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void UI::Resize()
{
    ImGuiIO& io = ImGui::GetIO();

    auto& extent = m_pRenderer->GetSwapChainExtent2D();
    io.DisplaySize = ImVec2(static_cast<float>(extent.width), static_cast<float>(extent.height));
}

void UI::Clean()
{
    vkDestroyDescriptorPool(m_pRenderer->GetLogicalDevice(), m_UIDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_pRenderer->GetLogicalDevice(), m_UIDescriptorSetLayout, nullptr);

    vkDestroyPipelineLayout(m_pRenderer->GetLogicalDevice(), m_UIPipelineLayout, nullptr);
    vkDestroyPipeline(m_pRenderer->GetLogicalDevice(), m_UIPipeline, nullptr);

    vkDestroyShaderModule(m_pRenderer->GetLogicalDevice(), m_UIVertexShaderModule, nullptr);
    vkDestroyShaderModule(m_pRenderer->GetLogicalDevice(), m_UIVertexSRGBShaderModule, nullptr);
    vkDestroyShaderModule(m_pRenderer->GetLogicalDevice(), m_UIFragmentShaderModule, nullptr);
    
    vkDestroyBuffer(m_pRenderer->GetLogicalDevice(), m_UIVertexBuffer, nullptr);
    vkFreeMemory(m_pRenderer->GetLogicalDevice(), m_UIVertexBufferMemory, nullptr);
    vkDestroyBuffer(m_pRenderer->GetLogicalDevice(), m_UIIndexBuffer, nullptr);
    vkFreeMemory(m_pRenderer->GetLogicalDevice(), m_UIIndexBufferMemory, nullptr);

    vkDestroySampler(m_pRenderer->GetLogicalDevice(), m_UIFontSampler, nullptr);
    vkDestroyImage(m_pRenderer->GetLogicalDevice(), m_UIFontImage, nullptr);
    vkDestroyImageView(m_pRenderer->GetLogicalDevice(), m_UIFontImageView, nullptr);
    vkFreeMemory(m_pRenderer->GetLogicalDevice(), m_UIFontImageMemory, nullptr);

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UI::UploadFont()
{
    unsigned char* pixels;
    int width, height;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    size_t upload_size = width * height * 4 * sizeof(char);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = upload_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VULKAN_ASSERT(vkCreateBuffer(m_pRenderer->GetLogicalDevice(), &buffer_info, nullptr, &stagingBuffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(m_pRenderer->GetLogicalDevice(), stagingBuffer, &req);
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = m_pRenderer->FindSuitableMemoryTypeIndex(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VULKAN_ASSERT(vkAllocateMemory(m_pRenderer->GetLogicalDevice(), &alloc_info, nullptr, &stagingBufferMemory));
    VULKAN_ASSERT(vkBindBufferMemory(m_pRenderer->GetLogicalDevice(), stagingBuffer, stagingBufferMemory, 0));

    char* map = nullptr;
    VULKAN_ASSERT(vkMapMemory(m_pRenderer->GetLogicalDevice(), stagingBufferMemory, 0, upload_size, 0, (void**)(&map)));
    memcpy(map, pixels, upload_size);
    VkMappedMemoryRange range[1] = {};
    range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range[0].memory = stagingBufferMemory;
    range[0].size = upload_size;
    VULKAN_ASSERT(vkFlushMappedMemoryRanges(m_pRenderer->GetLogicalDevice(), 1, range));
    vkUnmapMemory(m_pRenderer->GetLogicalDevice(), stagingBufferMemory);

    VkCommandBuffer commandBuffer = m_pRenderer->BeginSingleTimeCommandBuffer();

    VkImageMemoryBarrier copy_barrier[1] = {};
    copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copy_barrier[0].image = m_UIFontImage;
    copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_barrier[0].subresourceRange.levelCount = 1;
    copy_barrier[0].subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, copy_barrier);

    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, m_UIFontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier use_barrier[1] = {};
    use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    use_barrier[0].image = m_UIFontImage;
    use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    use_barrier[0].subresourceRange.levelCount = 1;
    use_barrier[0].subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, use_barrier);

    m_pRenderer->EndSingleTimeCommandBuffer(commandBuffer);

    vkDeviceWaitIdle(m_pRenderer->GetLogicalDevice());

    vkDestroyBuffer(m_pRenderer->GetLogicalDevice(), stagingBuffer, nullptr);
    vkFreeMemory(m_pRenderer->GetLogicalDevice(), stagingBufferMemory, nullptr);
}

void UI::CreateUIDescriptorPool()
{
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (UINT)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    VULKAN_ASSERT(vkCreateDescriptorPool(m_pRenderer->GetLogicalDevice(), &pool_info, nullptr, &m_UIDescriptorPool), "Create ImGui descriptor pool failed");
}

void UI::CreateUIDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding binding[1] = {};
    binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding[0].descriptorCount = 1;
    binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = binding;
    VULKAN_ASSERT(vkCreateDescriptorSetLayout(m_pRenderer->GetLogicalDevice(), &info, nullptr, &m_UIDescriptorSetLayout), "Create ImGui descriptorSetLayout failed");
}

void UI::CreateUIFontDescriptorSet()
{
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_UIDescriptorPool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &m_UIDescriptorSetLayout;
    VULKAN_ASSERT(vkAllocateDescriptorSets(m_pRenderer->GetLogicalDevice(), &alloc_info, &m_UIFontDescriptorSet));

    VkDescriptorImageInfo desc_image[1] = {};
    desc_image[0].sampler = m_UIFontSampler;
    desc_image[0].imageView = m_UIFontImageView;
    desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write_desc[1] = {};
    write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_desc[0].dstSet = m_UIFontDescriptorSet;
    write_desc[0].descriptorCount = 1;
    write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_desc[0].pImageInfo = desc_image;
    vkUpdateDescriptorSets(m_pRenderer->GetLogicalDevice(), 1, write_desc, 0, nullptr);
}

void UI::CreateOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory, VkDeviceSize& p_buffer_size, size_t new_size, VkBufferUsageFlagBits usage)
{
    vkDeviceWaitIdle(m_pRenderer->GetLogicalDevice());
    if (buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(m_pRenderer->GetLogicalDevice(), buffer, nullptr);
    if (buffer_memory != VK_NULL_HANDLE)
        vkFreeMemory(m_pRenderer->GetLogicalDevice(), buffer_memory, nullptr);

    //VkDeviceSize vertex_buffer_size_aligned = ((new_size - 1) / bd->BufferMemoryAlignment + 1) * bd->BufferMemoryAlignment;
    VkDeviceSize vertex_buffer_size_aligned = new_size;
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = vertex_buffer_size_aligned;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VULKAN_ASSERT(vkCreateBuffer(m_pRenderer->GetLogicalDevice(), &buffer_info, nullptr, &buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(m_pRenderer->GetLogicalDevice(), buffer, &req);
    //bd->BufferMemoryAlignment = (bd->BufferMemoryAlignment > req.alignment) ? bd->BufferMemoryAlignment : req.alignment;
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = m_pRenderer->FindSuitableMemoryTypeIndex(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VULKAN_ASSERT(vkAllocateMemory(m_pRenderer->GetLogicalDevice(), &alloc_info, nullptr, &buffer_memory));

    VULKAN_ASSERT(vkBindBufferMemory(m_pRenderer->GetLogicalDevice(), buffer, buffer_memory, 0));

    p_buffer_size = req.size;
}

void UI::SetupRenderState(ImDrawData* draw_data, VkPipeline pipeline, VkCommandBuffer command_buffer, int fb_width, int fb_height)
{
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_UIPipeline);

    if (draw_data->TotalVtxCount > 0)
    {
        VkBuffer vertex_buffers[1] = { m_UIVertexBuffer };
        VkDeviceSize vertex_offset[1] = { 0 };
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offset);
        vkCmdBindIndexBuffer(command_buffer, m_UIIndexBuffer, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)fb_width;
    viewport.height = (float)fb_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    float scale[2];
    scale[0] = 2.0f / draw_data->DisplaySize.x;
    scale[1] = 2.0f / draw_data->DisplaySize.y;
    float translate[2];
    translate[0] = -1.0f - draw_data->DisplayPos.x * scale[0];
    translate[1] = -1.0f - draw_data->DisplayPos.y * scale[1];
    vkCmdPushConstants(command_buffer, m_UIPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
    vkCmdPushConstants(command_buffer, m_UIPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
}

void UI::CreateUIPipelineLayout()
{
    // Constants: we are using 'vec2 offset' and 'vec2 scale' instead of a full 3d projection matrix
    VkPushConstantRange push_constants[1] = {};
    push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constants[0].offset = sizeof(float) * 0;
    push_constants[0].size = sizeof(float) * 4;
    VkDescriptorSetLayout set_layout[1] = { m_UIDescriptorSetLayout };
    VkPipelineLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = push_constants;
    VULKAN_ASSERT(vkCreatePipelineLayout(m_pRenderer->GetLogicalDevice(), &layout_info, nullptr, &m_UIPipelineLayout), "Create ImGui pipelineLayout failed");
}

void UI::CreateUIPipeline()
{
    ImGuiIO& io = ImGui::GetIO();

    VkPipelineShaderStageCreateInfo stage[2] = {};
    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage[0].module = io.ConfigFlags & ImGuiConfigFlags_IsSRGB ? m_UIVertexSRGBShaderModule : m_UIVertexShaderModule;
    stage[0].pName = "main";
    stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage[1].module = m_UIFragmentShaderModule;
    stage[1].pName = "main";

    VkVertexInputBindingDescription binding_desc[1] = {};
    binding_desc[0].stride = sizeof(ImDrawVert);
    binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute_desc[3] = {};
    attribute_desc[0].location = 0;
    attribute_desc[0].binding = binding_desc[0].binding;
    attribute_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[0].offset = IM_OFFSETOF(ImDrawVert, pos);
    attribute_desc[1].location = 1;
    attribute_desc[1].binding = binding_desc[0].binding;
    attribute_desc[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[1].offset = IM_OFFSETOF(ImDrawVert, uv);
    attribute_desc[2].location = 2;
    attribute_desc[2].binding = binding_desc[0].binding;
    attribute_desc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attribute_desc[2].offset = IM_OFFSETOF(ImDrawVert, col);

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = 1;
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = 3;
    vertex_info.pVertexAttributeDescriptions = attribute_desc;

    VkPipelineInputAssemblyStateCreateInfo ia_info = {};
    ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.cullMode = VK_CULL_MODE_NONE;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_attachment[1] = {};
    color_attachment[0].blendEnable = VK_TRUE;
    color_attachment[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = (uint32_t)IM_ARRAYSIZE(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stage;
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &ia_info;
    info.pViewportState = &viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &ms_info;
    info.pDepthStencilState = &depth_info;
    info.pColorBlendState = &blend_info;
    info.pDynamicState = &dynamic_state;
    info.layout = m_UIPipelineLayout;
    info.renderPass = m_UIRenderPass;
    info.subpass = 0;

    VULKAN_ASSERT(vkCreateGraphicsPipelines(m_pRenderer->GetLogicalDevice(), VK_NULL_HANDLE, 1, &info, nullptr, &m_UIPipeline), "Create ImGui pipeline failed");
}

void UI::CreateUIShaderModule()
{
    auto vertShaderData = DZW_VulkanUtils::ReadShaderFile("./Assert/Shader/ImGui/imgui_vert.spv");
    auto sRGBVertShaderData = DZW_VulkanUtils::ReadShaderFile("./Assert/Shader/ImGui/imgui_vert_srgb.spv");
    auto fragShaderData = DZW_VulkanUtils::ReadShaderFile("./Assert/Shader/ImGui/imgui_frag.spv");

    m_UIVertexShaderModule = DZW_VulkanUtils::CreateShaderModule(m_pRenderer->GetLogicalDevice(), vertShaderData);
    m_UIVertexSRGBShaderModule = DZW_VulkanUtils::CreateShaderModule(m_pRenderer->GetLogicalDevice(), sRGBVertShaderData);
    m_UIFragmentShaderModule = DZW_VulkanUtils::CreateShaderModule(m_pRenderer->GetLogicalDevice(), fragShaderData);
}

void UI::CreateUIFontSampler()
{
    // Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling.
    VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.minLod = -1000;
    info.maxLod = 1000;
    info.maxAnisotropy = 1.0f;
    VULKAN_ASSERT(vkCreateSampler(m_pRenderer->GetLogicalDevice(), &info, nullptr, &m_UIFontSampler), "Create ImGui Font Sampler failed");
}

void UI::CreateUIFontImageAndBindMemory()
{
    unsigned char* pixels;
    int width, height;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VULKAN_ASSERT(vkCreateImage(m_pRenderer->GetLogicalDevice(), &info, nullptr, &m_UIFontImage), "Create ImGui Font Image failed");

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(m_pRenderer->GetLogicalDevice(), m_UIFontImage, &req);
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;



    alloc_info.memoryTypeIndex = m_pRenderer->FindSuitableMemoryTypeIndex(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VULKAN_ASSERT(vkAllocateMemory(m_pRenderer->GetLogicalDevice(), &alloc_info, nullptr, &m_UIFontImageMemory));
    VULKAN_ASSERT(vkBindImageMemory(m_pRenderer->GetLogicalDevice(), m_UIFontImage, m_UIFontImageMemory, 0));
}

void UI::CreateUIFontImageView()
{
    VkImageViewCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = m_UIFontImage;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.layerCount = 1;
    VULKAN_ASSERT(vkCreateImageView(m_pRenderer->GetLogicalDevice(), &info, nullptr, &m_UIFontImageView));
}

