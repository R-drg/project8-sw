// Independent diagnostic client. It does not emulate Xenos or contain game code.
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_VI_NN
#include <vulkan/vulkan.h>
#include <switch.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

// Match NXVK's nvk_bringup_ex startup contract before libnx initialization.
extern "C" {
u32 __nx_applet_type = AppletType_Application;
size_t __nx_heap_size = 0;
}
extern "C" PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance, const char*);
#ifdef SWITCH_COMBINED_PROBE
bool run_memory_probe(FILE*);
#endif
static FILE* logfile;
template<class T> static void property(const char* name, T value) {
    if constexpr (std::is_floating_point_v<T>) std::fprintf(logfile,"property %s=%g\n",name,double(value));
    else if constexpr (std::is_signed_v<T>) std::fprintf(logfile,"property %s=%lld\n",name,static_cast<long long>(value));
    else std::fprintf(logfile,"property %s=%llu\n",name,static_cast<unsigned long long>(value));
}
template<class T, size_t N> static void property(const char* name, const T (&values)[N]) {
    for(size_t i=0;i<N;++i) { char indexed[160]; std::snprintf(indexed,sizeof(indexed),"%s[%zu]",name,i);property(indexed,values[i]); }
}
static void check(VkResult r, const char* operation) {
    std::fprintf(logfile, "%s: %d\n", operation, r); std::fflush(logfile);
    if (r != VK_SUCCESS) throw std::runtime_error(operation);
}
#define CHECK(call) check(call, #call)
static PFN_vkVoidFunction resolve(VkInstance instance, const char* name) {
    auto address = vk_icdGetInstanceProcAddr(instance, name);
    std::fprintf(logfile, "ICD instance=%p %s=%p\n", reinterpret_cast<void*>(instance),
                 name, reinterpret_cast<void*>(address));
    std::fflush(logfile);
    return address;
}
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_message(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
    std::fprintf(logfile, "driver severity=0x%x type=0x%x %s: %s\n", severity, types,
                 data->pMessageIdName ? data->pMessageIdName : "?",
                 data->pMessage ? data->pMessage : "");
    std::fflush(logfile);
    return VK_FALSE;
}
#define GLOBAL(name) auto name = reinterpret_cast<PFN_##name>(resolve(nullptr, #name)); if (!name) throw std::runtime_error(#name)
#define INSTANCE(name) auto name = reinterpret_cast<PFN_##name>(resolve(instance, #name)); if (!name) throw std::runtime_error(#name)
#define DEVICE(name) auto name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name)); if (!name) throw std::runtime_error(#name)

template<class T, class F> std::vector<T> enumerate(F call) {
    for (;;) {
        uint32_t count = 0; CHECK(call(&count, static_cast<T*>(nullptr)));
        if (!count) return {};
        std::vector<T> values(count);
        VkResult r = call(&count, values.data());
        if (r == VK_INCOMPLETE) continue;
        CHECK(r); values.resize(count); return values;
    }
}
static bool extension(const std::vector<VkExtensionProperties>& exts, const char* name) {
    return std::any_of(exts.begin(), exts.end(), [&](const auto& e) { return !std::strcmp(e.extensionName, name); });
}

