#include<iostream>
#include<fstream>
#include<windows.h>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include<vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

using u16 = typename vk::CppType<vk::IndexType, vk::IndexType::eUint16>::Type;
using u32 = typename vk::CppType<vk::IndexType, vk::IndexType::eUint32>::Type;
namespace vk {
	Bool32 True = VK_TRUE;
	Bool32 False = VK_FALSE;
}

constexpr u32 WIDTH = 80;
constexpr u32 HEIGHT = 60;

#ifdef RELEASE
const std::vector<const char*> layers = {};
const std::vector<const char*> instance_extensions = {};
#else
const std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> instance_extensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
#endif

#ifndef RELEASE
VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
		VkDebugUtilsMessageTypeFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
		void* user_data) {
	std::cerr << "[validation]: " << callback_data->pMessage << std::endl;
	return vk::False;
}
#endif

int main() {
	try {
		vk::DynamicLoader dl;
		{
			auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
			VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
		}
		
		vk::Instance instance {};
		vk::DebugUtilsMessengerEXT debug_messenger {};
		u32 surface[WIDTH][HEIGHT] {};
		vk::PhysicalDevice physical_device {};
		vk::Device device {};
		vk::Queue queue {};
		vk::RenderPass render_pass {};
		vk::PipelineLayout pipeline_layout {};
		vk::Pipeline graphics_pipeline {};
		for (const auto layer_name : layers) {
			auto found_layer = false;
			for (const auto& l : vk::enumerateInstanceLayerProperties()) {
				if (std::string(layer_name) == l.layerName) {
					found_layer = true;
					break;
				}
			}
			if (!found_layer) throw std::runtime_error("layer requested, but not found!");
		}
		// Create Instance
		vk::ApplicationInfo app_info = {
			.pApplicationName = "Hello, Triangle!",
			.applicationVersion = VK_MAKE_VERSION(0, 1, 0),
			.pEngineName = "Null Engine",
			.engineVersion = VK_MAKE_VERSION(0, 0, 0),
			.apiVersion = VK_API_VERSION_1_3,
		};
		vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> instance_create_info {
			vk::InstanceCreateInfo {
				.pApplicationInfo = &app_info,
				.enabledLayerCount = static_cast<u32>(layers.size()),
				.ppEnabledLayerNames = layers.data(),
				.enabledExtensionCount = static_cast<u32>(instance_extensions.size()),
				.ppEnabledExtensionNames = instance_extensions.data(),
			},
			vk::DebugUtilsMessengerCreateInfoEXT {
				.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
					| vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
					| vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
				.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
					| vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
					| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
				.pfnUserCallback = debug_callback,
				.pUserData = nullptr,
			}
		};
		instance = vk::createInstance(instance_create_info.get<vk::InstanceCreateInfo>());
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
#ifndef RELEASE
		// Setup Debug Messenger
		debug_messenger = instance.createDebugUtilsMessengerEXT(instance_create_info.get<vk::DebugUtilsMessengerCreateInfoEXT>());
#endif
		// Pick Physical Device
		{
			auto physical_devices = instance.enumeratePhysicalDevices();
			if (physical_devices.empty())
				throw std::runtime_error("failed to find GPUs with vulkan support!");
			for (const auto& pd : physical_devices) {
				auto found_queue = false;
				auto queue_families = pd.getQueueFamilyProperties();
				for (const auto& qf : queue_families)
					if (qf.queueFlags & vk::QueueFlagBits::eGraphics) {
						found_queue = true;
						break;
					}
				if (found_queue) {
					physical_device = pd;
					break;
				}
			}
			if (!bool(physical_device))
				throw std::runtime_error("failed to find a suitable GPU!");
		}
		// Create Logical Device
		{
			u32 queue_family;
			auto queue_families = physical_device.getQueueFamilyProperties();
			u32 i = 0;
			for (const auto& qf : queue_families) {
				if (qf.queueFlags & vk::QueueFlagBits::eGraphics) {
					queue_family = i;
					break;
				}
				++i;
			}
			float queue_priority = 1;
			vk::DeviceQueueCreateInfo queue_create_info = {
				.queueFamilyIndex = queue_family,
				.queueCount = 1,
				.pQueuePriorities = &queue_priority,
			};
			vk::PhysicalDeviceFeatures device_features = {};
			device = physical_device.createDevice(vk::DeviceCreateInfo {
				.queueCreateInfoCount = 1,
				.pQueueCreateInfos = &queue_create_info,
				.enabledLayerCount = static_cast<u32>(layers.size()),
				.ppEnabledLayerNames = layers.data(),
				.enabledExtensionCount = 0,
				.ppEnabledExtensionNames = nullptr,
				.pEnabledFeatures = &device_features,
			});
			queue = device.getQueue(queue_family, 0);
		}
		// Create Render Pass
		{
			vk::AttachmentDescription color_attachment {
				.format = vk::Format::eR8G8B8A8Srgb,
				.samples = vk::SampleCountFlagBits::e1,
				.loadOp = vk::AttachmentLoadOp::eClear,
				.storeOp = vk::AttachmentStoreOp::eStore,
				.stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
				.stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
				.initialLayout = vk::ImageLayout::eUndefined,
				.finalLayout = vk::ImageLayout::eTransferDstOptimal,
			};
			vk::AttachmentReference color_attachment_ref {
				.attachment = 0,
				.layout = vk::ImageLayout::eColorAttachmentOptimal,
			};
			vk::SubpassDescription subpass {
				.pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
				.colorAttachmentCount = 1,
				.pColorAttachments = &color_attachment_ref,
			};
			render_pass = device.createRenderPass(vk::RenderPassCreateInfo {
				.attachmentCount = 1,
				.pAttachments = &color_attachment,
				.subpassCount = 1,
				.pSubpasses = &subpass,
			});
		}
		// Create Graphics Pipeline
		{
			std::vector<char> vert_shader;
			std::vector<char> frag_shader;
			{
				std::ifstream file("shaders/vert.spv", std::ios::ate | std::ios::binary);
				if (!file.is_open())
					throw std::runtime_error("failed to open file!");
				auto file_size = (size_t) file.tellg();
				vert_shader.reserve(file_size);
				file.seekg(0);
				file.read(vert_shader.data(), file_size);
				file.close();
			}
			{
				std::ifstream file("shaders/frag.spv", std::ios::ate | std::ios::binary);
				if (!file.is_open())
					throw std::runtime_error("failed to open file!");
				auto file_size = (size_t) file.tellg();
				frag_shader.reserve(file_size);
				file.seekg(0);
				file.read(frag_shader.data(), file_size);
				file.close();
			}
			vk::ShaderModule vert_module = device.createShaderModule(vk::ShaderModuleCreateInfo {
				.codeSize = vert_shader.size(),
				.pCode = reinterpret_cast<const u32*>(vert_shader.data()),
			});
			vk::ShaderModule frag_module = device.createShaderModule(vk::ShaderModuleCreateInfo {
				.codeSize = frag_shader.size(),
				.pCode = reinterpret_cast<const u32*>(frag_shader.data()),
			});
			vk::PipelineShaderStageCreateInfo shader_stages[] = {
				{
					.stage = vk::ShaderStageFlagBits::eVertex,
					.module = vert_module,
					.pName = "main",
				},			
				{
					.stage = vk::ShaderStageFlagBits::eFragment,
					.module = frag_module,
					.pName = "main",
				},
			};
			std::vector<vk::DynamicState> dynamic_states = {
				vk::DynamicState::eViewport,
				vk::DynamicState::eScissor,
			};
			vk::PipelineDynamicStateCreateInfo dynamic_state {
				.dynamicStateCount = static_cast<u32>(dynamic_states.size()),
				.pDynamicStates = dynamic_states.data(),
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info {
				.vertexBindingDescriptionCount = 0,
				.pVertexBindingDescriptions = nullptr,
				.vertexAttributeDescriptionCount = 0,
				.pVertexAttributeDescriptions = nullptr,
			};
			vk::PipelineInputAssemblyStateCreateInfo input_assembly {
				.topology = vk::PrimitiveTopology::eTriangleList,
				.primitiveRestartEnable = vk::False,
			};
			vk::Viewport viewport {
				.x = 0,
				.y = 0,
				.width = WIDTH,
				.height = HEIGHT,
				.minDepth = 0,
				.maxDepth = 1,
			};
			vk::Rect2D scissor {
				.offset = {0, 0},
				.extent = vk::Extent2D {
					.width = WIDTH,
					.height = HEIGHT,
				}
			};
			vk::PipelineViewportStateCreateInfo viewport_state {
				.viewportCount = 1,
				.pViewports = &viewport,
				.scissorCount = 1,
				.pScissors = &scissor,
			};
			vk::PipelineRasterizationStateCreateInfo rasterizer {
				.depthClampEnable = vk::False,
				.rasterizerDiscardEnable = vk::False,
				.polygonMode = vk::PolygonMode::eFill,
				.cullMode = vk::CullModeFlagBits::eBack,
				.frontFace = vk::FrontFace::eClockwise,
				.depthBiasEnable = vk::False,
				.depthBiasConstantFactor = 0,
				.depthBiasClamp = 0,
				.depthBiasSlopeFactor = 0,
				.lineWidth = 1,
			};
			vk::PipelineMultisampleStateCreateInfo multisampling {
				.rasterizationSamples = vk::SampleCountFlagBits::e1,
				.sampleShadingEnable = vk::False,
				.minSampleShading = 1,
				.pSampleMask = nullptr,
				.alphaToCoverageEnable = vk::False,
				.alphaToOneEnable = vk::False,
			};
			vk::PipelineColorBlendAttachmentState color_blend_attachment {
				.blendEnable = vk::True,
				.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
				.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
				.colorBlendOp = vk::BlendOp::eAdd,
				.srcAlphaBlendFactor = vk::BlendFactor::eOne,
				.dstAlphaBlendFactor = vk::BlendFactor::eZero,
				.alphaBlendOp = vk::BlendOp::eAdd,
				.colorWriteMask = vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA,
			};
			vk::PipelineColorBlendStateCreateInfo color_blending {
				.logicOpEnable = vk::False,
				.logicOp = vk::LogicOp::eCopy,
				.attachmentCount = 1,
				.pAttachments = &color_blend_attachment,
				.blendConstants = std::array {0.f, 0.f, 0.f, 0.f},
			};
			pipeline_layout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo {
				.setLayoutCount = 0,
				.pSetLayouts = nullptr,
				.pushConstantRangeCount = 0,
				.pPushConstantRanges = nullptr,
			});
			graphics_pipeline = device.createGraphicsPipeline(nullptr, vk::GraphicsPipelineCreateInfo {
				.stageCount = 2,
				.pStages = shader_stages,
				.pVertexInputState = &vertex_input_info,
				.pInputAssemblyState = &input_assembly,
				.pViewportState = &viewport_state,
				.pRasterizationState = &rasterizer,
				.pMultisampleState = &multisampling,
				.pDepthStencilState = nullptr,
				.pColorBlendState = &color_blending,
				.pDynamicState = &dynamic_state,
				.layout = pipeline_layout,
				.renderPass = render_pass,
				.subpass = 0,
				.basePipelineHandle = nullptr,
				.basePipelineIndex = -1,
			}).value;

			device.destroyShaderModule(vert_module);
			device.destroyShaderModule(frag_module);
		}

		auto run = true;
		while(run) {
			auto esc = GetAsyncKeyState(VK_ESCAPE);
			if ((1 << 15) & esc)
				run = false;
		};

		device.destroyPipeline(graphics_pipeline);
		device.destroyPipelineLayout(pipeline_layout);
		device.destroyRenderPass(render_pass);
		device.destroy();
#ifndef RELEASE
		instance.destroyDebugUtilsMessengerEXT(debug_messenger);
#endif
		instance.destroy();
		dl.~DynamicLoader();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
}
