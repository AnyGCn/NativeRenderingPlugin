#pragma once

typedef enum RTReflectionKernelImageIndex
{
    OutImageIndex                   = 0,
    ThinGBufferPositionIndex        = 1,
    ThinGBufferDirectionIndex       = 2,
    IrradianceMapIndex              = 3
} RTReflectionKernelImageIndex;

typedef enum RTReflectionKernelBufferIndex
{
    SceneIndex                      = 0,
    AccelerationStructureIndex      = 1
} RTReflectionKernelBufferIndex;
