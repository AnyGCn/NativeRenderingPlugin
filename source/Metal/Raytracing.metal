#include "ShaderDefinition.h"

#include <metal_stdlib>
#include <simd/simd.h>

// Include the header that this Metal shader code shares with the Swift/C code that executes Metal API commands.
using namespace metal;
using raytracing::instance_acceleration_structure;

kernel void rtReflection(
             texture2d< float, access::write >      outImage                [[texture(OutImageIndex)]],
             texture2d< float >                     positions               [[texture(ThinGBufferPositionIndex)]],
             texture2d< float >                     directions              [[texture(ThinGBufferDirectionIndex)]],
             instance_acceleration_structure        accelerationStructure   [[buffer(AccelerationStructureIndex)]],
             uint2 tid [[thread_position_in_grid]])
{
    uint w = outImage.get_width();
    uint h = outImage.get_height();
    if ( tid.x < w&& tid.y < h )
    {
        float4 finalColor = float4( 0.0, 0.0, 0.0, 1.0 );
        if (is_null_instance_acceleration_structure(accelerationStructure))
        {
            finalColor = float4( 1.0, 0.0, 1.0, 1.0 );
        }
        else
        {
            raytracing::ray r;
            r.origin = positions.read(tid).xyz;
            r.direction = normalize(directions.read(tid).xyz);
            r.min_distance = 0.1;
            r.max_distance = FLT_MAX;

            raytracing::intersector<raytracing::instancing, raytracing::triangle_data> inter;
            inter.assume_geometry_type( raytracing::geometry_type::triangle );
            auto intersection = inter.intersect( r, accelerationStructure, 0xFF );
            if ( intersection.type == raytracing::intersection_type::triangle )
            {
                finalColor = float4( 1.0f, 1.0f, 1.0f, 1.0f );
            }
            else if ( intersection.type == raytracing::intersection_type::none )
            {
                finalColor = float4( 0.0f, 0.0f, 0.0f, 1.0f );
            }
        }
        outImage.write( finalColor, tid );
    }
}
