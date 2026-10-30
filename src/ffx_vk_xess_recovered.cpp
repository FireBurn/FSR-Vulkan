// SPDX-License-Identifier: MIT
#include "ffx_vk_xess_recovered.h"
#include "ffx_vk_xess_fixed_context.hpp"
#include "ffx_vk_xess_input_conversion.hpp"
#include <cstdio>
#include <memory>
#include <new>

struct FfxVkXeSSRecoveredContext {
    VkPhysicalDevice physicalDevice{};
    VkDevice device{};
    FfxVkXeSSReplayExtent extent{};
    std::unique_ptr<ffxvk::XeSSInputConversion> conversion;
    std::unique_ptr<ffxvk::XeSSFixedContext> implementation;
    std::vector<uint8_t> converted;
};

namespace {
void message(char* output,size_t capacity,const char* value) noexcept {
    if(output && capacity) std::snprintf(output,capacity,"%s",value);
}
void require(bool condition,const char* why) {
    if(!condition) throw std::invalid_argument(why);
}
void shaderValid(const FfxVkXeSSRecoveredBlob& blob) {
    require(blob.data && blob.sizeBytes>=20 && blob.sizeBytes<=4*1024*1024 && blob.sizeBytes%4==0,
        "XeSS shader blob must contain 20..4194304 bytes in whole words");
    uint32_t magic;std::memcpy(&magic,blob.data,sizeof(magic));
    require(magic==0x07230203u,"XeSS shader blob is not SPIR-V");
}
void bufferValid(const FfxVkXeSSRecoveredBlob& blob,size_t expected) {
    require(blob.data && blob.sizeBytes==expected,"XeSS model buffer size mismatch");
}
std::vector<uint32_t> shader(const FfxVkXeSSRecoveredBlob& blob) {
    std::vector<uint32_t> result(blob.sizeBytes/4);
    std::memcpy(result.data(),blob.data,blob.sizeBytes);return result;
}
std::vector<uint8_t> buffer(const FfxVkXeSSRecoveredBlob& blob) {
    std::vector<uint8_t> result(blob.sizeBytes);
    std::memcpy(result.data(),blob.data,blob.sizeBytes);return result;
}
uint32_t little32(const uint8_t* data) noexcept {
    return uint32_t(data[0])|(uint32_t(data[1])<<8)|(uint32_t(data[2])<<16)|(uint32_t(data[3])<<24);
}
uint64_t little64(const uint8_t* data) noexcept {return little32(data)|(uint64_t(little32(data+4))<<32);}
uint32_t checksum(const uint8_t* data,size_t bytes) noexcept {
    uint32_t crc=0xffffffffu;
    for(size_t i=0;i<bytes;++i) {
        crc^=data[i];
        for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1u)));
    }
    return ~crc;
}
ffxvk::XeSSExternalImage image(const FfxVkXeSSRecoveredImage& value) {
    return {value.image,value.view,value.extent,value.format};
}
template<class Function> VkResult boundary(char* diagnostic,size_t capacity,Function function) noexcept {
    message(diagnostic,capacity,"");
    try {return function();}
    catch(const ffxvk::XeSSVulkanError& error) {message(diagnostic,capacity,error.what());return error.result;}
    catch(const std::bad_alloc&) {message(diagnostic,capacity,"XeSS host allocation failed");return VK_ERROR_OUT_OF_HOST_MEMORY;}
    catch(const std::exception& error) {message(diagnostic,capacity,error.what());return VK_ERROR_INITIALIZATION_FAILED;}
    catch(...) {message(diagnostic,capacity,"Unknown XeSS failure");return VK_ERROR_INITIALIZATION_FAILED;}
}
}

