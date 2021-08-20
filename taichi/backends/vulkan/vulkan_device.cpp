#include "taichi/backends/vulkan/vulkan_api.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include <set>

#include "taichi/backends/vulkan/vulkan_common.h"
#include "taichi/backends/vulkan/loader.h"
#include "taichi/backends/vulkan/vulkan_device.h"
#include "taichi/common/logging.h"

#include "spirv_reflect.h"

namespace taichi {
namespace lang {
namespace vulkan {

const std::unordered_map<BufferFormat, VkFormat> buffer_format_ti_2_vk = {
    {BufferFormat::r8, VK_FORMAT_R8_UNORM},
    {BufferFormat::rg8, VK_FORMAT_R8G8_UNORM},
    {BufferFormat::rgba8, VK_FORMAT_R8G8B8A8_UNORM},
    {BufferFormat::r8u, VK_FORMAT_R8_UINT},
    {BufferFormat::rg8u, VK_FORMAT_R8G8_UINT},
    {BufferFormat::rgba8u, VK_FORMAT_R8G8B8A8_UINT},
    {BufferFormat::r8i, VK_FORMAT_R8_SINT},
    {BufferFormat::rg8i, VK_FORMAT_R8G8_SINT},
    {BufferFormat::rgba8i, VK_FORMAT_R8G8B8A8_SINT},
    {BufferFormat::r16, VK_FORMAT_R16_UNORM},
    {BufferFormat::rg16, VK_FORMAT_R16G16_UNORM},
    {BufferFormat::rgb16, VK_FORMAT_R16G16B16_UNORM},
    {BufferFormat::rgba16, VK_FORMAT_R16G16B16A16_UNORM},
    {BufferFormat::r16u, VK_FORMAT_R16_UNORM},
    {BufferFormat::rg16u, VK_FORMAT_R16G16_UNORM},
    {BufferFormat::rgb16u, VK_FORMAT_R16G16B16_UNORM},
    {BufferFormat::rgba16u, VK_FORMAT_R16G16B16A16_UNORM},
    {BufferFormat::r16i, VK_FORMAT_R16_SINT},
    {BufferFormat::rg16i, VK_FORMAT_R16G16_SINT},
    {BufferFormat::rgb16i, VK_FORMAT_R16G16B16_SINT},
    {BufferFormat::rgba16i, VK_FORMAT_R16G16B16A16_SINT},
    {BufferFormat::r16f, VK_FORMAT_R16_SFLOAT},
    {BufferFormat::rg16f, VK_FORMAT_R16G16_SFLOAT},
    {BufferFormat::rgb16f, VK_FORMAT_R16G16B16_SFLOAT},
    {BufferFormat::rgba16f, VK_FORMAT_R16G16B16A16_SFLOAT},
    {BufferFormat::r32u, VK_FORMAT_R32_UINT},
    {BufferFormat::rg32u, VK_FORMAT_R32G32_UINT},
    {BufferFormat::rgb32u, VK_FORMAT_R32G32B32_UINT},
    {BufferFormat::rgba32u, VK_FORMAT_R32G32B32A32_UINT},
    {BufferFormat::r32i, VK_FORMAT_R32_SINT},
    {BufferFormat::rg32i, VK_FORMAT_R32G32_SINT},
    {BufferFormat::rgb32i, VK_FORMAT_R32G32B32_SINT},
    {BufferFormat::rgba32i, VK_FORMAT_R32G32B32A32_SINT},
    {BufferFormat::r32f, VK_FORMAT_R32_SFLOAT},
    {BufferFormat::rg32f, VK_FORMAT_R32G32_SFLOAT},
    {BufferFormat::rgb32f, VK_FORMAT_R32G32B32_SFLOAT},
    {BufferFormat::rgba32f, VK_FORMAT_R32G32B32A32_SFLOAT},
    {BufferFormat::depth16, VK_FORMAT_D16_UNORM},
    {BufferFormat::depth24stencil8, VK_FORMAT_D24_UNORM_S8_UINT},
    {BufferFormat::depth32f, VK_FORMAT_D32_SFLOAT}};

VulkanPipeline::VulkanPipeline(const Params &params)
    : device_(params.device->vk_device()), name_(params.name) {
  create_descriptor_set_layout(params);
  create_shader_stages(params);
  create_pipeline_layout();
  create_compute_pipeline(params);

  for (VkShaderModule shader_module : shader_modules_) {
    vkDestroyShaderModule(device_, shader_module, kNoVkAllocCallbacks);
  }
}

VulkanPipeline::VulkanPipeline(
    const Params &params,
    const RasterParams &raster_params,
    const std::vector<VertexInputBinding> &vertex_inputs,
    const std::vector<VertexInputAttribute> &vertex_attrs)
    : device_(params.device->vk_device()), name_(params.name) {
  create_descriptor_set_layout(params);
  create_shader_stages(params);
  create_pipeline_layout();
  create_graphics_pipeline(raster_params, vertex_inputs, vertex_attrs);

  for (VkShaderModule shader_module : shader_modules_) {
    vkDestroyShaderModule(device_, shader_module, kNoVkAllocCallbacks);
  }
}

VulkanPipeline::~VulkanPipeline() {
  vkDestroyPipeline(device_, pipeline_, kNoVkAllocCallbacks);
  vkDestroyPipelineLayout(device_, pipeline_layout_, kNoVkAllocCallbacks);
}

VkShaderModule VulkanPipeline::create_shader_module(VkDevice device,
                                                    const SpirvCodeView &code) {
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = code.size;
  create_info.pCode = code.data;

  VkShaderModule shader_module;
  BAIL_ON_VK_BAD_RESULT(
      vkCreateShaderModule(device, &create_info, kNoVkAllocCallbacks,
                           &shader_module),
      "failed to create shader module");
  return shader_module;
}

VkPipeline VulkanPipeline::graphics_pipeline(
    const VulkanRenderPassDesc &renderpass_desc,
    VkRenderPass renderpass) {
  if (graphics_pipeline_.find(renderpass) != graphics_pipeline_.end()) {
    return graphics_pipeline_.at(renderpass);
  }

  std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(
      renderpass_desc.color_attachments.size());
  for (int i = 0; i < renderpass_desc.color_attachments.size(); i++) {
    blend_attachments[i].colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachments[i].blendEnable = VK_FALSE;
  }

  graphics_pipeline_template_->color_blending.attachmentCount =
      renderpass_desc.color_attachments.size();
  graphics_pipeline_template_->color_blending.pAttachments =
      blend_attachments.data();

  graphics_pipeline_template_->pipeline_info.renderPass = renderpass;

  VkPipeline pipeline;
  BAIL_ON_VK_BAD_RESULT(
      vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1,
                                &graphics_pipeline_template_->pipeline_info,
                                nullptr, &pipeline),
      "Graphics pipeline creation failed");

