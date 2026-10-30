// SPDX-License-Identifier: MIT
// Experimental recovered-graph resource ownership on a borrowed Vulkan device.
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>

namespace ffxvk {
class XeSSVulkanError final : public std::runtime_error {
public:
    const VkResult result;
    explicit XeSSVulkanError(VkResult value)
        :std::runtime_error("XeSS Vulkan allocation failed: "+std::to_string(value)),result(value) {}
};
struct XeSSBuffer { VkBuffer handle{}; VkDeviceMemory memory{}; void* mapped{}; };
struct XeSSImage { VkImage handle{}; VkDeviceMemory memory{}; VkImageView view{}; };
struct XeSSStage {
    VkDescriptorSetLayout setLayout{};
    VkPipelineLayout layout{};
    VkPipeline pipeline{};
    VkDescriptorSet set{};
    VkShaderModule shader{};
    VkDescriptorPool pool{};
};

// Device and physicalDevice must describe the same logical device. The caller
// enables the recovered shaders' required features and retires all commands
// before clear/destruction. This class never creates/destroys a device, submits
// work, or waits on an engine queue. Returned handles remain owned here.
class XeSSResources final {
    VkDevice device_;
    VkPhysicalDeviceMemoryProperties memory_{};
    std::vector<XeSSBuffer> buffers_;
    std::vector<XeSSImage> images_;
    std::vector<XeSSStage> stages_;
    std::vector<VkSampler> samplers_;
    std::map<VkDescriptorSetLayout,std::vector<VkDescriptorPoolSize>> stagePoolSizes_;
    std::vector<VkDescriptorPool> bindingPools_;
    static void check(VkResult result) {
        if(result!=VK_SUCCESS) throw XeSSVulkanError(result);
    }
    uint32_t memoryType(uint32_t bits,VkMemoryPropertyFlags flags) const {
        for(uint32_t i=0;i<memory_.memoryTypeCount;++i)
            if((bits&(1u<<i)) && (memory_.memoryTypes[i].propertyFlags&flags)==flags) return i;
        throw std::runtime_error("XeSS required memory type unavailable");
    }
public:
    XeSSResources(VkPhysicalDevice physicalDevice,VkDevice device):device_(device) {
        if(!physicalDevice || !device) throw std::invalid_argument("XeSS requires a caller-owned device");
        vkGetPhysicalDeviceMemoryProperties(physicalDevice,&memory_);
    }
    XeSSResources(const XeSSResources&)=delete;
    XeSSResources& operator=(const XeSSResources&)=delete;
    ~XeSSResources() {clear();}
    void clear() noexcept {
        for(auto pool:bindingPools_) if(pool) vkDestroyDescriptorPool(device_,pool,nullptr);
        for(auto& s:stages_) {
            if(s.pipeline) vkDestroyPipeline(device_,s.pipeline,nullptr);
            if(s.shader) vkDestroyShaderModule(device_,s.shader,nullptr);
            if(s.pool) vkDestroyDescriptorPool(device_,s.pool,nullptr);
            if(s.layout) vkDestroyPipelineLayout(device_,s.layout,nullptr);
            if(s.setLayout) vkDestroyDescriptorSetLayout(device_,s.setLayout,nullptr);
        }
        for(auto sampler:samplers_) if(sampler) vkDestroySampler(device_,sampler,nullptr);
        for(auto& im:images_) {
            if(im.view) vkDestroyImageView(device_,im.view,nullptr);
            if(im.handle) vkDestroyImage(device_,im.handle,nullptr);
            if(im.memory) vkFreeMemory(device_,im.memory,nullptr);
        }
        for(auto& b:buffers_) {
            if(b.mapped) vkUnmapMemory(device_,b.memory);
            if(b.handle) vkDestroyBuffer(device_,b.handle,nullptr);
            if(b.memory) vkFreeMemory(device_,b.memory,nullptr);
        }
        bindingPools_.clear();stagePoolSizes_.clear();stages_.clear();samplers_.clear();images_.clear();buffers_.clear();
    }
    // Shader words remain caller-owned and can be released after this call.
    // Descriptor writes are performed by the graph builder, not this owner.
    XeSSStage createStage(const std::vector<uint32_t>& code,
        const std::vector<VkDescriptorSetLayoutBinding>& bindings,uint32_t pushBytes) {
        if(code.size()<5 || code[0]!=0x07230203u || bindings.empty() ||
            !pushBytes || pushBytes%4 || pushBytes>128)
            throw std::invalid_argument("Invalid XeSS stage description");
        std::map<VkDescriptorType,uint32_t> totals;
        std::map<uint32_t,bool> seen;
        for(const auto& binding:bindings) {
            if(!binding.descriptorCount || binding.stageFlags!=VK_SHADER_STAGE_COMPUTE_BIT ||
                binding.pImmutableSamplers || !seen.emplace(binding.binding,true).second ||
                binding.descriptorCount>UINT32_MAX-totals[binding.descriptorType])
                throw std::invalid_argument("Invalid XeSS descriptor binding");
            totals[binding.descriptorType]+=binding.descriptorCount;
        }
        std::vector<VkDescriptorPoolSize> sizes;
        for(auto total:totals) sizes.push_back({total.first,total.second});
        stages_.emplace_back();auto& s=stages_.back();
        VkDescriptorSetLayoutCreateInfo li{};li.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li.bindingCount=static_cast<uint32_t>(bindings.size());li.pBindings=bindings.data();
        check(vkCreateDescriptorSetLayout(device_,&li,nullptr,&s.setLayout));
        stagePoolSizes_[s.setLayout]=sizes;
        VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,pushBytes};
        VkPipelineLayoutCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pi.setLayoutCount=1;pi.pSetLayouts=&s.setLayout;pi.pushConstantRangeCount=1;pi.pPushConstantRanges=&push;
        check(vkCreatePipelineLayout(device_,&pi,nullptr,&s.layout));
        VkShaderModuleCreateInfo si{};si.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        si.codeSize=code.size()*sizeof(uint32_t);si.pCode=code.data();check(vkCreateShaderModule(device_,&si,nullptr,&s.shader));
        VkComputePipelineCreateInfo ci{};ci.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;ci.layout=s.layout;
        ci.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,VK_SHADER_STAGE_COMPUTE_BIT,s.shader,"main",nullptr};
        check(vkCreateComputePipelines(device_,VK_NULL_HANDLE,1,&ci,nullptr,&s.pipeline));
        VkDescriptorPoolCreateInfo dpi{};dpi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dpi.maxSets=1;dpi.poolSizeCount=static_cast<uint32_t>(sizes.size());dpi.pPoolSizes=sizes.data();
        check(vkCreateDescriptorPool(device_,&dpi,nullptr,&s.pool));
        VkDescriptorSetAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool=s.pool;ai.descriptorSetCount=1;ai.pSetLayouts=&s.setLayout;
        check(vkAllocateDescriptorSets(device_,&ai,&s.set));return s;
    }
    // Allocate independent mutable bindings sharing an owned stage's immutable
    // pipeline/layout. Only the new pool/set is added to ownership tracking.
    XeSSStage createBindings(const XeSSStage& source) {
        const auto sizes=stagePoolSizes_.find(source.setLayout);
        bool owned=false;
        for(const auto& stage:stages_)
            if(stage.setLayout==source.setLayout && stage.layout==source.layout &&
                stage.pipeline==source.pipeline && stage.pipeline) {owned=true;break;}
        if(!owned || sizes==stagePoolSizes_.end()) throw std::invalid_argument("XeSS stage is not owned here");
        bindingPools_.push_back(VK_NULL_HANDLE);
        VkDescriptorPoolCreateInfo pi{};pi.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.maxSets=1;pi.poolSizeCount=static_cast<uint32_t>(sizes->second.size());pi.pPoolSizes=sizes->second.data();
        check(vkCreateDescriptorPool(device_,&pi,nullptr,&bindingPools_.back()));
        XeSSStage result=source;result.pool=bindingPools_.back();result.set=VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo ai{};ai.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool=result.pool;ai.descriptorSetCount=1;ai.pSetLayouts=&result.setLayout;
        check(vkAllocateDescriptorSets(device_,&ai,&result.set));return result;
    }
    VkSampler createSampler(const VkSamplerCreateInfo& info) {
        samplers_.push_back(VK_NULL_HANDLE);
        check(vkCreateSampler(device_,&info,nullptr,&samplers_.back()));
        return samplers_.back();
    }
    XeSSBuffer createBuffer(VkDeviceSize size,VkBufferUsageFlags usage) {
        if(!size || !usage) throw std::invalid_argument("Invalid XeSS buffer description");
        // Register before Vulkan calls so partial initialization is reclaimed.
        buffers_.emplace_back();auto& b=buffers_.back();
        VkBufferCreateInfo info{};info.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size=size;info.usage=usage;check(vkCreateBuffer(device_,&info,nullptr,&b.handle));
        VkMemoryRequirements requirements;vkGetBufferMemoryRequirements(device_,b.handle,&requirements);
        VkMemoryAllocateFlagsInfo flags{};flags.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        flags.flags=(usage&VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)?VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT:0;
        VkMemoryAllocateInfo alloc{};alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;alloc.pNext=&flags;
        alloc.allocationSize=requirements.size;
        alloc.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkAllocateMemory(device_,&alloc,nullptr,&b.memory));
        check(vkBindBufferMemory(device_,b.handle,b.memory,0));check(vkMapMemory(device_,b.memory,0,size,0,&b.mapped));
        return b;
    }
    XeSSImage createImage(uint32_t width,uint32_t height,VkFormat format,bool arrayView=false,uint32_t layers=1) {
        if(!width || !height || !layers || format==VK_FORMAT_UNDEFINED || (!arrayView && layers!=1))
            throw std::invalid_argument("Invalid XeSS image description");
        images_.emplace_back();auto& im=images_.back();
        VkImageCreateInfo info{};info.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType=VK_IMAGE_TYPE_2D;info.format=format;info.extent={width,height,1};
        info.mipLevels=1;info.arrayLayers=layers;info.samples=VK_SAMPLE_COUNT_1_BIT;
        info.usage=VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        check(vkCreateImage(device_,&info,nullptr,&im.handle));
        VkMemoryRequirements requirements;vkGetImageMemoryRequirements(device_,im.handle,&requirements);
        VkMemoryAllocateInfo alloc{};alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize=requirements.size;alloc.memoryTypeIndex=memoryType(requirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(vkAllocateMemory(device_,&alloc,nullptr,&im.memory));check(vkBindImageMemory(device_,im.handle,im.memory,0));
        VkImageViewCreateInfo view{};view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image=im.handle;view.viewType=arrayView?VK_IMAGE_VIEW_TYPE_2D_ARRAY:VK_IMAGE_VIEW_TYPE_2D;view.format=format;
        view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,layers};check(vkCreateImageView(device_,&view,nullptr,&im.view));
        return im;
    }
};
} // namespace ffxvk