struct Probe {
    VkDebugUtilsMessengerEXT messenger{};
    VkInstance instance{}; VkDevice device{}; VkSurfaceKHR surface{};
    VkSwapchainKHR swapchain{}; VkRenderPass renderpass{}; VkPipelineLayout layout{};
    VkPipeline pipeline{}; VkCommandPool pool{}; VkSemaphore acquire{}; VkSemaphore rendered{};
    VkFence fence{}; VkShaderModule vertex{}, fragment{};
    std::vector<VkImageView> views; std::vector<VkFramebuffer> frames;
    ~Probe() {
        if (device) {
            auto get = reinterpret_cast<PFN_vkGetDeviceProcAddr>(vk_icdGetInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
#define DESTROY(name, object) if (object) { auto fn = reinterpret_cast<PFN_##name>(get(device, #name)); if(fn) fn(device, object, nullptr); }
            auto idle = reinterpret_cast<PFN_vkDeviceWaitIdle>(get(device, "vkDeviceWaitIdle"));
            if (idle) idle(device);
            DESTROY(vkDestroyCommandPool, pool);
            for (auto frame : frames) { DESTROY(vkDestroyFramebuffer, frame); }
            DESTROY(vkDestroyPipeline, pipeline); DESTROY(vkDestroyPipelineLayout, layout);
            DESTROY(vkDestroyShaderModule, vertex); DESTROY(vkDestroyShaderModule, fragment);
            DESTROY(vkDestroyRenderPass, renderpass);
            for (auto view : views) { DESTROY(vkDestroyImageView, view); }
            DESTROY(vkDestroySemaphore, acquire); DESTROY(vkDestroySemaphore, rendered);
            DESTROY(vkDestroyFence, fence); DESTROY(vkDestroySwapchainKHR, swapchain);
            auto destroy = reinterpret_cast<PFN_vkDestroyDevice>(get(device, "vkDestroyDevice"));
            if (destroy) destroy(device, nullptr);
#undef DESTROY
        }
        if (surface) {
            auto destroy = reinterpret_cast<PFN_vkDestroySurfaceKHR>(vk_icdGetInstanceProcAddr(instance, "vkDestroySurfaceKHR"));
            if (destroy) destroy(instance, surface, nullptr);
        }
        if (messenger) {
            auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                resolve(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy) destroy(instance, messenger, nullptr);
        }
        if (instance) {
            auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(vk_icdGetInstanceProcAddr(instance, "vkDestroyInstance"));
            if (destroy) destroy(instance, nullptr);
        }
    }
    void run() {
        GLOBAL(vkEnumerateInstanceExtensionProperties);
        auto ie = enumerate<VkExtensionProperties>([&](auto* n, auto* p) { return vkEnumerateInstanceExtensionProperties(nullptr, n, p); });
        for (const auto& e : ie) std::fprintf(logfile, "instance extension %s %u\n", e.extensionName, e.specVersion);
        const char* instance_extensions[] = {"VK_KHR_surface", "VK_NN_vi_surface", "VK_EXT_debug_utils"};
        for (auto name : instance_extensions) if (!extension(ie, name)) throw std::runtime_error(name);
        GLOBAL(vkCreateInstance);
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Project8 feasibility probe"; app.apiVersion = VK_API_VERSION_1_3;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo = &app; ici.enabledExtensionCount = 3; ici.ppEnabledExtensionNames = instance_extensions;
        VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback = debug_message;
        ici.pNext = &debug;
        CHECK(vkCreateInstance(&ici, nullptr, &instance));
        INSTANCE(vkCreateDebugUtilsMessengerEXT);
        CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debug, nullptr, &messenger));
        INSTANCE(vkEnumeratePhysicalDevices); INSTANCE(vkEnumerateDeviceExtensionProperties);
        INSTANCE(vkGetPhysicalDeviceProperties); INSTANCE(vkGetPhysicalDeviceFeatures);
        INSTANCE(vkGetPhysicalDeviceFeatures2); INSTANCE(vkGetPhysicalDeviceFormatProperties);
        INSTANCE(vkGetPhysicalDeviceImageFormatProperties);
        auto devices = enumerate<VkPhysicalDevice>([&](auto* n, auto* p) { VkResult r = vkEnumeratePhysicalDevices(instance, n, p);
            std::fprintf(logfile, "vkEnumeratePhysicalDevices(%s): VkResult=%d count=%u\n", p ? "devices" : "count", r, *n);
            std::fflush(logfile); return r; });
        if (devices.empty()) throw std::runtime_error("No physical device");
        VkPhysicalDevice physical = devices.front();
        VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(physical, &properties);
        std::fprintf(logfile, "device=%s api=%u.%u.%u driver=%u vendor=%x deviceID=%x type=%u\n",
            properties.deviceName, VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion),
            VK_VERSION_PATCH(properties.apiVersion), properties.driverVersion, properties.vendorID, properties.deviceID, properties.deviceType);
        // Retain the complete returned struct as a hex dump in addition to named
        // limits, so no unprinted field is silently reported as tested support.
        const auto* bytes = reinterpret_cast<const unsigned char*>(&properties);
        std::fprintf(logfile, "VkPhysicalDeviceProperties raw (%zu bytes):\n", sizeof(properties));
        for (size_t i=0; i<sizeof(properties); ++i) std::fprintf(logfile, "%02x%s", bytes[i], i%32==31 ? "\n" : "");
        std::fputc('\n',logfile);
