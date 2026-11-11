#include "optix/optix_backend.h"

// optix_stubs.h pulls in <windows.h>, whose min/max macros would break the
// std::max calls inside optix_stack_size.h.
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cuda.h>
#include <optix.h>
#include <optix_function_table_definition.h>
#include <optix_stubs.h>
#include <optix_stack_size.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "optix/device_scene.h"
#include "optix/scene_upload.h"

#ifndef TDSG_OPTIX_PTX
#error "TDSG_OPTIX_PTX must point at the PTX file built from src/optix/shaders.cu"
#endif

namespace {

void checkOptix(OptixResult result, const char* what) {
    if (result != OPTIX_SUCCESS) {
        throw std::runtime_error(std::string(what) + " failed (OptixResult " +
                                 std::to_string(int(result)) + ")");
    }
}

void checkCuda(CUresult result, const char* what) {
    if (result != CUDA_SUCCESS) {
        throw std::runtime_error(std::string(what) + " failed (CUresult " +
                                 std::to_string(int(result)) + ")");
    }
}

std::string readText(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error(std::string("cannot open OptiX PTX: ") + path);
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

void contextLog(unsigned int level, const char* tag, const char* message, void*) {
    std::fprintf(stderr, "[OptiX][%u][%s] %s\n", level, tag, message ? message : "");
}

// Owns every OptiX/CUDA object of one render and releases them in reverse order.
struct Resources {
    CUdevice cuda_device = 0;
    CUcontext cuda_context = nullptr;
    OptixDeviceContext optix_context = nullptr;
    OptixModule module = nullptr;
    OptixProgramGroup groups[4] = {nullptr, nullptr, nullptr, nullptr};
    OptixPipeline pipeline = nullptr;
    std::vector<CUdeviceptr> allocations;

    Resources() = default;
    Resources(const Resources&) = delete;
    Resources& operator=(const Resources&) = delete;

    ~Resources() {
        for (CUdeviceptr pointer : allocations) {
            if (pointer) {
                cuMemFree(pointer);
            }
        }
        if (pipeline) {
            optixPipelineDestroy(pipeline);
        }
        for (OptixProgramGroup& group : groups) {
            if (group) {
                optixProgramGroupDestroy(group);
            }
        }
        if (module) {
            optixModuleDestroy(module);
        }
        if (optix_context) {
            optixDeviceContextDestroy(optix_context);
        }
        if (cuda_context) {
            cuDevicePrimaryCtxRelease(cuda_device);
        }
    }

    CUdeviceptr upload(size_t bytes, const void* data) {
        if (bytes == 0) {
            return 0;
        }
        CUdeviceptr pointer = 0;
        checkCuda(cuMemAlloc(&pointer, bytes), "cuMemAlloc");
        allocations.push_back(pointer);
        if (data) {
            checkCuda(cuMemcpyHtoD(pointer, data, bytes), "cuMemcpyHtoD");
        }
        return pointer;
    }
};

}  // namespace

bool OptixAvailable() {
    static const bool available = [] {
        try {
            checkCuda(cuInit(0), "cuInit");
            int count = 0;
            checkCuda(cuDeviceGetCount(&count), "cuDeviceGetCount");
            return count > 0;
        } catch (...) {
            return false;
        }
    }();
    return available;
}

bool RenderOptix(const Scene& scene, const Camera& camera, Film& film,
                 std::string_view integrator, const RenderSettings& settings,
                 int device, std::string& error) {
    try {
        int32_t integratorId = DEV_INTEGRATOR_PATH;
        if (integrator == "path") {
            integratorId = DEV_INTEGRATOR_PATH;
        } else if (integrator == "nee") {
            integratorId = DEV_INTEGRATOR_NEE;
        } else if (integrator == "mis") {
            integratorId = DEV_INTEGRATOR_MIS;
        } else if (integrator == "normal") {
            integratorId = DEV_INTEGRATOR_NORMAL;
        } else {
            error = "the optix backend does not implement integrator '" + std::string(integrator) +
                    "' (use path, nee, mis or normal)";
            return false;
        }

        const auto* pinhole = dynamic_cast<const PinholeCamera*>(&camera);
        if (!pinhole) {
            error = "the optix backend only supports the pinhole camera";
            return false;
        }
        if (settings.spp <= 0) {
            error = "spp must be positive";
            return false;
        }

        const UploadedScene uploaded = UploadScene(scene);

        Resources res;
        checkCuda(cuInit(0), "cuInit");
        CUdevice cuda_device;
        checkCuda(cuDeviceGet(&cuda_device, device), "cuDeviceGet");
        // The primary context keeps the code independent of the cuCtxCreate
        // signature, which changed across CUDA versions.
        checkCuda(cuDevicePrimaryCtxRetain(&res.cuda_context, cuda_device),
                  "cuDevicePrimaryCtxRetain");
        checkCuda(cuCtxSetCurrent(res.cuda_context), "cuCtxSetCurrent");
        res.cuda_device = cuda_device;

        char device_name[128] = {};
        checkCuda(cuDeviceGetName(device_name, sizeof(device_name), cuda_device), "cuDeviceGetName");
        std::printf("optix backend: %s, %u triangles, %u spheres, %u lights\n", device_name,
                    uploaded.triangleCount(), uploaded.sphereCount(),
                    unsigned(uploaded.lights.size()));

        checkOptix(optixInit(), "optixInit");
        OptixDeviceContextOptions context_options{};
        context_options.logCallbackFunction = &contextLog;
        context_options.logCallbackLevel = 2;
        checkOptix(optixDeviceContextCreate(res.cuda_context, &context_options, &res.optix_context),
                   "optixDeviceContextCreate");

        OptixModuleCompileOptions module_options{};
        module_options.optLevel = OPTIX_COMPILE_OPTIMIZATION_DEFAULT;
        module_options.debugLevel = OPTIX_COMPILE_DEBUG_LEVEL_NONE;

        OptixPipelineCompileOptions pipeline_options{};
        pipeline_options.usesMotionBlur = 0;
        pipeline_options.traversableGraphFlags =
            OPTIX_TRAVERSABLE_GRAPH_FLAG_ALLOW_SINGLE_LEVEL_INSTANCING;
        pipeline_options.numPayloadValues = 8;
        pipeline_options.numAttributeValues = 2;
        pipeline_options.exceptionFlags = OPTIX_EXCEPTION_FLAG_NONE;
        pipeline_options.pipelineLaunchParamsVariableName = "params";
        pipeline_options.pipelineLaunchParamsSizeInBytes = sizeof(DeviceLaunchParams);
        pipeline_options.usesPrimitiveTypeFlags =
            OPTIX_PRIMITIVE_TYPE_FLAGS_TRIANGLE | OPTIX_PRIMITIVE_TYPE_FLAGS_CUSTOM;

        const std::string ptx = readText(TDSG_OPTIX_PTX);
        char log[8192];
        size_t log_size = sizeof(log);
        checkOptix(optixModuleCreate(res.optix_context, &module_options, &pipeline_options,
                                     ptx.c_str(), ptx.size(), log, &log_size, &res.module),
                   "optixModuleCreate");

        OptixProgramGroupDesc group_descriptions[4]{};
        group_descriptions[0].kind = OPTIX_PROGRAM_GROUP_KIND_RAYGEN;
        group_descriptions[0].raygen.module = res.module;
        group_descriptions[0].raygen.entryFunctionName = "__raygen__render";

        group_descriptions[1].kind = OPTIX_PROGRAM_GROUP_KIND_MISS;
        group_descriptions[1].miss.module = res.module;
        group_descriptions[1].miss.entryFunctionName = "__miss__background";

        // Hit group 0: built-in triangles. Hit group 1: analytic spheres.
        group_descriptions[2].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        group_descriptions[2].hitgroup.moduleCH = res.module;
        group_descriptions[2].hitgroup.entryFunctionNameCH = "__closesthit__triangle";
        group_descriptions[2].hitgroup.moduleAH = res.module;
        group_descriptions[2].hitgroup.entryFunctionNameAH = "__anyhit__shadow";

        group_descriptions[3].kind = OPTIX_PROGRAM_GROUP_KIND_HITGROUP;
        group_descriptions[3].hitgroup.moduleIS = res.module;
        group_descriptions[3].hitgroup.entryFunctionNameIS = "__intersection__sphere";
        group_descriptions[3].hitgroup.moduleCH = res.module;
        group_descriptions[3].hitgroup.entryFunctionNameCH = "__closesthit__sphere";
        group_descriptions[3].hitgroup.moduleAH = res.module;
        group_descriptions[3].hitgroup.entryFunctionNameAH = "__anyhit__shadow";

        OptixProgramGroupOptions group_options{};
        log_size = sizeof(log);
        checkOptix(optixProgramGroupCreate(res.optix_context, group_descriptions, 4, &group_options,
                                           log, &log_size, res.groups),
                   "optixProgramGroupCreate");

        OptixPipelineLinkOptions link_options{};
        link_options.maxTraceDepth = 2;
        log_size = sizeof(log);
        checkOptix(optixPipelineCreate(res.optix_context, &pipeline_options, &link_options,
                                       res.groups, 4, log, &log_size, &res.pipeline),
                   "optixPipelineCreate");

        OptixStackSizes stack_sizes{};
        for (OptixProgramGroup group : res.groups) {
            checkOptix(optixProgramGroupGetStackSize(group, &stack_sizes, res.pipeline),
                       "optixProgramGroupGetStackSize");
        }
        unsigned int direct_from_traversal = 0, direct_from_state = 0, continuation = 0;
        checkOptix(optixUtilComputeStackSizes(&stack_sizes, link_options.maxTraceDepth, 0, 0,
                                              &direct_from_traversal, &direct_from_state,
                                              &continuation),
                   "optixUtilComputeStackSizes");
        // The helper only accounts for maxTraceDepth, not for the extra level the
        // IAS adds, and it reports zero for programs without callables. Keep a
        // floor so a hit program always has continuation stack available.
        const unsigned int continuation_stack = std::max(continuation * 2u, 2048u);
        checkOptix(optixPipelineSetStackSize(res.pipeline, direct_from_traversal,
                                             direct_from_state, continuation_stack, 2),
                   "optixPipelineSetStackSize");

        // Acceleration structures. OptiX requires every build input of one GAS
        // to have the same primitive type, so triangles and analytic spheres go
        // into separate GASes joined by a single-level IAS.
        const auto buildAccel = [&](const OptixBuildInput& input) {
            OptixAccelBuildOptions options{};
            options.buildFlags = OPTIX_BUILD_FLAG_PREFER_FAST_TRACE;
            options.operation = OPTIX_BUILD_OPERATION_BUILD;

            OptixAccelBufferSizes sizes{};
            checkOptix(optixAccelComputeMemoryUsage(res.optix_context, &options, &input, 1, &sizes),
                       "optixAccelComputeMemoryUsage");

            CUdeviceptr temp = res.upload(sizes.tempSizeInBytes, nullptr);
            CUdeviceptr output = res.upload(sizes.outputSizeInBytes, nullptr);
            OptixTraversableHandle accel = 0;
            checkOptix(optixAccelBuild(res.optix_context, 0, &options, &input, 1, temp,
                                       sizes.tempSizeInBytes, output, sizes.outputSizeInBytes,
                                       &accel, nullptr, 0),
                       "optixAccelBuild");
            return accel;
        };

        CUdeviceptr d_vertices = 0;
        CUdeviceptr d_aabbs = 0;
        uint32_t geometry_flags[2] = {OPTIX_GEOMETRY_FLAG_NONE, OPTIX_GEOMETRY_FLAG_NONE};
        OptixTraversableHandle triangle_accel = 0;
        OptixTraversableHandle sphere_accel = 0;

        if (uploaded.triangleCount() > 0) {
            d_vertices = res.upload(uploaded.triVertices.size() * sizeof(DevVec3),
                                    uploaded.triVertices.data());

            OptixBuildInput input{};
            input.type = OPTIX_BUILD_INPUT_TYPE_TRIANGLES;
            input.triangleArray.vertexBuffers = &d_vertices;
            input.triangleArray.numVertices = uint32_t(uploaded.triVertices.size());
            input.triangleArray.vertexFormat = OPTIX_VERTEX_FORMAT_FLOAT3;
            input.triangleArray.vertexStrideInBytes = sizeof(DevVec3);
            input.triangleArray.indexFormat = OPTIX_INDICES_FORMAT_NONE;
            input.triangleArray.indexBuffer = 0;
            input.triangleArray.numIndexTriplets = 0;
            input.triangleArray.indexStrideInBytes = 0;
            input.triangleArray.flags = &geometry_flags[0];
            input.triangleArray.numSbtRecords = 1;
            input.triangleArray.sbtIndexOffsetBuffer = 0;
            input.triangleArray.sbtIndexOffsetSizeInBytes = 0;
            input.triangleArray.sbtIndexOffsetStrideInBytes = 0;
            input.triangleArray.transformFormat = OPTIX_TRANSFORM_FORMAT_NONE;
            triangle_accel = buildAccel(input);
        }

        if (uploaded.sphereCount() > 0) {
            std::vector<OptixAabb> aabbs(uploaded.sphereCount());
            for (size_t i = 0; i < uploaded.sphereCount(); ++i) {
                const DeviceSphere& sphere = uploaded.spheres[i];
                aabbs[i].minX = sphere.center.x - sphere.radius;
                aabbs[i].minY = sphere.center.y - sphere.radius;
                aabbs[i].minZ = sphere.center.z - sphere.radius;
                aabbs[i].maxX = sphere.center.x + sphere.radius;
                aabbs[i].maxY = sphere.center.y + sphere.radius;
                aabbs[i].maxZ = sphere.center.z + sphere.radius;
            }
            d_aabbs = res.upload(aabbs.size() * sizeof(OptixAabb), aabbs.data());

            OptixBuildInput input{};
            input.type = OPTIX_BUILD_INPUT_TYPE_CUSTOM_PRIMITIVES;
            input.customPrimitiveArray.aabbBuffers = &d_aabbs;
            input.customPrimitiveArray.numPrimitives = uploaded.sphereCount();
            input.customPrimitiveArray.strideInBytes = sizeof(OptixAabb);
            input.customPrimitiveArray.flags = &geometry_flags[1];
            input.customPrimitiveArray.numSbtRecords = 1;
            input.customPrimitiveArray.sbtIndexOffsetBuffer = 0;
            input.customPrimitiveArray.sbtIndexOffsetSizeInBytes = 0;
            input.customPrimitiveArray.sbtIndexOffsetStrideInBytes = 0;
            sphere_accel = buildAccel(input);
        }

        if (triangle_accel == 0 && sphere_accel == 0) {
            error = "scene has no geometry";
            return false;
        }

        // Top level: instance 0 -> triangles (hit group 0), instance 1 -> spheres
        // (hit group 1). Both use an identity transform.
        std::vector<OptixInstance> instances;
        const auto addInstance = [&](OptixTraversableHandle accel, uint32_t instanceId,
                                     uint32_t sbtOffset) {
            OptixInstance instance{};
            const float identity[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
            std::memcpy(instance.transform, identity, sizeof(identity));
            instance.instanceId = instanceId;
            instance.sbtOffset = sbtOffset;
            instance.visibilityMask = 255;
            instance.flags = OPTIX_INSTANCE_FLAG_NONE;
            instance.traversableHandle = accel;
            instances.push_back(instance);
        };
        if (triangle_accel) {
            addInstance(triangle_accel, 0, 0);
        }
        if (sphere_accel) {
            addInstance(sphere_accel, 1, 1);
        }
        std::printf("optix: instances %u\n", unsigned(instances.size()));
        std::fflush(stdout);

        CUdeviceptr d_instances =
            res.upload(instances.size() * sizeof(OptixInstance), instances.data());
        OptixBuildInput instance_input{};
        instance_input.type = OPTIX_BUILD_INPUT_TYPE_INSTANCES;
        instance_input.instanceArray.instances = d_instances;
        instance_input.instanceArray.numInstances = uint32_t(instances.size());
        instance_input.instanceArray.instanceStride = sizeof(OptixInstance);
        OptixTraversableHandle handle = buildAccel(instance_input);
        checkCuda(cuCtxSynchronize(), "cuCtxSynchronize");

        // Shader binding table: 1 raygen, 1 miss, 2 hit groups.
        struct alignas(OPTIX_SBT_RECORD_ALIGNMENT) SbtRecord {
            char header[OPTIX_SBT_RECORD_HEADER_SIZE];
        };

        SbtRecord raygen_record{};
        checkOptix(optixSbtRecordPackHeader(res.groups[0], &raygen_record),
                   "optixSbtRecordPackHeader");
        SbtRecord miss_record{};
        checkOptix(optixSbtRecordPackHeader(res.groups[1], &miss_record), "optixSbtRecordPackHeader");
        SbtRecord hitgroup_records[2] = {};
        checkOptix(optixSbtRecordPackHeader(res.groups[2], &hitgroup_records[0]),
                   "optixSbtRecordPackHeader");
        checkOptix(optixSbtRecordPackHeader(res.groups[3], &hitgroup_records[1]),
                   "optixSbtRecordPackHeader");

        OptixShaderBindingTable sbt{};
        sbt.raygenRecord = res.upload(sizeof(raygen_record), &raygen_record);
        sbt.missRecordBase = res.upload(sizeof(miss_record), &miss_record);
        sbt.missRecordStrideInBytes = sizeof(SbtRecord);
        sbt.missRecordCount = 1;
        sbt.hitgroupRecordBase = res.upload(sizeof(hitgroup_records), hitgroup_records);
        sbt.hitgroupRecordStrideInBytes = sizeof(SbtRecord);
        sbt.hitgroupRecordCount = 2;

        // Launch parameters.
        const int width = camera.width();
        const int height = camera.height();
        const size_t pixel_count = size_t(width) * size_t(height);

        DeviceLaunchParams params{};
        params.scene.triangleCount = uploaded.triangleCount();
        params.scene.sphereCount = uploaded.sphereCount();
        params.scene.lightCount = uint32_t(uploaded.lights.size());
        params.scene.lightWeight = uploaded.lightWeight;

        if (!uploaded.triVertices.empty()) {
            params.scene.triVertices = reinterpret_cast<const DevVec3*>(res.upload(
                uploaded.triVertices.size() * sizeof(DevVec3), uploaded.triVertices.data()));
            params.scene.triangles = reinterpret_cast<const DeviceTriangleInfo*>(res.upload(
                uploaded.triangles.size() * sizeof(DeviceTriangleInfo), uploaded.triangles.data()));
        }
        if (!uploaded.spheres.empty()) {
            params.scene.spheres = reinterpret_cast<const DeviceSphere*>(res.upload(
                uploaded.spheres.size() * sizeof(DeviceSphere), uploaded.spheres.data()));
        }
        if (!uploaded.materials.empty()) {
            params.scene.materials = reinterpret_cast<const DeviceMaterial*>(res.upload(
                uploaded.materials.size() * sizeof(DeviceMaterial), uploaded.materials.data()));
        }
        if (!uploaded.lights.empty()) {
            params.scene.lights = reinterpret_cast<const DeviceLight*>(res.upload(
                uploaded.lights.size() * sizeof(DeviceLight), uploaded.lights.data()));
        }
        params.scene.background = reinterpret_cast<const DevColor*>(
            res.upload(sizeof(DevColor), &uploaded.background));
        if (!uploaded.cie.empty()) {
            params.scene.cie = reinterpret_cast<const float*>(
                res.upload(uploaded.cie.size() * sizeof(float), uploaded.cie.data()));
        }
        if (!uploaded.illuminant.empty()) {
            params.scene.illuminant = reinterpret_cast<const float*>(res.upload(
                uploaded.illuminant.size() * sizeof(float), uploaded.illuminant.data()));
        }

        const Vec3f camera_position = pinhole->getPosition();
        const Vec3f camera_forward = pinhole->getForward();
        const Vec3f camera_right = pinhole->getRight();
        const Vec3f camera_up = pinhole->getUp();
        params.camera.position = DevVec3{camera_position.x, camera_position.y, camera_position.z};
        params.camera.forward = DevVec3{camera_forward.x, camera_forward.y, camera_forward.z};
        params.camera.right = DevVec3{camera_right.x, camera_right.y, camera_right.z};
        params.camera.up = DevVec3{camera_up.x, camera_up.y, camera_up.z};
        params.camera.tanHalfFov = pinhole->getTanHalfFov();
        params.camera.width = width;
        params.camera.height = height;

        params.accum = reinterpret_cast<DevVec3*>(
            res.upload(pixel_count * sizeof(DevVec3), nullptr));
        params.width = uint32_t(width);
        params.height = uint32_t(height);
        params.spp = uint32_t(settings.spp);
        params.seed = settings.seed;
        params.maxDepth = settings.max_depth;
        params.integrator = integratorId;
        params.handle = uint64_t(handle);

        CUdeviceptr d_params = res.upload(sizeof(params), &params);
        checkOptix(optixLaunch(res.pipeline, 0, d_params, sizeof(params), &sbt, params.width,
                               params.height, 1),
                   "optixLaunch");
        checkCuda(cuCtxSynchronize(), "cuCtxSynchronize");

        std::vector<DevVec3> accum(pixel_count);
        checkCuda(cuMemcpyDtoH(accum.data(), reinterpret_cast<CUdeviceptr>(params.accum),
                               pixel_count * sizeof(DevVec3)),
                  "cuMemcpyDtoH");

        const float inverse_spp = 1.0f / float(settings.spp);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const DevVec3& value = accum[size_t(y) * size_t(width) + size_t(x)];
                film.addSamples(x, y,
                                SpectrumRGB{value.x * inverse_spp, value.y * inverse_spp,
                                            value.z * inverse_spp},
                                settings.spp);
            }
        }

        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