  graphics_pipeline_[renderpass] = pipeline;

  graphics_pipeline_template_->color_blending.attachmentCount = 0;
  graphics_pipeline_template_->color_blending.pAttachments = nullptr;
  graphics_pipeline_template_->pipeline_info.renderPass = VK_NULL_HANDLE;

  return pipeline;
}

void VulkanPipeline::create_descriptor_set_layout(const Params &params) {
  std::unordered_set<uint32_t> sets_used;

  for (auto &code_view : params.code) {
    SpvReflectShaderModule module;
    SpvReflectResult result =
        spvReflectCreateShaderModule(code_view.size, code_view.data, &module);
    TI_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    uint32_t set_count = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &set_count, nullptr);
    TI_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<SpvReflectDescriptorSet *> desc_sets(set_count);
    result = spvReflectEnumerateDescriptorSets(&module, &set_count,
                                               desc_sets.data());
    TI_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

    for (SpvReflectDescriptorSet *desc_set : desc_sets) {
      uint32_t set = desc_set->set;
      for (int i = 0; i < desc_set->binding_count; i++) {
        SpvReflectDescriptorBinding *desc_binding = desc_set->bindings[i];

        if (desc_binding->descriptor_type ==
            SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
          resource_binder_.rw_buffer(set, desc_binding->binding, kDeviceNullPtr,
                                     0);
        } else if (desc_binding->descriptor_type ==
                   SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
          resource_binder_.buffer(set, desc_binding->binding, kDeviceNullPtr,
                                  0);
        } else if (desc_binding->descriptor_type ==
                   SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
        }
      }
      sets_used.insert(set);
    }

    // Handle special vertex shaders stuff
    if (code_view.stage == VK_SHADER_STAGE_VERTEX_BIT) {
      uint32_t attrib_count;
      result =
          spvReflectEnumerateInputVariables(&module, &attrib_count, nullptr);
      TI_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);
      std::vector<SpvReflectInterfaceVariable *> attribs(attrib_count);
      result = spvReflectEnumerateOutputVariables(&module, &attrib_count,
                                                  attribs.data());
      TI_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS);

      for (SpvReflectInterfaceVariable *attrib : attribs) {
        uint32_t location = attrib->location;
        SpvReflectTypeDescription *type = attrib->type_description;
        TI_WARN("attrib {}:{}", location, type->type_name);
      }
    }
  }

  for (uint32_t set : sets_used) {
    VkDescriptorSetLayout layout =
        params.device->get_desc_set_layout(resource_binder_.get_set(set));

    set_layouts_.push_back(layout);
  }

  resource_binder_.lock_layout();
}

void VulkanPipeline::create_shader_stages(const Params &params) {
  for (auto &code_view : params.code) {
    VkPipelineShaderStageCreateInfo &shader_stage_info =
        shader_stages_.emplace_back();

    VkShaderModule shader_module = create_shader_module(device_, code_view);

    shader_stage_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_info.stage = code_view.stage;
    shader_stage_info.module = shader_module;
    shader_stage_info.pName = "main";

    shader_modules_.push_back(shader_module);
  }
}

void VulkanPipeline::create_pipeline_layout() {
  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = set_layouts_.size();
  pipeline_layout_info.pSetLayouts = set_layouts_.data();
  pipeline_layout_info.pushConstantRangeCount = 0;
  pipeline_layout_info.pPushConstantRanges = nullptr;
  BAIL_ON_VK_BAD_RESULT(
      vkCreatePipelineLayout(device_, &pipeline_layout_info,
                             kNoVkAllocCallbacks, &pipeline_layout_),
      "failed to create pipeline layout");
}

void VulkanPipeline::create_compute_pipeline(const Params &params) {
  VkComputePipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.stage = shader_stages_[0];
  pipeline_info.layout = pipeline_layout_;
  BAIL_ON_VK_BAD_RESULT(
      vkCreateComputePipelines(device_, /*pipelineCache=*/VK_NULL_HANDLE,
                               /*createInfoCount=*/1, &pipeline_info,
                               kNoVkAllocCallbacks, &pipeline_),
      "failed to create pipeline");
}

void VulkanPipeline::create_graphics_pipeline(
    const RasterParams &raster_params,
    const std::vector<VertexInputBinding> &vertex_inputs,
    const std::vector<VertexInputAttribute> &vertex_attrs) {
  this->graphics_pipeline_template_ =
      std::make_unique<GraphicsPipelineTemplate>();

  // Use dynamic viewport state
  VkPipelineViewportStateCreateInfo &viewport_state =
      graphics_pipeline_template_->viewport_state;
  viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 0;
  viewport_state.pViewports = nullptr;
  viewport_state.scissorCount = 0;
  viewport_state.pScissors = nullptr;

  for (const VertexInputBinding &binding : vertex_inputs) {
    VkVertexInputBindingDescription &desc =
        graphics_pipeline_template_->input_bindings.emplace_back();
    desc.binding = binding.binding;
    desc.stride = binding.stride;
    desc.inputRate = binding.instance ? VK_VERTEX_INPUT_RATE_INSTANCE
                                      : VK_VERTEX_INPUT_RATE_VERTEX;
  }

  for (const VertexInputAttribute &attr : vertex_attrs) {
    VkVertexInputAttributeDescription &desc =
        graphics_pipeline_template_->input_attrs.emplace_back();
    desc.binding = attr.binding;
    desc.location = attr.location;
    desc.format = buffer_format_ti_2_vk.at(attr.format);
    desc.offset = attr.offset;
  }

  VkPipelineVertexInputStateCreateInfo &vertex_input =
      graphics_pipeline_template_->input;
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.pNext = nullptr;
  vertex_input.flags = 0;
  vertex_input.vertexBindingDescriptionCount =
      graphics_pipeline_template_->input_bindings.size();
  vertex_input.pVertexBindingDescriptions =
      graphics_pipeline_template_->input_bindings.data();
  vertex_input.vertexAttributeDescriptionCount =
      graphics_pipeline_template_->input_attrs.size();
  vertex_input.pVertexAttributeDescriptions =
      graphics_pipeline_template_->input_attrs.data();

  VkPipelineInputAssemblyStateCreateInfo &input_assembly =
      graphics_pipeline_template_->input_assembly;
  input_assembly.topology = raster_params.prim_topology;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  VkPipelineRasterizationStateCreateInfo &rasterizer =
      graphics_pipeline_template_->rasterizer;
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = raster_params.raster_cull_mode;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  VkPipelineMultisampleStateCreateInfo &multisampling =
      graphics_pipeline_template_->multisampling;
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineDepthStencilStateCreateInfo &depth_stencil =
      graphics_pipeline_template_->depth_stencil;
  depth_stencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil.depthTestEnable = raster_params.depth_test;
  depth_stencil.depthWriteEnable = raster_params.depth_write;
  depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
  depth_stencil.depthBoundsTestEnable = VK_FALSE;
  depth_stencil.stencilTestEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo &color_blending =
      graphics_pipeline_template_->color_blending;
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.logicOp = VK_LOGIC_OP_COPY;
  color_blending.attachmentCount = 0;
  color_blending.pAttachments = nullptr;  // Filled in later
  color_blending.blendConstants[0] = 0.0f;
  color_blending.blendConstants[1] = 0.0f;
  color_blending.blendConstants[2] = 0.0f;
  color_blending.blendConstants[3] = 0.0f;

  VkPipelineDynamicStateCreateInfo &dynamic_state =
      graphics_pipeline_template_->dynamic_state;
  dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.pNext = NULL;
  dynamic_state.pDynamicStates =
      graphics_pipeline_template_->dynamic_state_enables.data();
  dynamic_state.dynamicStateCount =
      graphics_pipeline_template_->dynamic_state_enables.size();

  VkGraphicsPipelineCreateInfo &pipeline_info =
      graphics_pipeline_template_->pipeline_info;
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = shader_stages_.size();
  pipeline_info.pStages = shader_stages_.data();
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pDepthStencilState = &depth_stencil;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pipeline_layout_;
  pipeline_info.renderPass = VK_NULL_HANDLE;  // Filled in later
  pipeline_info.subpass = 0;
  pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
}

