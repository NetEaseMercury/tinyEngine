#include "IMGUIManager.hpp"
#include <direct.h>
static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension)
{
    for (const VkExtensionProperties& p : properties)
        if (strcmp(p.extensionName, extension) == 0)
            return true;
    return false;
}

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

static std::string convertBackslashes(const std::string& input) {
    std::string output;
    for (auto ch : input) {
        if (ch == '\\') {
            output += "\\\\";
        }
        else {
            output += ch;
        }
    }
    return output;
}

void UIManager::initIMGUI()
{

    window = glfwCreateWindow(1280, 720, "IMGUIWindow", nullptr, nullptr);

    ImGui_ImplVulkanH_Window* wd = &MainWindowData;
    setVulkan();
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    setIMGUIVulkanWindow(wd, surface, w, h);
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    g_io = io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = Instance;
    init_info.PhysicalDevice = PhysicalDevice;
    init_info.Device = Device;
    init_info.QueueFamily = QueueFamily;
    init_info.Queue = Queue;
    init_info.PipelineCache = PipelineCache;
    init_info.DescriptorPool = DescriptorPool;
    init_info.RenderPass = wd->RenderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);


}

void UIManager::setVulkanInstance(const VkInstance& instance,VkAllocationCallbacks* allocator)
{
    Instance = instance;
    Allocator = allocator;
}

void UIManager::setIMGUIVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
    wd->Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, QueueFamily, wd->Surface, &res);
    if (res != VK_TRUE)
    {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(PhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(PhysicalDevice, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
    //printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(Instance, PhysicalDevice, Device, wd, QueueFamily, Allocator, width, height, MinImageCount);

}

void UIManager::FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
    VkResult err;

    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    err = vkAcquireNextImageKHR(Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        SwapChainRebuild = true;
        return;
    }

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(Device, 1, &fd->Fence, VK_TRUE, 1000000000);    // wait indefinitely instead of periodically checking

        err = vkResetFences(Device, 1, &fd->Fence);
    }
    {
        err = vkResetCommandPool(Device, fd->CommandPool, 0);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

        // Record dear imgui primitives into command buffer
        ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

        // Submit command buffer
        vkCmdEndRenderPass(fd->CommandBuffer);
        {
            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = &image_acquired_semaphore;
            info.pWaitDstStageMask = &wait_stage;
            info.commandBufferCount = 1;
            info.pCommandBuffers = &fd->CommandBuffer;
            info.signalSemaphoreCount = 1;
            info.pSignalSemaphores = &render_complete_semaphore;

            err = vkEndCommandBuffer(fd->CommandBuffer);
            check_vk_result(err);
            err = vkQueueSubmit(Queue, 1, &info, fd->Fence);
            check_vk_result(err);
        }
    }
}

void UIManager::FramePresent(ImGui_ImplVulkanH_Window* wd)
{
    if (SwapChainRebuild)
        return;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        SwapChainRebuild = true;
        return;
    }
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
}

void UIManager::setPhysicalDevice(const VkDevice& device, const VkPhysicalDevice& physicalDevice)
{
    PhysicalDevice = physicalDevice;
    Device = device;
    vkGetDeviceQueue(Device, QueueFamily, 0, &Queue);
}

void UIManager::setVulkan()
{
    // Select graphics queue family
    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, nullptr);
        VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
        vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &count, queues);
        for (uint32_t i = 0; i < count; i++)
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                QueueFamily = i;
                break;
            }
        free(queues);
        IM_ASSERT(QueueFamily != (uint32_t)-1);
    }

    // Create Descriptor Pool
    // The example only requires a single combined image sampler descriptor for the font image and only uses one descriptor set (for that)
    // If you wish to load e.g. additional textures you may need to alter pools sizes.
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1;
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        auto err = vkCreateDescriptorPool(Device, &pool_info, Allocator, &DescriptorPool);
        check_vk_result(err);
    }
    // 创建surface
    {
        VkResult err = glfwCreateWindowSurface(Instance, window, Allocator, &surface);
        check_vk_result(err);
    }
}

void UIManager::setRefreshVulkanStatus(bool status)
{
    refreshVulkanRender = status;
}

void UIManager::setModelDefaultPath()
{
    char currentPath[1024];
    if (getcwd(currentPath, sizeof(currentPath))) {
        snprintf(VertexShaderPath, sizeof(VertexShaderPath), "%s/res/shaders/", currentPath);
        snprintf(FragShdaerPath, sizeof(FragShdaerPath), "%s/res/shaders/", currentPath);
        snprintf(ModelPath, sizeof(ModelPath), "%s/res/models/", currentPath);
        snprintf(TexturePath, sizeof(TexturePath), "%s/res/textures/", currentPath);
        snprintf(currentVertexShaderPath, sizeof(currentVertexShaderPath), "%svert.spv", VertexShaderPath);
        snprintf(currentFragShdaerPath, sizeof(currentFragShdaerPath), "%sfrag.spv", FragShdaerPath);
        snprintf(currentModelPath, sizeof(currentModelPath), "%scyber_room.obj", ModelPath);
        snprintf(currentTexturePath, sizeof(currentTexturePath), "%scyber_room.png", TexturePath);
        vertexShaderPath = convertBackslashes(currentVertexShaderPath);
        fragShaderPath = convertBackslashes(currentFragShdaerPath);
        modelPath = convertBackslashes(currentModelPath);
        texturePath = convertBackslashes(currentTexturePath);
    }
}

