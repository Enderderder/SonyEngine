/* Author: Morten S. Mikkelsen
 * Freely available for any type of use.
 */

#ifndef __WOOD_H__
#define __WOOD_H__

#include "../toolkit/shader_common/noise.hs"

void wood(out float3 diff_col,
		out float3 spec_col,
		in float3 shader_pos,
		float ringscale = 10,
		float3 lightwood = float3(0.3, 0.12, 0.03),
		float3 darkwood  = float3(0.05, 0.01, 0.005) )
{
	float3 PP = shader_pos;
	float y, z, r;
	
	PP += mynoise(PP);
	
	// compute radial distance r from PP to axis of tree
	y = PP.y;
	z = PP.z;
	r = sqrt (y*y + z*z);
	
	// map radial distance r nto ring position [0, 1]
	r *= ringscale;
	r += abs (mynoise(float3(r,0,0)));
	r -= floor (r);
	
	// use ring poisition r to select wood color
	r = smoothstep (0, 0.8, r) - smoothstep (0.83, 1.0, r);
	diff_col = lerp(lightwood, darkwood, r);
	spec_col = (0.3 * r + 0.7) * float3(1,1,1);
}



#endif