/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

uint encodeEndpointBc5(float2 endpoint)
{
    const uint r = (uint)clamp(endpoint.r * 255.0 + 0.5, 0, 255.0);
    const uint g = (uint)clamp(endpoint.g * 255.0 + 0.5, 0, 255.0);
    return (g << 8) | r;
}

float2 decodeEndpointBc5(uint endpoint)
{
    const uint r = (endpoint >> 0) & 255;
    const uint g = (endpoint >> 8) & 255;
    return float2(r, g) / 255.0;
}

RW_Texture2D<uint4> g_compressedMip0 : register(u0); // MIP0 of the compressed source texture

thread_group_memory uint temp[192]; // to hold the indices of a microtile before writing to memory