VulkanResourceBinder::VulkanResourceBinder(VkPipelineBindPoint bind_point)
    : bind_point_(bind_point) {
}

VulkanResourceBinder::~VulkanResourceBinder() {
}

#define CHECK_SET_BINDINGS                                          \
  bool set_not_found = (sets_.find(set) == sets_.end());            \
  if (set_not_found) {                                              \
    if (layout_locked_) {                                           \
      return;                                                       \
    } else {                                                        \
      sets_[set] = {};                                              \
    }                                                               \
  }                                                                 \
  auto &bindings = sets_.at(set).bindings;                          \
  if (layout_locked_ && bindings.find(binding) == bindings.end()) { \
    return;                                                         \
  }

void VulkanResourceBinder::rw_buffer(uint32_t set,
                                     uint32_t binding,
                                     DevicePtr ptr,
                                     size_t size) {
  CHECK_SET_BINDINGS;

  if (layout_locked_) {
    TI_ASSERT(bindings.at(binding).type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  }
  bindings[binding] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ptr, size};
}

void VulkanResourceBinder::rw_buffer(uint32_t set,
                                     uint32_t binding,
                                     DeviceAllocation alloc) {
  rw_buffer(set, binding, alloc.get_ptr(0), VK_WHOLE_SIZE);
}

void VulkanResourceBinder::buffer(uint32_t set,
                                  uint32_t binding,
                                  DevicePtr ptr,
                                  size_t size) {
  CHECK_SET_BINDINGS;

  if (layout_locked_) {
    TI_ASSERT(bindings.at(binding).type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  }
  bindings[binding] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ptr, size};
}

void VulkanResourceBinder::buffer(uint32_t set,
                                  uint32_t binding,
                                  DeviceAllocation alloc) {
  buffer(set, binding, alloc.get_ptr(0), VK_WHOLE_SIZE);
}

#undef CHECK_SET_BINDINGS

void VulkanResourceBinder::vertex_buffer(DevicePtr ptr, uint32_t binding) {
  TI_NOT_IMPLEMENTED
}

void VulkanResourceBinder::index_buffer(DevicePtr ptr, size_t index_width) {
  TI_NOT_IMPLEMENTED
}

void VulkanResourceBinder::framebuffer_color(DeviceAllocation image,
                                             uint32_t binding) {
  TI_NOT_IMPLEMENTED
}

void VulkanResourceBinder::framebuffer_depth_stencil(DeviceAllocation image) {
  TI_NOT_IMPLEMENTED
}

void VulkanResourceBinder::write_to_set(uint32_t index,
                                        VulkanDevice &device,
                                        VkDescriptorSet set) {
  std::vector<VkDescriptorBufferInfo> buffer_infos;
  std::vector<VkWriteDescriptorSet> desc_writes;

  for (auto &pair : sets_.at(index).bindings) {
    uint32_t binding = pair.first;

    if (pair.second.ptr != kDeviceNullPtr) {
      VkDescriptorBufferInfo &buffer_info = buffer_infos.emplace_back();
      buffer_info.buffer = device.get_vkbuffer(pair.second.ptr);
      buffer_info.offset = pair.second.ptr.offset;
      buffer_info.range = pair.second.size;

      VkWriteDescriptorSet &write = desc_writes.emplace_back();
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.pNext = nullptr;
      write.dstSet = set;
      write.dstBinding = binding;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = pair.second.type;
      write.pImageInfo = nullptr;
      write.pBufferInfo = nullptr;
      write.pTexelBufferView = nullptr;
    }
  }

  // Set these pointers later as std::vector resize can relocate the pointers
  int i = 0;
  for (auto &write : desc_writes) {
    write.pBufferInfo = &buffer_infos[i];
    i++;
  }

  vkUpdateDescriptorSets(device.vk_device(), desc_writes.size(),
                         desc_writes.data(), /*descriptorCopyCount=*/0,
                         /*pDescriptorCopies=*/nullptr);
}

void VulkanResourceBinder::lock_layout() {
  layout_locked_ = true;
}

VulkanCommandList::VulkanCommandList(VulkanDevice *ti_device,
                                     VkCommandBuffer buffer)
    : ti_device_(ti_device), device_(ti_device->vk_device()), buffer_(buffer) {
  VkCommandBufferBeginInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.pNext = nullptr;
  info.pInheritanceInfo = nullptr;
  info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  vkBeginCommandBuffer(buffer, &info);
}

VulkanCommandList::~VulkanCommandList() {
  for (auto pair : desc_sets_) {
    ti_device_->dealloc_desc_set(pair.first, pair.second);
  }
  ti_device_->dealloc_command_list(this);
}

