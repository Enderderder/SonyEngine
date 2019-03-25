/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/
DataBuffer<uint> g_console_cell : register(t0);
DataBuffer<uint> g_console_font : register(t1);

ConstantBuffer Colors : register(c0)
{
  uint4 g_color[16][4];
}

ConstantBuffer Parameters : register(c1)
{
  uint g_log2CharacterWidth;
  uint g_log2CharacterHeight;
  uint g_screenWidthInCharacters;
  uint g_dummy;
  int  g_horizontalPixelOffset;
  int  g_verticalPixelOffset;
  uint g_screenWidthInPixels;
  uint g_screenHeightInPixels;
  float4 g_cursorPosition;
  float4 g_cursorSize;
  uint4 g_cursorColor;
}

#define kLog2GlyphWidth 3
#define kLog2GlyphHeight 3
#define kGlyphWidth (1<<kLog2GlyphWidth)
#define kGlyphHeight (1<<kLog2GlyphHeight)
#define kBitsPerByte 8
#define kBytesPerWord 4

float4 uintToFloat4(uint u)
{
	float4 result;
	result.x = (u >> 24) & 0xff;
	result.y = (u >> 16) & 0xff;
	result.z = (u >>  8) & 0xff;
	result.w = (u >>  0) & 0xff;
	return result / 255.0;
}

float4 renderDebugText(uint2 position)
{
    float cursor0Distance = length((float2)position - g_cursorPosition.xy);
	if(clamp(cursor0Distance, g_cursorSize.x, g_cursorSize.y) == cursor0Distance)
	  return uintToFloat4(g_color[g_cursorColor.x][g_cursorColor.y>>2][g_cursorColor.y&3]);

	float cursor1Distance = length((float2)position - g_cursorPosition.zw);
	if(clamp(cursor1Distance, g_cursorSize.z, g_cursorSize.w) == cursor1Distance)
	  return uintToFloat4(g_color[g_cursorColor.z][g_cursorColor.w>>2][g_cursorColor.w&3]);

	int2 pos;
	pos.x = position.x - g_horizontalPixelOffset;
	pos.y = position.y - g_verticalPixelOffset;
	if(pos.x < 0 || pos.x >= g_screenWidthInPixels || pos.y < 0 || pos.y >= g_screenHeightInPixels)
		return float4(0, 0, 0, 0);

	const uint x = pos.x >> (g_log2CharacterWidth-kLog2GlyphWidth);
	const uint y = pos.y >> (g_log2CharacterHeight-kLog2GlyphHeight);

	const uint x_tile  = x / kGlyphWidth;
	const uint y_tile  = y / kGlyphHeight;

	const uint x_pixel = x % kGlyphWidth;
	const uint y_pixel = y % kGlyphHeight;

	const uint cell_index = (y_tile * g_screenWidthInCharacters) + x_tile;
	uint tile = g_console_cell[cell_index];	

	const uint glyph           = (tile>> 0) & 0x0000FFFF;
	const uint foregroundColor = (tile>>16) & 0x0000000F;
	const uint foregroundAlpha = (tile>>20) & 0x0000000F;
	const uint backgroundColor = (tile>>24) & 0x0000000F;
	const uint backgroundAlpha = (tile>>28) & 0x0000000F;

	if(glyph == 0)
		return float4(0, 0, 0, 0);

	const uint bitsPerGlyph = kGlyphWidth * kGlyphHeight;
	const uint bytesPerGlyph = bitsPerGlyph / kBitsPerByte;
	const uint wordsPerGlyph = bytesPerGlyph / kBytesPerWord;
	const uint font_index = glyph * wordsPerGlyph + (y_pixel / kBytesPerWord);
	uint bits = g_console_font[font_index];
	bits >>= (y_pixel % kBytesPerWord) * kGlyphWidth;
	bits >>= kGlyphWidth - 1 - x_pixel;

	if(bits & 1)
	  return uintToFloat4(g_color[foregroundAlpha][foregroundColor>>2][foregroundColor&3]);
	else
	  return uintToFloat4(g_color[backgroundAlpha][backgroundColor>>2][backgroundColor&3]);
}
