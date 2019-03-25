/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

uint encodeEndpointBc7(float4 endpoint)
{
    const uint4 quantized = (uint4)clamp(endpoint * 255.0 + 0.5, 0, 255.0);
    uint word = (quantized.r << 0) | (quantized.g << 8) | (quantized.b << 16) | (quantized.a << 24);
    const uint r_lsb = (word >>  0) & 1;
    const uint g_lsb = (word >>  8) & 1;
    const uint b_lsb = (word >> 16) & 1;
    const uint a_lsb = (word >> 24) & 1;
    if(r_lsb + g_lsb + b_lsb + a_lsb < 2)
        word &= 0xFEFEFEFE;
    else
        word |= 0x01010101;
    return word;
}

float4 decodeEndpointBc7(uint endpoint)
{
    uint4 quantized;
    quantized.r = (endpoint >>  0) & 255;
    quantized.g = (endpoint >>  8) & 255;
    quantized.b = (endpoint >> 16) & 255;
    quantized.a = (endpoint >> 24) & 255;
    return float4(quantized) / 255;
}

RW_Texture2D<uint4> g_compressedMip0 : register(u0); // MIP0 of the compressed source texture

thread_group_memory uint2 temp[64]; // to hold the indices of a microtile before writing to memory