extern "C" VkResult ffxVkXeSSRecoveredCreate(const FfxVkXeSSRecoveredCreateInfo* info,
    FfxVkXeSSRecoveredContext** outContext,char* diagnostic,size_t capacity) {
    if(outContext) *outContext=nullptr;
    return boundary(diagnostic,capacity,[&] {
        require(info && outContext,"XeSS creation requires info and an output context");
        require(info->structSize==sizeof(*info) && info->abiVersion==FFX_VK_XESS_RECOVERED_ABI_VERSION,
            "Unsupported XeSS create-info ABI");
        require(info->physicalDevice && info->device,"XeSS requires caller-owned Vulkan devices");
        require(ffxVkXeSSReplayExtentSupported(&info->extent),"Unverified XeSS extent");
        require(info->frameSlots>=1 && info->frameSlots<=8,"XeSS frame slots must be in [1,8]");
        require(info->model && info->model->structSize==sizeof(*info->model),"Unsupported XeSS model ABI");
        const auto& source=*info->model;
        shaderValid(source.prepareShader);shaderValid(source.resolveShader);
        constexpr size_t weights[]={2304,4608,18432,73728,73728,36864,18432,9216,4608,2304,2304,2304};
        constexpr size_t channels[]={16,32,64,128,64,64,32,32,16,16,16,16};
        for(unsigned i=0;i<12;++i) {
            const auto& conv=source.convolutions[i];shaderValid(conv.shader);
            bufferValid(conv.weights,weights[i]);bufferValid(conv.scales,channels[i]*4);bufferValid(conv.biases,channels[i]*4);
        }
        bufferValid(source.resolveWeights,512);bufferValid(source.resolveScales,256);bufferValid(source.resolveBiases,256);
        // Validate every span before allocating/copying payloads or GPU objects.
        ffxvk::XeSSFixedModel model;
        model.prepareShader=shader(source.prepareShader);model.resolveShader=shader(source.resolveShader);
        for(unsigned i=0;i<12;++i) {
            const auto& conv=source.convolutions[i];auto& target=model.convolutions[i];
            target.shader=shader(conv.shader);
            target.buffers={buffer(conv.weights),buffer(conv.scales),buffer(conv.biases)};
        }
        model.resolveBuffers={buffer(source.resolveWeights),buffer(source.resolveScales),buffer(source.resolveBiases)};
        auto result=std::make_unique<FfxVkXeSSRecoveredContext>();
        result->physicalDevice=info->physicalDevice;result->device=info->device;result->extent=info->extent;
        result->converted.resize(info->frameSlots,0);
        result->implementation=std::make_unique<ffxvk::XeSSFixedContext>(info->physicalDevice,info->device,model,info->extent,info->frameSlots);
        *outContext=result.release();return VK_SUCCESS;
    });
}
extern "C" void ffxVkXeSSRecoveredDestroy(FfxVkXeSSRecoveredContext* context) {delete context;}
extern "C" VkResult ffxVkXeSSRecoveredModelFromBlob(const void* blob,size_t sizeBytes,
    FfxVkXeSSRecoveredModel* outModel,char* diagnostic,size_t capacity) {
    if(outModel) *outModel={};
    return boundary(diagnostic,capacity,[&] {
        constexpr size_t count=53,headerBytes=16+count*24;
        require(blob && outModel && sizeBytes>=headerBytes && sizeBytes<=64*1024*1024,"Invalid XeSS container size");
        const auto* data=static_cast<const uint8_t*>(blob);
        require(std::memcmp(data,"XESSVK1\0",8)==0 && little32(data+8)==1 && little32(data+12)==count,
            "Unsupported XeSS container header");
        std::array<FfxVkXeSSRecoveredBlob,count> spans{};
        size_t end=headerBytes;
        for(size_t i=0;i<count;++i) {
            const auto* entry=data+16+i*24;
            const uint64_t offset=little64(entry),bytes=little64(entry+8);
            require(little32(entry+20)==0 && offset==((end+15)&~size_t(15)) &&
                offset<=sizeBytes && bytes>0 && bytes<=4*1024*1024 && bytes<=sizeBytes-offset,
                "Invalid XeSS container entry bounds");
            const auto* payload=data+static_cast<size_t>(offset);
            require(checksum(payload,static_cast<size_t>(bytes))==little32(entry+16),"XeSS container CRC mismatch");
            spans[i]={payload,static_cast<size_t>(bytes)};end=static_cast<size_t>(offset+bytes);
        }
        require(end==sizeBytes,"Unexpected trailing XeSS container data");
        FfxVkXeSSRecoveredModel model{};model.structSize=sizeof(model);
        model.prepareShader=spans[0];model.resolveShader=spans[1];
        for(unsigned i=0;i<12;++i) {
            auto& target=model.convolutions[i];const size_t index=2+i*4;
            target.shader=spans[index];target.weights=spans[index+1];target.scales=spans[index+2];target.biases=spans[index+3];
        }
        model.resolveWeights=spans[50];model.resolveScales=spans[51];model.resolveBiases=spans[52];
        *outModel=model;return VK_SUCCESS;
    });
}
extern "C" VkResult ffxVkXeSSRecoveredBind(FfxVkXeSSRecoveredContext* context,
    uint32_t frameSlot,const FfxVkXeSSRecoveredImages* images,char* diagnostic,size_t capacity) {
    return boundary(diagnostic,capacity,[&] {
        require(context && context->implementation && images,"XeSS binding requires a context and images");
        require(images->structSize==sizeof(*images),"Unsupported XeSS image ABI");
        context->implementation->bind({image(images->color),image(images->motion),image(images->depth),image(images->output)},frameSlot);
        context->converted[frameSlot]=0;
        return VK_SUCCESS;
    });
}
extern "C" VkResult ffxVkXeSSRecoveredBindConverted(FfxVkXeSSRecoveredContext* context,
    uint32_t frameSlot,const FfxVkXeSSRecoveredImages* images,const FfxVkXeSSRecoveredConversion* conversion,
    char* diagnostic,size_t capacity) {
    return boundary(diagnostic,capacity,[&] {
        require(context && context->implementation && images && conversion,"XeSS converted binding requires images and parameters");
        const uint32_t slots=context->implementation->frameSlotCount();
        ffxvk::XeSSInputConversion::validate(context->extent,slots,frameSlot,*images,*conversion);
        if(!context->conversion) context->conversion=std::make_unique<ffxvk::XeSSInputConversion>(
            context->physicalDevice,context->device,context->extent,slots);
        const auto canonical=context->conversion->bind(frameSlot,*images,*conversion);
        context->implementation->bind(canonical,frameSlot);context->converted[frameSlot]=1;
        return VK_SUCCESS;
    });
}
extern "C" VkResult ffxVkXeSSRecoveredRecord(FfxVkXeSSRecoveredContext* context,
    VkCommandBuffer commands,const FfxVkXeSSReplayFrame* frame,uint32_t frameSlot,char* diagnostic,size_t capacity) {
    return boundary(diagnostic,capacity,[&] {
        require(context && context->implementation && frame,"XeSS recording requires a context and frame");
        const auto valid=context->implementation->validateRecord(commands,*frame,frameSlot);
        if(valid!=VK_SUCCESS) {
            message(diagnostic,capacity,"XeSS record rejected command/slot, missing bindings/reset, or invalid frame parameters");
            return valid;
        }
        if(context->converted[frameSlot]) context->conversion->before(commands,frameSlot);
        const auto result=context->implementation->record(commands,*frame,frameSlot);
        if(result==VK_SUCCESS && context->converted[frameSlot]) context->conversion->after(commands,frameSlot);
        if(result!=VK_SUCCESS) message(diagnostic,capacity,"XeSS record rejected command/slot, missing bindings/reset, or invalid frame parameters");
        return result;
    });
}
extern "C" VkResult ffxVkXeSSRecoveredDiscardHistory(FfxVkXeSSRecoveredContext* context) {
    if(!context || !context->implementation) return VK_ERROR_INITIALIZATION_FAILED;
    context->implementation->discardHistory();
    if(context->conversion) context->conversion->discardHistory();
    return VK_SUCCESS;
}
extern "C" uint32_t ffxVkXeSSRecoveredFrameSlotCount(const FfxVkXeSSRecoveredContext* context) {
    return context && context->implementation?context->implementation->frameSlotCount():0;
}
extern "C" VkBool32 ffxVkXeSSRecoveredGetExtent(VkExtent2D output,FfxVkXeSSReplayExtent* extent) {
    return ffxVkXeSSReplayExtentForOutput(output.width,output.height,extent)?VK_TRUE:VK_FALSE;
}