#define PROPERTY(name) property(#name, properties.name);
#include "properties.inc"
#undef PROPERTY
        std::fprintf(logfile, "\nMSAA color=%x depth=%x stencil=%x sampledColor=%x storage=%x\n",
            properties.limits.framebufferColorSampleCounts, properties.limits.framebufferDepthSampleCounts,
            properties.limits.framebufferStencilSampleCounts, properties.limits.sampledImageColorSampleCounts,
            properties.limits.storageImageSampleCounts);
        VkPhysicalDeviceFeatures features{}; vkGetPhysicalDeviceFeatures(physical, &features);
#define FEATURE(name) std::fprintf(logfile, "feature %s=%u\n", #name, features.name);
#include "features.inc"
#undef FEATURE
        auto de = enumerate<VkExtensionProperties>([&](auto* n, auto* p) { return vkEnumerateDeviceExtensionProperties(physical, nullptr, n, p); });
        for (const auto& e : de) std::fprintf(logfile, "device extension %s %u\n", e.extensionName, e.specVersion);
        const char* optional[] = {"VK_KHR_dynamic_rendering", "VK_EXT_fragment_shader_interlock", "VK_EXT_shader_stencil_export",
            "VK_KHR_image_format_list", "VK_KHR_sampler_ycbcr_conversion", "VK_EXT_custom_border_color", "VK_EXT_robustness2", "VK_KHR_spirv_1_4"};
        for (auto name : optional) std::fprintf(logfile, "extension %s=%d (core promotion must be considered separately)\n", name, extension(de, name));
        VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceDynamicRenderingFeatures dynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
        VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT interlock{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT};
        VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcr{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES};
        VkPhysicalDeviceCustomBorderColorFeaturesEXT border{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT};
        VkPhysicalDeviceRobustness2FeaturesEXT robust{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
#define CHAIN(value) value.pNext = f2.pNext; f2.pNext = &value
        if (properties.apiVersion >= VK_API_VERSION_1_3 || extension(de, optional[0])) { CHAIN(dynamic); }
        if (extension(de, optional[1])) { CHAIN(interlock); }
        if (properties.apiVersion >= VK_API_VERSION_1_1 || extension(de, optional[4])) { CHAIN(ycbcr); }
        if (extension(de, optional[5])) { CHAIN(border); }
        if (extension(de, optional[6])) { CHAIN(robust); }
#undef CHAIN
        vkGetPhysicalDeviceFeatures2(physical, &f2);
        std::fprintf(logfile, "dynamicRendering=%u pixelInterlock=%u sampleInterlock=%u shadingRateInterlock=%u ycbcr=%u customBorderColors=%u withoutFormat=%u robustBufferAccess2=%u robustImageAccess2=%u nullDescriptor=%u\n",
            dynamic.dynamicRendering, interlock.fragmentShaderPixelInterlock, interlock.fragmentShaderSampleInterlock,
            interlock.fragmentShaderShadingRateInterlock, ycbcr.samplerYcbcrConversion, border.customBorderColors,
            border.customBorderColorWithoutFormat, robust.robustBufferAccess2, robust.robustImageAccess2, robust.nullDescriptor);
#define FORMAT(name) report_format(physical, name, #name, vkGetPhysicalDeviceFormatProperties, vkGetPhysicalDeviceImageFormatProperties);
#include "formats.inc"
#undef FORMAT
        std::fflush(logfile);
        if (!features.independentBlend || !features.fragmentStoresAndAtomics || !features.vertexPipelineStoresAndAtomics || !extension(de,"VK_KHR_swapchain"))
            throw std::runtime_error("ReXGlue mandatory feature gate failed");
        INSTANCE(vkCreateViSurfaceNN); INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);
        INSTANCE(vkGetPhysicalDeviceSurfaceSupportKHR); INSTANCE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
        INSTANCE(vkGetPhysicalDeviceSurfaceFormatsKHR); INSTANCE(vkGetPhysicalDeviceSurfacePresentModesKHR);
        VkViSurfaceCreateInfoNN sci{VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN}; sci.window = nwindowGetDefault();
        CHECK(vkCreateViSurfaceNN(instance, &sci, nullptr, &surface));
        uint32_t count=0; vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queues(count); vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues.data());
        uint32_t family = UINT32_MAX;
        for (uint32_t i=0; i<count; ++i) {
            VkBool32 present{}; CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physical, i, surface, &present));
            if (queues[i].queueCount && (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) { family = i; break; }
        }
        if (family == UINT32_MAX) throw std::runtime_error("No graphics/present queue");
        INSTANCE(vkCreateDevice); INSTANCE(vkGetDeviceProcAddr);
        float priority=1; VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex=family; qci.queueCount=1; qci.pQueuePriorities=&priority;
        VkPhysicalDeviceFeatures enabled{}; enabled.independentBlend=enabled.fragmentStoresAndAtomics=enabled.vertexPipelineStoresAndAtomics=VK_TRUE;
        const char* swapchain_extension="VK_KHR_swapchain";
        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; dci.queueCreateInfoCount=1; dci.pQueueCreateInfos=&qci;
        dci.pEnabledFeatures=&enabled; dci.enabledExtensionCount=1; dci.ppEnabledExtensionNames=&swapchain_extension;
        CHECK(vkCreateDevice(physical,&dci,nullptr,&device));
        DEVICE(vkGetDeviceQueue); VkQueue queue{}; vkGetDeviceQueue(device,family,0,&queue);
        VkSurfaceCapabilitiesKHR caps{}; CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps));
        auto formats=enumerate<VkSurfaceFormatKHR>([&](auto* n,auto* p){return vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,n,p);});
        auto modes=enumerate<VkPresentModeKHR>([&](auto* n,auto* p){return vkGetPhysicalDeviceSurfacePresentModesKHR(physical,surface,n,p);});
        for(auto m:modes) std::fprintf(logfile,"present mode=%u\n",m);
        if(formats.empty() || !(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) throw std::runtime_error("No color surface");
        auto sf=formats.front(); if(sf.format==VK_FORMAT_UNDEFINED) sf.format=VK_FORMAT_R8G8B8A8_UNORM;
        VkExtent2D extent=caps.currentExtent;
        if(extent.width==UINT32_MAX) extent={std::clamp(1280u,caps.minImageExtent.width,caps.maxImageExtent.width),std::clamp(720u,caps.minImageExtent.height,caps.maxImageExtent.height)};
        if(!extent.width || !extent.height) throw std::runtime_error("Zero surface extent");
        VkSwapchainCreateInfoKHR sw{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR}; sw.surface=surface;
        sw.minImageCount=std::max(2u,caps.minImageCount); if(caps.maxImageCount) sw.minImageCount=std::min(sw.minImageCount,caps.maxImageCount);
        sw.imageFormat=sf.format; sw.imageColorSpace=sf.colorSpace; sw.imageExtent=extent; sw.imageArrayLayers=1;
        sw.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; sw.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE; sw.preTransform=caps.currentTransform;
        sw.compositeAlpha=static_cast<VkCompositeAlphaFlagBitsKHR>(caps.supportedCompositeAlpha & (~caps.supportedCompositeAlpha+1));
        sw.presentMode=VK_PRESENT_MODE_FIFO_KHR; sw.clipped=VK_TRUE;
        DEVICE(vkCreateSwapchainKHR); CHECK(vkCreateSwapchainKHR(device,&sw,nullptr,&swapchain));
        DEVICE(vkGetSwapchainImagesKHR);
        auto images=enumerate<VkImage>([&](auto* n,auto* p){return vkGetSwapchainImagesKHR(device,swapchain,n,p);});
        DEVICE(vkCreateRenderPass);
        VkAttachmentDescription attachment{}; attachment.format=sf.format; attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE; attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED; attachment.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{}; sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS; sub.colorAttachmentCount=1; sub.pColorAttachments=&ref;
        VkSubpassDependency dependency{}; dependency.srcSubpass=VK_SUBPASS_EXTERNAL; dependency.dstSubpass=0;
        dependency.srcStageMask=dependency.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO}; rp.attachmentCount=1; rp.pAttachments=&attachment; rp.subpassCount=1; rp.pSubpasses=&sub;
        rp.dependencyCount=1; rp.pDependencies=&dependency; CHECK(vkCreateRenderPass(device,&rp,nullptr,&renderpass));
        DEVICE(vkCreateImageView); DEVICE(vkCreateFramebuffer);
        for(auto image:images) {
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; vi.image=image; vi.viewType=VK_IMAGE_VIEW_TYPE_2D; vi.format=sf.format;
            vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}; VkImageView view{}; CHECK(vkCreateImageView(device,&vi,nullptr,&view)); views.push_back(view);
            VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO}; fb.renderPass=renderpass; fb.attachmentCount=1; fb.pAttachments=&views.back(); fb.width=extent.width; fb.height=extent.height; fb.layers=1;
            VkFramebuffer frame{}; CHECK(vkCreateFramebuffer(device,&fb,nullptr,&frame)); frames.push_back(frame);
        }
        DEVICE(vkCreateShaderModule);
        vertex=shader("romfs:/triangle.vert.spv",vkCreateShaderModule); fragment=shader("romfs:/triangle.frag.spv",vkCreateShaderModule);
        DEVICE(vkCreatePipelineLayout); VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO}; CHECK(vkCreatePipelineLayout(device,&pl,nullptr,&layout));
        VkPipelineShaderStageCreateInfo stages[2]{};
        for(auto& stage:stages) {stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stage.pName="main";}
        stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT; stages[0].module=vertex; stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module=fragment;
        VkPipelineVertexInputStateCreateInfo vin{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{0,0,float(extent.width),float(extent.height),0,1}; VkRect2D scissor{{0,0},extent};
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; vp.viewportCount=vp.scissorCount=1; vp.pViewports=&viewport; vp.pScissors=&scissor;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; raster.polygonMode=VK_POLYGON_MODE_FILL; raster.lineWidth=1;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend{}; blend.colorWriteMask=0xf;
        VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; bs.attachmentCount=1; bs.pAttachments=&blend;
        VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO}; gp.stageCount=2; gp.pStages=stages; gp.pVertexInputState=&vin; gp.pInputAssemblyState=&ia;
        gp.pViewportState=&vp; gp.pRasterizationState=&raster; gp.pMultisampleState=&ms; gp.pColorBlendState=&bs; gp.layout=layout; gp.renderPass=renderpass;
        DEVICE(vkCreateGraphicsPipelines); CHECK(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&gp,nullptr,&pipeline));
        DEVICE(vkCreateCommandPool); VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cp.queueFamilyIndex=family; cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        CHECK(vkCreateCommandPool(device,&cp,nullptr,&pool));
        DEVICE(vkAllocateCommandBuffers); VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ca.commandPool=pool; ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ca.commandBufferCount=1;
        VkCommandBuffer command{}; CHECK(vkAllocateCommandBuffers(device,&ca,&command));
        DEVICE(vkCreateSemaphore); VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        CHECK(vkCreateSemaphore(device,&sem,nullptr,&acquire)); CHECK(vkCreateSemaphore(device,&sem,nullptr,&rendered));
        DEVICE(vkCreateFence); VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; CHECK(vkCreateFence(device,&fi,nullptr,&fence));
        DEVICE(vkAcquireNextImageKHR); DEVICE(vkResetCommandBuffer); DEVICE(vkBeginCommandBuffer); DEVICE(vkEndCommandBuffer);
        DEVICE(vkCmdBeginRenderPass); DEVICE(vkCmdBindPipeline); DEVICE(vkCmdDraw); DEVICE(vkCmdEndRenderPass);
        DEVICE(vkQueueSubmit); DEVICE(vkQueuePresentKHR); DEVICE(vkWaitForFences); DEVICE(vkResetFences); DEVICE(vkQueueWaitIdle);
        unsigned presented=0;
        for(unsigned frame=0;frame<300 && appletMainLoop();++frame) {
            uint32_t index{}; CHECK(vkAcquireNextImageKHR(device,swapchain,UINT64_MAX,acquire,VK_NULL_HANDLE,&index));
            CHECK(vkResetCommandBuffer(command,0)); VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; CHECK(vkBeginCommandBuffer(command,&bi));
            VkClearValue clear{}; clear.color.float32[2]=.12f; clear.color.float32[3]=1;
            VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO}; begin.renderPass=renderpass; begin.framebuffer=frames.at(index); begin.renderArea=scissor; begin.clearValueCount=1; begin.pClearValues=&clear;
            vkCmdBeginRenderPass(command,&begin,VK_SUBPASS_CONTENTS_INLINE); vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline); vkCmdDraw(command,3,1,0,0); vkCmdEndRenderPass(command);
            CHECK(vkEndCommandBuffer(command));
            VkPipelineStageFlags wait_stage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquire;submit.pWaitDstStageMask=&wait_stage;
            submit.commandBufferCount=1;submit.pCommandBuffers=&command;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&rendered;
            CHECK(vkQueueSubmit(queue,1,&submit,fence));
            VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};present.waitSemaphoreCount=1;present.pWaitSemaphores=&rendered;present.swapchainCount=1;present.pSwapchains=&swapchain;present.pImageIndices=&index;
            CHECK(vkQueuePresentKHR(queue,&present)); CHECK(vkWaitForFences(device,1,&fence,VK_TRUE,UINT64_MAX));
            // Serial diagnostic: wait for present to consume its semaphore before reuse.
            CHECK(vkQueueWaitIdle(queue)); CHECK(vkResetFences(device,1,&fence)); ++presented;
        }
        if(!presented) throw std::runtime_error("No frame presented");
        std::fprintf(logfile,"VULKAN_PASS: mandatory features enabled; triangle submitted/presented %u frames. Human must confirm visible triangle.\n",presented);
    }
    VkShaderModule shader(const char* path, PFN_vkCreateShaderModule create) {
        FILE* file=std::fopen(path,"rb"); if(!file) throw std::runtime_error(path);
        std::fseek(file,0,SEEK_END); long size=std::ftell(file); std::rewind(file);
        if(size<=0 || size%4) {std::fclose(file);throw std::runtime_error("Invalid SPIR-V length");}
        std::vector<uint32_t> code(size/4); size_t read=std::fread(code.data(),1,size,file);std::fclose(file);
        if(read!=size_t(size) || code[0]!=0x07230203) throw std::runtime_error("Invalid SPIR-V");
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};ci.codeSize=size;ci.pCode=code.data();
        VkShaderModule module{};CHECK(create(device,&ci,nullptr,&module));return module;
    }
    static void report_format(VkPhysicalDevice physical, VkFormat format, const char* name,
        PFN_vkGetPhysicalDeviceFormatProperties get, PFN_vkGetPhysicalDeviceImageFormatProperties image) {
        VkFormatProperties p{};get(physical,format,&p);
        std::fprintf(logfile,"format %s linear=%x optimal=%x buffer=%x\n",name,p.linearTilingFeatures,p.optimalTilingFeatures,p.bufferFeatures);
        for (VkImageUsageFlags usage : {VkImageUsageFlags(VK_IMAGE_USAGE_SAMPLED_BIT),VkImageUsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),VkImageUsageFlags(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),VkImageUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT)}) {
            VkImageFormatProperties i{};VkResult rc=image(physical,format,VK_IMAGE_TYPE_2D,VK_IMAGE_TILING_OPTIMAL,usage,0,&i);
            std::fprintf(logfile,"  usage=%x rc=%d sampleCounts=%x maxExtent=%ux%ux%u\n",usage,rc,i.sampleCounts,i.maxExtent.width,i.maxExtent.height,i.maxExtent.depth);
        }
    }
};
int main() {
    // libnx's startup initializes applet/VI. A console framebuffer would compete
    // with the Vulkan swapchain for the default NWindow; write diagnostics to SD.
#ifdef SWITCH_COMBINED_PROBE
    logfile=std::fopen("sdmc:/switch/project8-feasibility-probe.log","w");
#else
    logfile=std::fopen("sdmc:/switch/project8-vulkan-probe.log","w");
#endif
    if(!logfile) return 1;
    std::fprintf(logfile, "probe revision: NXVK bring-up correction 2\n");
    const int env_result = setenv("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", 1);
    setenv("MESA_SHADER_CACHE_SHOW_STATS", "1", 1);
    const char* opt_in = std::getenv("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER");
    std::fprintf(logfile, "setenv result=%d NVK_I_WANT_A_BROKEN_VULKAN_DRIVER=%s\n",
                 env_result, opt_in ? opt_in : "<unset>");
    std::fprintf(logfile, "__nx_applet_type=%u Application=%u actual=%u __nx_heap_size=%zu (0=automatic)\n",
                 unsigned(__nx_applet_type), unsigned(AppletType_Application),
                 unsigned(appletGetAppletType()), __nx_heap_size);
    std::fprintf(logfile, "ICD vk_icdGetInstanceProcAddr=%p\n", reinterpret_cast<void*>(vk_icdGetInstanceProcAddr));
    for (InfoType type : {InfoType_TotalMemorySize, InfoType_UsedMemorySize}) {
        u64 value = 0; Result info_rc = svcGetInfo(&value, type, CUR_PROCESS_HANDLE, 0);
        std::fprintf(logfile, "GetInfo(%u): rc=0x%x bytes=%llu\n", unsigned(type), unsigned(info_rc),
                     static_cast<unsigned long long>(value));
    }
    std::fflush(logfile);
    Result rc=romfsInit(); std::fprintf(logfile,"romfsInit=0x%x appletType=%u\n",rc,unsigned(appletGetAppletType()));
    int status=1;
    if(R_SUCCEEDED(rc)) {
        try {
#ifdef SWITCH_COMBINED_PROBE
            if (!run_memory_probe(logfile)) throw std::runtime_error("Aliasing or cleanup failed; Vulkan skipped");
            std::fprintf(logfile, "Continuing to Vulkan independently of reprotection status.\n");
            std::fflush(logfile);
#endif
            if (env_result != 0 || !opt_in || std::strcmp(opt_in, "1"))
                throw std::runtime_error("NXVK driver opt-in could not be set");
            Probe probe;probe.run();status=0;
        }
        catch(const std::exception& e) {std::fprintf(logfile,"FAIL: %s\n",e.what());}
        romfsExit();
    }
    std::fclose(logfile);return status;
}
