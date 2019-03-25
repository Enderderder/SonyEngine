/* Author: Morten S. Mikkelsen
 * Freely available for any type of use.
 */

#ifndef __WOOD2_H__
#define __WOOD2_H__

#include "../toolkit/shader_common/noise.hs"


void wood2(out float3 diff_col,
	out float3 spec_col,
	in float3 shader_pos,
	float ringscale = 15,
	float3 lightwood = float3(0.69, 0.44, 0.25),
	float3 darkwood  = float3(0.35, 0.22, 0.08),
	float grainy = 0.4 )
{
	float r, r2;
	float my_t;
	
	// Calculate in shader space
	float3 PP = shader_pos;
	
	my_t = PP.z / ringscale;
	float3 PQ = float3(PP.x*8, PP.y*8, PP.z);
	my_t += (mynoise(PQ) / 16);
	
	PQ = float3(PP.x, my_t, PP.y+12.93);
	r = ringscale * mynoise(PQ);
	r -= floor(r);
	r = 0.2 + 0.8 * smoothstep(0.2, 0.55, r) * (1 - smoothstep(0.75, 0.8, r));
	
	// extra line added for fine grain
	PQ = float3(PP.x*128+5, PP.z*8-3, PP.y*128+1);
	r2 = grainy * (1.3 - mynoise(PQ)) + (1-grainy);
	
	diff_col = lerp(lightwood, darkwood, r*r2*r2);
	spec_col = 1;
}



#endif