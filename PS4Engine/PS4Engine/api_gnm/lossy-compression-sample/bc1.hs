/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

uint encodeEndpointBc1(float4 endpoint)
{
    const uint b = (uint)clamp(endpoint.b * 31 + 0.5, 0, 31);
    const uint g = (uint)clamp(endpoint.g * 63 + 0.5, 0, 63);
    const uint r = (uint)clamp(endpoint.r * 31 + 0.5, 0, 31);
    const uint word = (b << 0) | (g << 5) | (r << 11);
    return word;
}

float4 decodeEndpointBc1(uint endpoint)
{
    const uint b = (endpoint >>  0) & 31;
    const uint g = (endpoint >>  5) & 63;
    const uint r = (endpoint >> 11) & 31;
    float4 normalized;
    normalized.r = r / 31.0;
    normalized.g = g / 63.0;
    normalized.b = b / 31.0;
    normalized.a =      1.0;
    return normalized;
}

RW_Texture2D<uint2> g_compressedMip0 : register(u0); // MIP0 of the compressed source texture

thread_group_memory uint temp[64]; // to hold the indices of a microtile before writing to memory