void VulkanCommandList::bind_pipeline(Pipeline *p) {
  auto pipeline = static_cast<VulkanPipeline *>(p);

  if (pipeline->is_graphics()) {
    VkPipeline vk_pipeline = pipeline->graphics_pipeline(
        current_renderpass_desc_, current_renderpass_);
    vkCmdBindPipeline(buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, vk_pipeline);

    VkViewport viewport;
    viewport.width = viewport_width_;
    viewport.height = viewport_height_;
    viewport.x = 0;
    viewport.y = 0;
    viewport.minDepth = 0.0;
    viewport.maxDepth = 1.0;

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = {viewport_width_, viewport_height_};

    vkCmdSetViewport(buffer_, 0, 1, &viewport);
    vkCmdSetScissor(buffer_, 0, 1, &scissor);
    vkCmdSetLineWidth(buffer_, 1.0f);
  } else {
    vkCmdBindPipeline(buffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                      pipeline->pipeline());
  }

  current_pipeline_ = pipeline;
}

void VulkanCommandList::bind_resources(ResourceBinder *ti_binder) {
  VulkanResourceBinder *binder = static_cast<VulkanResourceBinder *>(ti_binder);

  for (auto &pair : binder->get_sets()) {
    VkDescriptorSetLayout layout = ti_device_->get_desc_set_layout(pair.second);
    VkDescriptorSet set = ti_device_->alloc_desc_set(layout);
    binder->write_to_set(pair.first, *ti_device_, set);
    vkCmdBindDescriptorSets(buffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                            current_pipeline_->pipeline_layout(),
                            /*firstSet=*/0,
                            /*descriptorSetCount=*/1, &set,
                            /*dynamicOffsetCount=*/0,
                            /*pDynamicOffsets=*/nullptr);
    desc_sets_.push_back(std::make_pair(layout, set));
  }

  if (current_pipeline_->is_graphics()) {
    auto [idx_ptr, type] = binder->get_index_buffer();
    if (idx_ptr.device) {
      vkCmdBindIndexBuffer(buffer_, ti_device_->get_vkbuffer(idx_ptr),
                           idx_ptr.offset, type);
    }

    for (auto [binding, ptr] : binder->get_vertex_buffers()) {
      VkBuffer buffer = ti_device_->get_vkbuffer(ptr);
      vkCmdBindVertexBuffers(buffer_, binding, 1, &buffer, &ptr.offset);
    }
  }
}

