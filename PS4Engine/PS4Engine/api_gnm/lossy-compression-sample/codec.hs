/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

Texture2D<float4> g_compressedMip1 : register(t0); // MIP1 of the compressed source texture

struct MinMax
{
   float4 min;
   float4 max;
   float4 sum;
};

int2 adjust(int2 texelID, int2 delta)
{
    uint width, height;
    g_compressedMip1.GetDimensions(width, height);

    texelID = (texelID / 2 - 1) + delta;  
    texelID = max(0, texelID);
    texelID = min(int2(width-1, height-1), texelID);
    return texelID;
}

MinMax findMinMax(int2 texelID)
{
    MinMax minmax;
    minmax.sum = 0.0;
	minmax.min = 1.0;
	minmax.max = 0.0;
	for(int i = 0; i < 16; ++i)
	{
		const float4 texel = g_compressedMip1[adjust(texelID, int2(i & 3, i >> 2))];
		minmax.sum += texel;
		minmax.min = min(minmax.min, texel);
		minmax.max = max(minmax.max, texel);
	}
    return minmax;
}

struct Endpoints
{
   float4 a;
   float4 b;
};

Endpoints findEndpoints(MinMax minmax, int2 texelID)
{
	const float4 range = minmax.max - minmax.min;
	const float largest = max(max(range.x, range.y), max(range.z, range.w));
    Endpoints endpoints;
    if(largest < 1.0 / 255.0)
    {
        const float4 mean = minmax.sum / 16;
        endpoints.a = endpoints.b = mean;
    }
    else
    {
        int c;
	         if(largest == range.x)
	        c = 0;
        else if(largest == range.y)
		    c = 1;
	    else if(largest == range.z)
		    c = 2;
        else 
            c = 3;
        float4 sumXY = 0;

	    for(int i = 0; i < 16; ++i)
	    {
		    const float4 texel = g_compressedMip1[adjust(texelID, int2(i & 3, i >> 2))];
            sumXY += texel * texel[c];
        }
	    const float4 sumY = minmax.sum;	
	    const float4 sumX = sumY[c];
	    const float4 sumX2 = sumXY[c];
	    const float4 b = (sumXY * 16 - sumY * sumX) / (sumX2 * 16 - sumX * sumX);
	    const float4 a = (sumY - b * sumX) / 16;  
        endpoints.a = a + b * minmax.min[c];
        endpoints.b = a + b * minmax.max[c];
    }
    return endpoints;
}