bool UIManager::refreshVulkanShader()
{
    return refreshVulkanRender;
}

float UIManager::updateSpeed()
{
    return speed;
}

void UIManager::startNewFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    int fb_width, fb_height;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    if (fb_width > 0 && fb_height > 0 && (SwapChainRebuild || MainWindowData.Width != fb_width || MainWindowData.Height != fb_height))
    {
        ImGui_ImplVulkan_SetMinImageCount(MinImageCount);
        ImGui_ImplVulkanH_CreateOrResizeWindow(Instance, PhysicalDevice, Device, &MainWindowData, QueueFamily, Allocator, fb_width, fb_height, MinImageCount);
        MainWindowData.FrameIndex = 0;
        SwapChainRebuild = false;
    }
    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
        return;
    }
    ImGui_ImplVulkanH_Window* wd = &MainWindowData;
    static float f = 0.0f;
    static int counter = 0;

    ImGui::Begin("tinyEngineOperationWindow"); 
    std::string prompt= R"(
        You can move by pressing w, a, s, d. 
        You can press and hold the right mouse button to rotate the view.
    )";
    ImGui::Text(prompt.c_str());              
    ImGui::SliderFloat("Camera Move Speed", &speed, 0.0f, 1.0f);

    ImGui::ColorEdit3("clear color", (float*)&clear_color); 

    // 创建一个静态变量来存储选中项的索引
    static int selectedIndex = 0;

    // 定义下拉选项的内容
    const char* items[] = { "cyberRoom", "fantasyGameInn", "vikingRoom"};
    const int itemCount = IM_ARRAYSIZE(items);

    // 创建下拉选项框
    ImGui::Combo("下拉选项框", &selectedIndex, items, itemCount);
    if (selectedIndex != currentIndex) {
        switch (selectedIndex)
        {
        case 0:
            snprintf(currentVertexShaderPath, sizeof(currentVertexShaderPath), "%svert.spv", VertexShaderPath);
            snprintf(currentFragShdaerPath, sizeof(currentFragShdaerPath), "%sfrag.spv", FragShdaerPath);
            snprintf(currentModelPath, sizeof(currentModelPath), "%scyber_room.obj", ModelPath);
            snprintf(currentTexturePath, sizeof(currentTexturePath), "%scyber_room.png", TexturePath);
            setRefreshVulkanStatus(true);
            break;
        case 1:
            snprintf(currentVertexShaderPath, sizeof(currentVertexShaderPath), "%svert.spv", VertexShaderPath);
            snprintf(currentFragShdaerPath, sizeof(currentFragShdaerPath), "%sfrag.spv", FragShdaerPath);
            snprintf(currentModelPath, sizeof(currentModelPath), "%sfantasy_game_inn.obj", ModelPath);
            snprintf(currentTexturePath, sizeof(currentTexturePath), "%sfantasy_game_inn.png", TexturePath);
            setRefreshVulkanStatus(true);
            break;
        case 2:
            snprintf(currentVertexShaderPath, sizeof(currentVertexShaderPath), "%svert.spv", VertexShaderPath);
            snprintf(currentFragShdaerPath, sizeof(currentFragShdaerPath), "%sfrag.spv", FragShdaerPath);
            snprintf(currentModelPath, sizeof(currentModelPath), "%sviking_room.obj", ModelPath);
            snprintf(currentTexturePath, sizeof(currentTexturePath), "%sviking_room.png", TexturePath);
            setRefreshVulkanStatus(true);
            break;
        default:
            break;
        }
        vertexShaderPath = convertBackslashes(currentVertexShaderPath);
        fragShaderPath = convertBackslashes(currentFragShdaerPath);
        modelPath = convertBackslashes(currentModelPath);
        texturePath = convertBackslashes(currentTexturePath);
        currentIndex = selectedIndex;
    }

    ImGui::End();

    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized)
    {
        wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
        wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
        wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
        wd->ClearValue.color.float32[3] = clear_color.w;
        FrameRender(wd, draw_data);
        FramePresent(wd);
    }
}

void UIManager::cleanUp()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ImGui_ImplVulkanH_DestroyWindow(Instance, Device, &MainWindowData, Allocator);
    vkDestroyDescriptorPool(Device, DescriptorPool, Allocator);
}