void VulkanCommandList::buffer_barrier(DevicePtr ptr, size_t size) {
  TI_ASSERT(ptr.device == ti_device_);

  VkBufferMemoryBarrier barrier;
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  barrier.pNext = nullptr;
  barrier.buffer = ti_device_->get_vkbuffer(ptr);
  barrier.offset = ptr.offset;
  barrier.size = size;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.srcAccessMask =
      (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
  barrier.dstAccessMask =
      (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

  vkCmdPipelineBarrier(
      buffer_,
      /*srcStageMask=*/
      VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      /*dstStageMask=*/VK_PIPELINE_STAGE_TRANSFER_BIT |
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      /*srcStageMask=*/0, /*memoryBarrierCount=*/0, nullptr,
      /*bufferMemoryBarrierCount=*/1,
      /*pBufferMemoryBarriers=*/&barrier,
      /*imageMemoryBarrierCount=*/0,
      /*pImageMemoryBarriers=*/nullptr);
}

void VulkanCommandList::buffer_barrier(DeviceAllocation alloc) {
  buffer_barrier(DevicePtr{alloc, 0}, VK_WHOLE_SIZE);
}

void VulkanCommandList::memory_barrier() {
  VkMemoryBarrier barrier;
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.pNext = nullptr;
  barrier.srcAccessMask =
      (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
  barrier.dstAccessMask =
      (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |
       VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

  vkCmdPipelineBarrier(
      buffer_,
      /*srcStageMask=*/
      VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      /*dstStageMask=*/VK_PIPELINE_STAGE_TRANSFER_BIT |
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      /*srcStageMask=*/0, /*memoryBarrierCount=*/1, &barrier,
      /*bufferMemoryBarrierCount=*/0,
      /*pBufferMemoryBarriers=*/nullptr,
      /*imageMemoryBarrierCount=*/0,
      /*pImageMemoryBarriers=*/nullptr);
}

void VulkanCommandList::buffer_copy(DevicePtr dst, DevicePtr src, size_t size) {
  VkBufferCopy copy_region{};
  copy_region.srcOffset = src.offset;
  copy_region.dstOffset = dst.offset;
  copy_region.size = size;
  vkCmdCopyBuffer(buffer_, ti_device_->get_vkbuffer(src),
                  ti_device_->get_vkbuffer(dst), /*regionCount=*/1,
                  &copy_region);
}

void VulkanCommandList::buffer_fill(DevicePtr ptr, size_t size, uint32_t data) {
  vkCmdFillBuffer(buffer_, ti_device_->get_vkbuffer(ptr), ptr.offset, size,
                  data);
}

void VulkanCommandList::dispatch(uint32_t x, uint32_t y, uint32_t z) {
  vkCmdDispatch(buffer_, x, y, z);
}

void VulkanCommandList::begin_renderpass(int x0,
                                         int y0,
                                         int x1,
                                         int y1,
                                         uint32_t num_color_attachments,
                                         DeviceAllocation *color_attachments,
                                         bool *color_clear,
                                         DeviceAllocation *depth_attachment,
                                         bool depth_clear) {
  VulkanRenderPassDesc &rp_desc = current_renderpass_desc_;
  current_renderpass_desc_.color_attachments.clear();
  rp_desc.clear_depth = depth_clear;

  bool has_depth = false;

  if (depth_attachment) {
    auto [image, view, format] = ti_device_->get_vk_image(*depth_attachment);
    rp_desc.depth_attachment = format;
    has_depth = true;
  } else {
    rp_desc.depth_attachment = VK_FORMAT_UNDEFINED;
  }

  std::vector<VkClearValue> clear_values(num_color_attachments +
                                         (has_depth ? 1 : 0));

  VulkanFramebufferDesc fb_desc;

  for (uint32_t i = 0; i < num_color_attachments; i++) {
    auto [image, view, format] =
        ti_device_->get_vk_image(color_attachments[i]);
    rp_desc.color_attachments.emplace_back(format, color_clear[i]);
    fb_desc.attachments.push_back(view);
    clear_values[i].color = VkClearColorValue{0.0, 0.0, 0.0, 0.0};
  }

  if (has_depth) {
    auto [image, view, format] = ti_device_->get_vk_image(*depth_attachment);
    clear_values[num_color_attachments].depthStencil =
        VkClearDepthStencilValue{1.0, 0};
    fb_desc.attachments.push_back(view);
  }

  current_renderpass_ = ti_device_->get_renderpass(rp_desc);

  fb_desc.width = x1 - x0;
  fb_desc.height = y1 - y0;
  fb_desc.renderpass = current_renderpass_;

  viewport_width_ = fb_desc.width;
  viewport_height_ = fb_desc.height;

  current_framebuffer_ = ti_device_->get_framebuffer(fb_desc);

  VkRect2D render_area;
  render_area.offset = {x0, y0};
  render_area.extent = {uint32_t(x1 - x0), uint32_t(y1 - y0)};

  VkRenderPassBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  begin_info.pNext = nullptr;
  begin_info.renderPass = current_renderpass_;
  begin_info.framebuffer = current_framebuffer_;
  begin_info.renderArea = render_area;
  begin_info.clearValueCount = clear_values.size();
  begin_info.pClearValues = clear_values.data();

  vkCmdBeginRenderPass(buffer_, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandList::end_renderpass() {
  vkCmdEndRenderPass(buffer_);

  current_renderpass_ = VK_NULL_HANDLE;
  current_framebuffer_ = VK_NULL_HANDLE;
}

void VulkanCommandList::draw(uint32_t num_verticies, uint32_t start_vertex) {
  vkCmdDraw(buffer_, num_verticies, /*instanceCount=*/1, start_vertex,
            /*firstInstance=*/0);
}

void VulkanCommandList::draw_indexed(uint32_t num_indicies,
                                     uint32_t start_vertex,
                                     uint32_t start_index) {
  vkCmdDrawIndexed(buffer_, num_indicies, /*instanceCount=*/1, start_index,
                   start_vertex,
                   /*firstInstance=*/0);
}

VkCommandBuffer VulkanCommandList::finalize() {
  if (!finalized_) {
    vkEndCommandBuffer(buffer_);
    finalized_ = true;
  }
  return buffer_;
}

void VulkanDevice::init_vulkan_structs(Params &params) {
  instance_ = params.instance;
  device_ = params.device;
  physical_device_ = params.physical_device;
  compute_queue_ = params.compute_queue;
  compute_pool_ = params.compute_pool;
  compute_queue_family_index_ = params.compute_queue_family_index;
  graphics_queue_ = params.graphics_queue;
  graphics_pool_ = params.graphics_pool;
  graphics_queue_family_index_ = params.graphics_queue_family_index;

  create_vma_allocator();

  VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
  BAIL_ON_VK_BAD_RESULT(vkCreateFence(device_, &fence_info, kNoVkAllocCallbacks,
                                      &cmd_sync_fence_),
                        "failed to create fence");
}

VulkanDevice::~VulkanDevice() {
  command_sync();

  TI_TRACE("Total #{} descriptor pools created", desc_set_pools_.size());

  size_t desc_count = 0;

  for (auto &pair : desc_set_pools_) {
    vkResetDescriptorPool(device_, pair.second.pool, 0);
    vkDestroyDescriptorPool(device_, pair.second.pool, kNoVkAllocCallbacks);
    desc_count += pair.second.free_sets.size();
  }

  TI_TRACE("Total #{} descriptors allocated", desc_count);

  for (auto &pair : desc_set_layouts_) {
    vkDestroyDescriptorSetLayout(device_, pair.second, kNoVkAllocCallbacks);
  }

  vmaDestroyAllocator(allocator_);
  vkDestroyFence(device_, cmd_sync_fence_, kNoVkAllocCallbacks);
}

std::unique_ptr<Pipeline> VulkanDevice::create_pipeline(PipelineSourceDesc &src,
                                                        std::string name) {
  TI_ASSERT(src.type == PipelineSourceType::spirv_binary &&
            src.stage == PipelineStageType::compute);

  SpirvCodeView code;
  code.data = (uint32_t *)src.data;
  code.size = src.size;
  code.stage = VK_SHADER_STAGE_COMPUTE_BIT;

  VulkanPipeline::Params params;
  params.code = {code};
  params.device = this;
  params.name = name;

  return std::make_unique<VulkanPipeline>(params);
}

DeviceAllocation VulkanDevice::allocate_memory(const AllocParams &params) {
  DeviceAllocation handle;

  handle.device = this;
  handle.alloc_id = (alloc_cnt_++);

  allocations_[handle.alloc_id] = {};
  AllocationInternal &alloc = allocations_[handle.alloc_id];

  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.pNext = nullptr;
  buffer_info.size = params.size;
  // FIXME: How to express this in a backend-neutral way?
  buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  VmaAllocationCreateInfo alloc_info{};

  if (params.host_read && params.host_write) {
    // This should be the unified memory on integrated GPUs
    alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    alloc_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
  } else if (params.host_read) {
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
  } else if (params.host_write) {
    alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
  } else {
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  }

  BAIL_ON_VK_BAD_RESULT(
      vmaCreateBuffer(allocator_, &buffer_info, &alloc_info, &alloc.buffer,
                      &alloc.allocation, &alloc.alloc_info),
      "Failed to allocate vk buffer");

  return handle;
}

void VulkanDevice::dealloc_memory(DeviceAllocation allocation) {
  AllocationInternal &alloc = allocations_[allocation.alloc_id];

  vmaDestroyBuffer(allocator_, alloc.buffer, alloc.allocation);

  allocations_.erase(allocation.alloc_id);
}

void *VulkanDevice::map_range(DevicePtr ptr, uint64_t size) {
  AllocationInternal &alloc_int = allocations_.at(ptr.alloc_id);

  TI_ASSERT_INFO(alloc_int.mapped == nullptr,
                 "Memory can not be mapped multiple times");

  vkMapMemory(device_, alloc_int.alloc_info.deviceMemory,
              alloc_int.alloc_info.offset + ptr.offset, size, 0,
              &alloc_int.mapped);

  return alloc_int.mapped;
}

void *VulkanDevice::map(DeviceAllocation alloc) {
  AllocationInternal &alloc_int = allocations_.at(alloc.alloc_id);

  TI_ASSERT_INFO(alloc_int.mapped == nullptr,
                 "Memory can not be mapped multiple times");

  vkMapMemory(device_, alloc_int.alloc_info.deviceMemory,
              alloc_int.alloc_info.offset, alloc_int.alloc_info.size, 0,
              &alloc_int.mapped);

  return alloc_int.mapped;
}

void VulkanDevice::unmap(DevicePtr ptr) {
  AllocationInternal &alloc_int = allocations_.at(ptr.alloc_id);

  TI_ASSERT_INFO(alloc_int.mapped, "Memory is not mapped");

  vkUnmapMemory(device_, alloc_int.alloc_info.deviceMemory);
  alloc_int.mapped = nullptr;
}

void VulkanDevice::unmap(DeviceAllocation alloc) {
  AllocationInternal &alloc_int = allocations_.at(alloc.alloc_id);

  TI_ASSERT_INFO(alloc_int.mapped, "Memory is not mapped");

  vkUnmapMemory(device_, alloc_int.alloc_info.deviceMemory);
  alloc_int.mapped = nullptr;
}

void VulkanDevice::memcpy_internal(DevicePtr dst,
                                   DevicePtr src,
                                   uint64_t size) {
}

std::unique_ptr<CommandList> VulkanDevice::new_command_list() {
  VkCommandBuffer buffer = VK_NULL_HANDLE;

  if (free_cmdbuffers_.size()) {
    buffer = free_cmdbuffers_.back();
    free_cmdbuffers_.pop_back();
  } else {
    // FIXME: Proper multi queue support, instead of defaulting to compute
    // command pool
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = compute_cmd_pool();
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    BAIL_ON_VK_BAD_RESULT(
        vkAllocateCommandBuffers(device_, &alloc_info, &buffer),
        "failed to allocate command buffer");
  }

  return std::make_unique<VulkanCommandList>(this, buffer);
}

void VulkanDevice::dealloc_command_list(CommandList *cmdlist) {
  VkCommandBuffer buffer =
      static_cast<VulkanCommandList *>(cmdlist)->finalize();
  if (in_flight_cmdlists_.find(buffer) == in_flight_cmdlists_.end()) {
    // Not in flight
    free_cmdbuffers_.push_back(buffer);
  } else {
    // In flight
    dealloc_cmdlists_.push_back(buffer);
  }
}

void VulkanDevice::submit(CommandList *cmdlist) {
  VkCommandBuffer buffer =
      static_cast<VulkanCommandList *>(cmdlist)->finalize();

  /*
  if (in_flight_cmdlists_.find(buffer) != in_flight_cmdlists_.end()) {
    TI_ERROR("Can not submit command list that is still in-flight");
    return;
  }
  */

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &buffer;

  // FIXME: Reuse fences as well?
  VkFence fence;
  VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, 0};
  BAIL_ON_VK_BAD_RESULT(
      vkCreateFence(device_, &fence_info, kNoVkAllocCallbacks, &fence),
      "failed to create fence");

  in_flight_cmdlists_.insert({buffer, fence});

  BAIL_ON_VK_BAD_RESULT(
      vkQueueSubmit(compute_queue(), /*submitCount=*/1, &submit_info,
                    /*fence=*/fence),
      "failed to submit command buffer");
}

void VulkanDevice::submit_synced(CommandList *cmdlist) {
  VkCommandBuffer buffer =
      static_cast<VulkanCommandList *>(cmdlist)->finalize();

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &buffer;

  BAIL_ON_VK_BAD_RESULT(
      vkQueueSubmit(compute_queue(), /*submitCount=*/1, &submit_info,
                    /*fence=*/cmd_sync_fence_),
      "failed to submit command buffer");

  // Timeout is in nanoseconds, 60s = 60,000ms = 60,000,000ns
  vkWaitForFences(device_, 1, &cmd_sync_fence_, true, (60 * 1000 * 1000));
}

void VulkanDevice::command_sync() {
  if (!in_flight_cmdlists_.size()) {
    return;
  }

  std::vector<VkFence> fences;
  fences.reserve(in_flight_cmdlists_.size());

  for (auto &pair : in_flight_cmdlists_) {
    fences.push_back(pair.second);
  }

  vkWaitForFences(device_, fences.size(), fences.data(), true,
                  (60 * 1000 * 1000));

  for (auto &pair : in_flight_cmdlists_) {
    vkDestroyFence(device_, pair.second, kNoVkAllocCallbacks);
  }

  in_flight_cmdlists_.clear();
  in_flight_desc_sets_.clear();

  for (auto buf : dealloc_cmdlists_) {
    free_cmdbuffers_.push_back(buf);
  }

  for (auto &pair : dealloc_desc_sets_) {
    pair.first->free_sets.push_back(pair.second);
  }
}

std::unique_ptr<Pipeline> VulkanDevice::create_raster_pipeline(
    std::vector<PipelineSourceDesc> &src,
    std::vector<BufferFormat> &render_target_formats,
    std::vector<VertexInputBinding> &vertex_inputs,
    std::vector<VertexInputAttribute> &vertex_attrs,
    std::string name) {
  return nullptr;
}

std::unique_ptr<Surface> VulkanDevice::create_surface(uint32_t width,
                                                      uint32_t height) {
  return std::make_unique<VulkanSurface>(this);
}

std::tuple<VkDeviceMemory, size_t, size_t>
VulkanDevice::get_vkmemory_offset_size(const DeviceAllocation &alloc) const {
  const AllocationInternal &alloc_int = allocations_.at(alloc.alloc_id);

  return std::make_tuple(alloc_int.alloc_info.deviceMemory,
                         alloc_int.alloc_info.offset,
                         alloc_int.alloc_info.size);
}

VkBuffer VulkanDevice::get_vkbuffer(const DeviceAllocation &alloc) const {
  const AllocationInternal &alloc_int = allocations_.at(alloc.alloc_id);

  return alloc_int.buffer;
}

std::tuple<VkImage, VkImageView, VkFormat> VulkanDevice::get_vk_image(
    const DeviceAllocation &alloc) const {
  const ImageAllocInternal &alloc_int = image_allocations_.at(alloc.alloc_id);

  return std::make_tuple(alloc_int.image, alloc_int.view, alloc_int.format);
}

VkFramebuffer VulkanDevice::get_framebuffer(const VulkanFramebufferDesc &desc) {
  if (framebuffer_pools_.find(desc) != framebuffer_pools_.end()) {
    return framebuffer_pools_.at(desc);
  }

  VkFramebufferCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  create_info.pNext = nullptr;
  create_info.flags = 0;
  create_info.renderPass = desc.renderpass;
  create_info.attachmentCount = desc.attachments.size();
  create_info.pAttachments = desc.attachments.data();
  create_info.width = desc.width;
  create_info.height = desc.height;
  create_info.layers = 1;

  VkFramebuffer framebuffer_;
  vkCreateFramebuffer(device_, &create_info, kNoVkAllocCallbacks,
                      &framebuffer_);

  framebuffer_pools_.insert({desc, framebuffer_});

  return framebuffer_;
}

DeviceAllocation VulkanDevice::import_vk_image(VkImage image,
                                               VkImageView view,
                                               VkFormat format) {
  ImageAllocInternal alloc_int;
  alloc_int.external = true;
  alloc_int.image = image;
  alloc_int.view = view;
  alloc_int.format = format;

  DeviceAllocation alloc;
  alloc.device = this;
  alloc.alloc_id = alloc_cnt_++;

  image_allocations_[alloc.alloc_id] = alloc_int;

  return alloc;
}

VkImageView VulkanDevice::get_vk_imageview(
    const DeviceAllocation &alloc) const {
  // FIXME: impl this
  return VkImageView();
}

VkRenderPass VulkanDevice::get_renderpass(const VulkanRenderPassDesc &desc) {
  if (renderpass_pools_.find(desc) != renderpass_pools_.end()) {
    return renderpass_pools_.at(desc);
  }

  std::vector<VkAttachmentDescription> attachments;
  std::vector<VkAttachmentReference> color_attachments;

  VkAttachmentReference depth_attachment;

  uint32_t i = 0;
  for (auto [format, clear] : desc.color_attachments) {
    VkAttachmentDescription &description = attachments.emplace_back();
    description.flags = 0;
    description.format = format;
    description.samples = VK_SAMPLE_COUNT_1_BIT;
    description.loadOp =
        clear ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference &ref = color_attachments.emplace_back();
    ref.attachment = i;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  if (desc.depth_attachment != VK_FORMAT_UNDEFINED) {
    VkAttachmentDescription &description = attachments.emplace_back();
    description.flags = 0;
    description.format = desc.depth_attachment;
    description.samples = VK_SAMPLE_COUNT_1_BIT;
    description.loadOp = desc.clear_depth ? VK_ATTACHMENT_LOAD_OP_LOAD
                                          : VK_ATTACHMENT_LOAD_OP_CLEAR;
    description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

    depth_attachment.attachment = i;
    depth_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }

  VkSubpassDescription subpass{};
  subpass.flags = 0;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = nullptr;
  subpass.colorAttachmentCount = color_attachments.size();
  subpass.pColorAttachments = color_attachments.data();
  subpass.pResolveAttachments = 0;
  subpass.pDepthStencilAttachment = desc.depth_attachment == VK_FORMAT_UNDEFINED
                                        ? nullptr
                                        : &depth_attachment;
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = nullptr;

  VkRenderPassCreateInfo renderpass_info{};
  renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderpass_info.pNext = nullptr;
  renderpass_info.flags = 0;
  renderpass_info.attachmentCount = attachments.size();
  renderpass_info.pAttachments = attachments.data();
  renderpass_info.subpassCount = 1;
  renderpass_info.pSubpasses = nullptr;
  renderpass_info.dependencyCount = 0;
  renderpass_info.pDependencies = nullptr;

  VkRenderPass renderpass;
  vkCreateRenderPass(device_, &renderpass_info, kNoVkAllocCallbacks,
                     &renderpass);

  renderpass_pools_.insert({desc, renderpass});

  return renderpass;
}

VkDescriptorSetLayout VulkanDevice::get_desc_set_layout(
    VulkanResourceBinder::Set &set) {
  if (desc_set_layouts_.find(set) == desc_set_layouts_.end()) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (auto &pair : set.bindings) {
      bindings.push_back(VkDescriptorSetLayoutBinding{
          /*binding=*/pair.first, pair.second.type, /*descriptorCount=*/1,
          VK_SHADER_STAGE_ALL,
          /*pImmutableSamplers=*/nullptr});
    }

    VkDescriptorSetLayout layout;
    {
      VkDescriptorSetLayoutCreateInfo create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      create_info.pNext = nullptr;
      create_info.flags = 0;
      create_info.bindingCount = bindings.size();
      create_info.pBindings = bindings.data();

      BAIL_ON_VK_BAD_RESULT(
          vkCreateDescriptorSetLayout(device_, &create_info,
                                      kNoVkAllocCallbacks, &layout),
          "Create descriptor layout failed");
    }

    VkDescriptorPool pool;
    {
      const int num_desc_types = 2;

      VkDescriptorPoolSize pool_size[num_desc_types];
      pool_size[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      pool_size[0].descriptorCount = 1000;
      pool_size[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      pool_size[1].descriptorCount = 1000;

      VkDescriptorPoolCreateInfo create_info{};
      create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      create_info.pNext = nullptr;
      create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
      create_info.maxSets = 1000;
      create_info.poolSizeCount = num_desc_types;
      create_info.pPoolSizes = pool_size;

      BAIL_ON_VK_BAD_RESULT(vkCreateDescriptorPool(device_, &create_info,
                                                   kNoVkAllocCallbacks, &pool),
                            "Create descriptor pool failed");
    }

    desc_set_layouts_[set] = layout;
    desc_set_pools_[layout] = {pool, {}};

    TI_TRACE("New descriptor set layout {}", (void *)layout);

    return layout;
  } else {
    return desc_set_layouts_.at(set);
  }
}

VkDescriptorSet VulkanDevice::alloc_desc_set(VkDescriptorSetLayout layout) {
  // TODO: Currently we assume the calling code has called get_desc_set_layout
  // before allocating a desc set. Either we should guard against this or
  // maintain this assumption in other parts of the VulkanBackend
  DescPool &desc_pool = desc_set_pools_.at(layout);

  if (desc_pool.free_sets.size()) {
    VkDescriptorSet set = desc_pool.free_sets.back();
    desc_pool.free_sets.pop_back();
    return set;
  } else {
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.descriptorPool = desc_pool.pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout;

    VkDescriptorSet set;
    BAIL_ON_VK_BAD_RESULT(vkAllocateDescriptorSets(device_, &alloc_info, &set),
                          "Alloc descriptor set from pool failed");

    return set;
  }
}

void VulkanDevice::dealloc_desc_set(VkDescriptorSetLayout layout,
                                    VkDescriptorSet set) {
  DescPool *pool = &desc_set_pools_.at(layout);
  if (in_flight_desc_sets_.find(set) == in_flight_desc_sets_.end()) {
    // Not in-flight
    pool->free_sets.push_back(set);
  } else {
    // Still in-flight
    dealloc_desc_sets_.push_back(std::make_pair(pool, set));
  }
}

void VulkanDevice::create_vma_allocator() {
  VolkDeviceTable table;
  VmaVulkanFunctions vk_vma_functions;

  volkLoadDeviceTable(&table, device_);
  vk_vma_functions.vkGetPhysicalDeviceProperties =
      PFN_vkGetPhysicalDeviceProperties(vkGetInstanceProcAddr(
          volkGetLoadedInstance(), "vkGetPhysicalDeviceProperties"));
  vk_vma_functions.vkGetPhysicalDeviceMemoryProperties =
      PFN_vkGetPhysicalDeviceMemoryProperties(vkGetInstanceProcAddr(
          volkGetLoadedInstance(), "vkGetPhysicalDeviceMemoryProperties"));
  vk_vma_functions.vkAllocateMemory = table.vkAllocateMemory;
  vk_vma_functions.vkFreeMemory = table.vkFreeMemory;
  vk_vma_functions.vkMapMemory = table.vkMapMemory;
  vk_vma_functions.vkUnmapMemory = table.vkUnmapMemory;
  vk_vma_functions.vkFlushMappedMemoryRanges = table.vkFlushMappedMemoryRanges;
  vk_vma_functions.vkInvalidateMappedMemoryRanges =
      table.vkInvalidateMappedMemoryRanges;
  vk_vma_functions.vkBindBufferMemory = table.vkBindBufferMemory;
  vk_vma_functions.vkBindImageMemory = table.vkBindImageMemory;
  vk_vma_functions.vkGetBufferMemoryRequirements =
      table.vkGetBufferMemoryRequirements;
  vk_vma_functions.vkGetImageMemoryRequirements =
      table.vkGetImageMemoryRequirements;
  vk_vma_functions.vkCreateBuffer = table.vkCreateBuffer;
  vk_vma_functions.vkDestroyBuffer = table.vkDestroyBuffer;
  vk_vma_functions.vkCreateImage = table.vkCreateImage;
  vk_vma_functions.vkDestroyImage = table.vkDestroyImage;
  vk_vma_functions.vkCmdCopyBuffer = table.vkCmdCopyBuffer;
  vk_vma_functions.vkGetBufferMemoryRequirements2KHR =
      table.vkGetBufferMemoryRequirements2KHR;
  vk_vma_functions.vkGetImageMemoryRequirements2KHR =
      table.vkGetImageMemoryRequirements2KHR;
  vk_vma_functions.vkBindBufferMemory2KHR = table.vkBindBufferMemory2KHR;
  vk_vma_functions.vkBindImageMemory2KHR = table.vkBindImageMemory2KHR;
  vk_vma_functions.vkGetPhysicalDeviceMemoryProperties2KHR =
      PFN_vkGetPhysicalDeviceMemoryProperties2KHR(vkGetInstanceProcAddr(
          volkGetLoadedInstance(), "vkGetPhysicalDeviceMemoryProperties2KHR"));

  VmaAllocatorCreateInfo allocatorInfo = {};
  allocatorInfo.vulkanApiVersion =
      this->get_cap(DeviceCapability::vk_api_version);
  allocatorInfo.physicalDevice = physical_device_;
  allocatorInfo.device = device_;
  allocatorInfo.instance = instance_;
  allocatorInfo.pVulkanFunctions = &vk_vma_functions;

  vmaCreateAllocator(&allocatorInfo, &allocator_);
}

VulkanSurface::VulkanSurface(VulkanDevice *device) : device_(device) {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window_ = glfwCreateWindow(640, 480, "Taichi", NULL, NULL);
  VkResult err =
      glfwCreateWindowSurface(device->vk_instance(), window_, NULL, &surface_);
  if (err) {
    TI_ERROR("Failed to create window ({})", err);
    return;
  }

  auto choose_surface_format =
      [](const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        for (const auto &availableFormat : availableFormats) {
          if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
              availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
          }
        }
        return availableFormats[0];
      };

  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->vk_physical_device(),
                                            surface_, &capabilities);

  VkBool32 supported = false;
  vkGetPhysicalDeviceSurfaceSupportKHR(device->vk_physical_device(),
                                       device->graphics_queue_family_index(),
                                       surface_, &supported);

  if (!supported) {
    TI_ERROR("Selected queue does not support presenting", err);
    return;
  }

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device->vk_physical_device(), surface_,
                                       &formatCount, nullptr);
  std::vector<VkSurfaceFormatKHR> surface_formats(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device->vk_physical_device(), surface_,
                                       &formatCount, surface_formats.data());

  VkSurfaceFormatKHR surface_format = choose_surface_format(surface_formats);

  int width, height;
  glfwGetFramebufferSize(window_, &width, &height);

  VkExtent2D extent = {uint32_t(width), uint32_t(height)};

  VkSwapchainCreateInfoKHR createInfo;
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.pNext = nullptr;
  createInfo.flags = 0;
  createInfo.surface = surface_;
  createInfo.minImageCount = capabilities.minImageCount;
  createInfo.imageFormat = surface_format.format;
  createInfo.imageColorSpace = surface_format.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  createInfo.queueFamilyIndexCount = 0;
  createInfo.pQueueFamilyIndices = nullptr;
  createInfo.preTransform = capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = nullptr;

  if (vkCreateSwapchainKHR(device->vk_device(), &createInfo,
                           kNoVkAllocCallbacks, &swapchain_) != VK_SUCCESS) {
    TI_ERROR("Failed to create swapchain");
    return;
  }

  VkSemaphoreCreateInfo sema_create_info;
  sema_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  sema_create_info.pNext = nullptr;
  sema_create_info.flags = 0;
  vkCreateSemaphore(device->vk_device(), &sema_create_info, kNoVkAllocCallbacks,
                    &image_available_);

  uint32_t num_images;
  vkGetSwapchainImagesKHR(device->vk_device(), swapchain_, &num_images,
                          nullptr);
  std::vector<VkImage> swapchain_images(num_images);
  vkGetSwapchainImagesKHR(device->vk_device(), swapchain_, &num_images,
                          swapchain_images.data());

  for (VkImage img : swapchain_images) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.pNext = nullptr;
    view_info.image = img;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = surface_format.format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView view;
    BAIL_ON_VK_BAD_RESULT(
        vkCreateImageView(device_->vk_device(), &view_info, nullptr, &view),
        "Failed to create image view");

    swapchain_images_.push_back(
        device->import_vk_image(img, view, surface_format.format));
  }
}

VulkanSurface::~VulkanSurface() {
}

std::pair<uint32_t, uint32_t> VulkanSurface::get_size() {
  int width, height;
  glfwGetFramebufferSize(window_, &width, &height);
  return std::make_pair(width, height);
}

DeviceAllocation VulkanSurface::get_target_image() {
  vkAcquireNextImageKHR(device_->vk_device(), swapchain_, UINT64_MAX,
                        image_available_, VK_NULL_HANDLE, &image_index_);

  return swapchain_images_[image_index_];
}

void VulkanSurface::present_image() {
  // TODO: In the future tie the wait semaphores.
  // Currently we should just halt and wait on host before present
  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 0;
  presentInfo.pWaitSemaphores = nullptr;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain_;
  presentInfo.pImageIndices = &image_index_;
  presentInfo.pResults = nullptr;

  vkQueuePresentKHR(device_->graphics_queue(), &presentInfo);
}

}  // namespace vulkan
}  // namespace lang
}  // namespace taichi
