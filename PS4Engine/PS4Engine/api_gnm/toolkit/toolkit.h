/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_TOOLKIT_H
#define _SCE_GNM_TOOLKIT_H

#include <gnm.h>
#include <gnmx.h>
#include "geommath/geommath.h"
#include "allocator.h"
#include "embedded_shader.h"
#include "dataformat_interpreter.h"

namespace sce
{
	namespace Gnm
	{
		/*
		   DCC Clear Values
		*/
		enum DccClearValue
		{
			kDccClearValueUncompressed = 0xFFFFFFFF,	///< The DCC blocks are marked uncompressed.
			kDccClearValueRgb0A0	   = 0x00000000,	///< The DCC blocks are cleared to: RGBA = {0.0, 0.0, 0.0, 0.0}
			kDccClearValueRgb0A1	   = 0x40404040,	///< The DCC blocks are cleared to: RGBA = {0.0, 0.0, 0.0, 1.0}
			kDccClearValueRgb1A0	   = 0x80808080,	///< The DCC blocks are cleared to: RGBA = {1.0, 1.0, 1.0, 0.0}
			kDccClearValueRgb1A1	   = 0xC0C0C0C0,	///< The DCC blocks are cleared to: RGBA = {1.0, 1.0, 1.0, 1.0}
		};
	}

	namespace Gnmx
	{
		namespace Toolkit
		{
			/** @brief Sets the viewport parameters associated with a viewport id. This implementation takes more familiar parameters,
					   but requires slightly more computation internally.
			   @param dcb The command buffer to which commands should be written.
			   @param viewportId           Id of the viewport (ranges from 0 through 15)
			   @param left                 Left hand edge of the viewport rectangle
			   @param top                  Upper hand edge of the viewport rectangle
			   @param right                Right hand edge of the viewport rectangle
			   @param bottom               Lower edge of the viewport rectangle
			   @param dmin                 Minimum Z Value from Viewport Transform.  Z values will be clamped by the DB to this value.
			   @param dmax                 Maximum Z Value from Viewport Transform.  Z values will be clamped by the DB to this value.
			   @par Definition Heading
			   Draw Command Buffer
			   */
			void setViewport(sce::Gnmx::GnmxDrawCommandBuffer *dcb, uint32_t viewportId, int32_t left, int32_t top, int32_t right, int32_t bottom, float dmin, float dmax);


			/** @brief Saves the first miplevel of a texture to a .tga file.
			 * @param texture The texture to save
			 * @param filename The path to the TGA file to save to
			 * @return If the save is successful, the return value will be 0. Otherwise, a non-zero error code will be returned.
			 */
			int32_t saveTextureToTga(const sce::Gnm::Texture* texture, const char* filename);

			/** @brief Saves the first miplevel of a texture to a .pfm file.
			 * @param texture The texture to save
			 * @param pixels CPU accessible pointer to the pixels of the texture you wish to save.
							 It's assumed that these pixels are 'untiled'
			 * @param fileName The path to the PFM file to save to
			 * @return If the save is successful, the return value will be 0. Otherwise, a non-zero error code will be returned.
			 */
			int32_t saveTextureToPfm(const sce::Gnm::Texture* texture, const uint8_t* pixels, const char *fileName);

			/** @brief Flushes a render target from the CB cache, regardless of which of eight possible render target indices it's assigned to.
			 *         this is required when we don't know which render target indices are assigned to a render target, and we need to
			 *         ensure that a subsequent compute job won't hit memory that's lingering in the CB cache.
			 * @param dcb The command buffer to which commands should be written.
			 * @param renderTarget The render target to synchronize
			 * @par Definition Heading
				Draw Command Buffer
			 */
			void synchronizeRenderTargetGraphicsToCompute(sce::Gnmx::GnmxDrawCommandBuffer *dcb, const sce::Gnm::RenderTarget* renderTarget);

			 /** @brief Flushes a depth render target's Z buffer data from the DB cache.
			 *         this is required when we need to ensure that a subsequent compute job won't hit memory that's lingering in the DB cache.
			 * @param dcb The command buffer to which commands should be written.
			 * @param depthTarget The depth render target to synchronize Z buffer
			 * @par Definition Heading
			 Draw Command Buffer
			 */
			void synchronizeDepthRenderTargetZGraphicsToCompute(sce::Gnmx::GnmxDrawCommandBuffer *dcb, const sce::Gnm::DepthRenderTarget* depthTarget);

			/** @brief Waits for pixel end-of-shader, and then flushes the L2$. This should be sufficient when one
			 *         wishes to synchronize between a graphics job that outputs data and a compute job that reads the
			 *         same data, since both jobs will run on the GPU and will hit L2$, though each CU's L1$ may have a
			 *         stale copy of data that the previous job wrote.
			 * @param dcb The command buffer to which commands should be written.
			 * @par Definition Heading
			 Draw Command Buffer
			 */
			void synchronizeGraphicsToCompute( sce::Gnmx::GnmxDrawCommandBuffer *dcb );

			/** @brief Waits for compute end-of-shader, and then flushes the L1$. This should be sufficient when one
			 *         wishes to synchronize between a compute job that outputs data and a graphics job that reads the
			 *         same data, since both jobs will run on the GPU and will hit L2$, though each CU's L1$ may have a
			 *         stale copy of data that the previous job wrote.
			 * @param dcb The command buffer to which commands should be written.
			 * @par Definition Heading
			 Draw Command Buffer
			 */
			void synchronizeComputeToGraphics( sce::Gnmx::GnmxDrawCommandBuffer *dcb );

			/** @brief Waits for compute end-of-shader, and then flushes the L1$. This should be sufficient when one
			 *         wishes to synchronize between a compute job that outputs data and a subsequent compute job that reads the
			 *         same data, since both jobs will run on the GPU and will hit L2$, though a given CU's L1$ may have a
			 *         stale copy of data that the previous job wrote.
			 * @param dcb The command buffer to which commands should be written.
			 * @par Definition Heading
			 Draw Command Buffer
			 */
			void synchronizeComputeToCompute( sce::Gnmx::GnmxDrawCommandBuffer *dcb );

			/** @brief Waits for compute end-of-shader, and then flushes the L1$. This should be sufficient when one
			 *         wishes to synchronize between a compute job that outputs data and a subsequent compute job that reads the
			 *         same data, since both jobs will run on the GPU and will hit L2$, though a given CU's L1$ may have a
			 *         stale copy of data that the previous job wrote.
			 * @param dcb The command buffer to which commands should be written.
			 * @par Definition Heading
			 Dispatch Command Buffer
			 */
			void synchronizeComputeToCompute( sce::Gnmx::GnmxDispatchCommandBuffer *dcb );

			/** @brief Initializes Toolkit with ONION and GARLIC memory allocators.
			 **
			 ** @param allocators a pointer to a Allocators object
			 **/
			void initializeWithAllocators(Gnmx::Toolkit::Allocators *allocators);

			/** @brief Allocate and map a memory block for the tessellation factor ring buffer */
			void* allocateTessellationFactorRingBuffer();

			/** @brief Deallocate the allocated memory block for the tessellation factor ring buffer */
			void deallocateTessellationFactorRingBuffer();

			namespace SurfaceUtil
			{
				/** @brief Uses CPU to fill a region of memory with a 32-bit value.
				 * @param destAddr The address to begin filling.
				 * @param dwords The number of dwords to fill.
				 * @param val The 32-bit value to set memory to.
				 */
				void fillDwordsWithCpu(uint32_t* destAddr, uint32_t dwords, uint32_t val);

				/** @brief Uses CP DMA to fill a region of memory with a 32-bit value.
				 * @param gfxc The graphics context to use.
				 * @param destAddr The address to begin filling.
				 * @param dwords The number of dwords to fill.
				 * @param val The 32-bit value to set memory to.
				 */
				void fillDwordsWithDma(GnmxGfxContext &gfxc, void *destAddr, uint32_t dwords, uint32_t val);

				/** @brief Uses a compute shader to clear memory to a repeating 32-bit pattern.
				 * @param gfxc The graphics context to use.
				 * @param destAddr The address to begin filling.
				 * @param dwords the number of dwords to fill.
				 * @param value the 32-bit value to set memory to.
				 */
				void fillDwordsWithCompute(GnmxGfxContext &gfxc, void *destAddr, uint32_t dwords, uint32_t value);

				/** @brief Uses a compute shader to clear memory, by writing a repeating bit pattern to it as an RW_DataBuffer<uint>.
				 * @param gfxc The graphics context to use
				 * @param destination The memory to clear
				 * @param destUints The number of uints in the destination buffer
				 * @param source Pointer to srcUints uint32_t values. Must be in GPU visible memory (not on stack.)
				 * @param srcUints The number of uints in the source buffer
				 */
				void clearMemoryToUints(GnmxGfxContext &gfxc, void *destination, uint32_t destUints, const uint32_t *source, uint32_t srcUints);

				/** @brief Uses a compute shader to clear a buffer to a constant value, by writing a repeating bit pattern to it as an RW_DataBuffer<uint>.
				 * @param gfxc The graphics context to use
				 * @param buffer The buffer to clear
				 * @param source Pointer to data to clear the buffer with
				 * @param sourceUints Number of uints in source data
				 */
				void clearBuffer(GnmxGfxContext &gfxc, const sce::Gnm::Buffer* buffer, uint32_t *source, uint32_t sourceUints);

				/** @brief Uses a compute shader to clear a buffer to a constant value.
				 * @param gfxc The graphics context to use
				 * @param buffer The buffer to clear
				 * @param vector The vector to clear the buffer to
				 */
				void clearBuffer(GnmxGfxContext &gfxc, const sce::Gnm::Buffer* buffer, const Vector4& vector);

				/** @brief Uses a compute shader to clear a texture to a solid color, by writing a repeating bit pattern to it as an RW_DataBuffer<uint>.
				 * @param gfxc The graphics context to use
				 * @param texture The texture to clear
				 * @param source Pointer to data to clear the texture with
				 * @param sourceUints Number of uints in source data
				 */
				void clearTexture(GnmxGfxContext &gfxc, const sce::Gnm::Texture* texture, uint32_t *source, uint32_t sourceUints);

				/** @brief Uses a compute shader to clear a texture to a solid color.
				 * @param gfxc The graphics context to use
				 * @param texture The texture to clear
				 * @param color The color to clear the texture to
				 */
				void clearTexture(GnmxGfxContext &gfxc, const sce::Gnm::Texture* texture, const Vector4& color);

				/** @brief Uses a compute shader to clear a render target to a solid color, by writing a repeating bit pattern to it as an RW_DataBuffer<uint>.
				 * @param gfxc The graphics context to use
				 * @param renderTarget The render target to clear
				 * @param source Pointer to data to clear the render target with
				 * @param sourceUints Number of uints in source data
				 */
				void clearRenderTarget(GnmxGfxContext &gfxc, const sce::Gnm::RenderTarget* renderTarget, uint32_t *source, uint32_t sourceUints);

				/** @brief Uses a compute shader to clear a render target to a solid color.
				 * @param gfxc The graphics context to use
				 * @param renderTarget The render target to clear
				 * @param color The color to clear the render target to
				 */
				void clearRenderTarget(GnmxGfxContext &gfxc, const sce::Gnm::RenderTarget* renderTarget, const Vector4& color);

		        /** @brief When clearing a DCC buffer, an 8-bit KEY value must be chosen to be the new value of every byte in the buffer.
                  *        When the intention is to fast-clear a RenderTarget's DCC-compressed COLOR buffer,
                  *        a 'Clear' DCC KEY should be chosen. When the intention is to convert a non-DCC-compressed COLOR buffer
                  *        to a DCC-compressed color buffer, the 'Uncompressed' KEY should be chosen.
                  * @see   clearDccBuffer
		          */
                enum DccKey
                {
                    kDccKeyUncompressed     = 0xFF, ///< uncompressed texels
                    kDccKeyFastClearRGB0A0  = 0x00, ///< the RGBA color [0, 0, 0, 0]
                    kDccKeyFastClearRGB0A1  = 0x40, ///< the RGBA color [0, 0, 0, 1]
                    kDccKeyFastClearRGB1A0  = 0x80, ///< the RGBA color [1, 1, 1, 0]
                    kDccKeyFastClearRGB1A1  = 0xC0, ///< the RGBA color [1, 1, 1, 1]
                };

				/** @brief              Uses a compute shader to clear the DCC buffer of a RenderTarget.
				  * @param gfxc         The graphics context to use
				  * @param renderTarget The RenderTarget to clear the DCC buffer of
				  * @param dccKey       The 8-bit DCC KEY value to clear the DCC buffer to
                  * @see   DccKey
				  */
				void clearDccBuffer(GnmxGfxContext &gfxc, const sce::Gnm::RenderTarget* renderTarget, const uint8_t dccKey);

				/** @brief Uses a compute shader to copy a buffer using RwBuffers with no format conversion.
				 * @param gfxc The graphics context to use
				 * @param bufferDst The buffer to copy to, which must be a multiple of 64 dwords (256 bytes) in size.
				 * @param bufferSrc The buffer to copy from, which must exactly match the size and format of bufferDst
				 */
				void copyBuffer(GnmxGfxContext &gfxc, const sce::Gnm::Buffer* bufferDst, const sce::Gnm::Buffer* bufferSrc);

				/** @brief Uses a compute shader to copy a texture.
				 *
				 *         The function will fail if the following criteria are not met:
				 *         - The source and destination textures must not be NULL.
				 *         - The source texture must have at least as many mip levels as the destination texture.
				 *         - The source texture must have at least as many array slices as the destination texture.
				 *         - The source and destination textures must have the same dimensionality (according to their getDimension() methods). Only 1D, 2D and 3D textures are currently supported.
				 *         - The source and destination textures must have the same width/height/depth, in all relevant dimensions (according to their dimensionality).
				 * @param gfxc The graphics context to use.
				 * @param textureDst The texture to copy to.
				 * @param textureSrc The texture to copy from.
				 */
				void copyTexture(GnmxGfxContext &gfxc, const sce::Gnm::Texture* textureDst, const sce::Gnm::Texture* textureSrc);

				/** @brief Uses a pixel shader to copy a texture to a render target.
				 *
				 * @param gfxc The graphics context to use.
				 * @param renderTargetDestination The render target to copy to.
				 * @param textureSource The texture to copy from.
				 */
				void copyTextureToRenderTarget(GnmxGfxContext &gfxc, const sce::Gnm::RenderTarget* renderTargetDestination, const sce::Gnm::Texture* textureSource);

				/** @brief Uses a compute shader to copy a render target using RwBuffers with no format conversion.
				 * @param gfxc The graphics context to use.
				 * @param renderTargetDst The render target to copy to.
				 * @param renderTargetSrc The render target to copy from, which must exactly match the size and format of renderTargetDst.
				 */
				void copyRenderTarget(GnmxGfxContext &gfxc, const sce::Gnm::RenderTarget* renderTargetDst, const sce::Gnm::RenderTarget* renderTargetSrc);

				/** @brief Uses a compute shader to copy a depth render target using RwBuffers with no format conversion.
				 * @param gfxc The graphics context to use.
				 * @param depthTargetDst The depth render target to copy Z to.
				 * @param depthTargetSrc The depth render target to copy Z from, which must exactly match the size and format of depthTargetDst.
				 */
				void copyDepthTargetZ(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget* depthTargetDst, const sce::Gnm::DepthRenderTarget* depthTargetSrc);

				/** @brief Clears a depth render target by enabling depth clear state, and rendering a RECT primitive.
				 * @param gfxc The graphics context to use.
				 * @param depthTarget The depth render target to clear.
				 * @note All array slices in depthTarget's current array view will be cleared.
				 */
				void clearDepthTarget(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget *depthTarget, const float depthValue);

				/** @brief Clears a stencil buffer by enabling stencil clear state, and rendering a RECT primitive.
				 * @param gfxc The graphics context to use.
				 * @param depthTarget The depth render target to clear the stencil buffer of.
				 * @note All array slices in depthTarget's current array view will be cleared.
				 */
				void clearStencilTarget(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget *depthTarget, const uint8_t stencilValue);

				/** @brief Clears a depth render target and its stencil buffer by enabling depth and stencil clear state, and rendering a RECT primitive.
				 * @param gfxc The graphics context to use.
				 * @param depthTarget The depth render target to clear.
				 * @note All array slices in depthTarget's current array view will be cleared.
				 */
				void clearDepthStencilTarget(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget *depthTarget, const float depthValue, const uint8_t stencilValue);

				/** @brief Clears a depth render target's stencil buffer by setting it to a constant value.
				 */
				void clearStencilSurface(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget *depthTarget, const uint8_t value);

				/** @brief Clears a depth render target's stencil buffer by setting it to the constant value of zero.
				 */
				void clearStencilSurface(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget *depthTarget);

				/** @brief Quickly clears a depth render target by setting all dwords in its HTILE buffer to a single value.
                 *         This version of the function takes as its argument an Htile structure capable of expressing any dword value.
				 */
				void clearHtileSurface(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget *depthTarget, const Gnm::Htile htile);

				/** @brief Quickly clears a depth render target by setting all dwords in its HTILE buffer to a single value.
                 *         This version of the function takes as its argument minZ and maxZ values, capable of expressing only HiZ-only dword values.
				 */
				void clearHtileSurface(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget *depthTarget, const float minZ, const float maxZ);

				/** @brief Quickly clears a depth render target by setting all dwords in its HTILE buffer to a single value.
                 *         This version of the function takes as its argument a Z value, capable of expressing only HiZ-only dword values.
				 */
				void clearHtileSurface(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget *depthTarget, const float Z);

				/** @brief Quickly clears a depth render target by setting all dwords in its HTILE buffer to a single value.
                 *         This version of the function takes no Z argument; minZ and maxZ of 1.f is implied. This makes sense if
                 *         the Z test is < or <=.
				 */
				void clearHtileSurface(GnmxGfxContext &gfxc, const sce::Gnm::DepthRenderTarget *depthTarget);

				/** @brief Quickly clears a render target by zeroing out its CMASK buffer.
				 */
				void clearCmaskSurface(GnmxGfxContext &gfxc, const sce::Gnm::RenderTarget *target);


				/** @brief Quickly clears a render target DCC buffer to a specify clear value using compute shader.
				 */
				void clearDccSurface(GnmxGfxContext &gfxc, const Gnm::RenderTarget *target, sce::Gnm::DccClearValue dccClearValue);


				/** @brief Generates mip maps for a texture by averaging each 2x2 texels of mip0 and storing in mip1,
				 *         then averaging each 2x2 texels of mip1 and storing in mip2, et cetera.
				 *         This binds each destination mip as a render target, and stalls between each pass to
				 *         synchronize the CB$ and L2$.
				 *         This function is optimized for convenience and familiarity, and not for performance.
				 *         The following render state is stomped by this function, and may need to be reset:
				 *         - Depth render target [function sets to depthTarget]
				 *         - screen viewport [function sets to depthTarget's pitch/height]
				 *         - vertex shader / pixel shader [function sets to undefined values]
				 *         - depth stencil control [function disables zwrite and sets compareFunc=NEVER]
				 *         - depth render control [function disables stencil and depth compression]
				 *         - blend control [function disables blending]
				 *         - MRT color mask [function sets to 0xF]
				 *         - primitive type (set to kPrimitiveTypeRectList)
				 *         The following render state is assumed to have been set up by the caller:
				 *         - active shader stages [function assumes only VS/PS are enabled]
				 */
				void generateMipMaps(GnmxGfxContext &gfxc, const sce::Gnm::Texture *texture);
			}

			class Timers
			{
				class Timer
				{
				public:
					uint64_t m_begin;
					uint64_t m_end;
				};
				Timer*   m_timer;
				uint64_t m_timers;
			public:
				void *m_addr;
				static Gnm::SizeAlign getRequiredBufferSizeAlign(uint32_t timers);
				void initialize(void *addr, uint32_t timers);
				void begin(sce::Gnmx::GnmxDrawCommandBuffer& dcb, uint32_t timer);
				void end(sce::Gnmx::GnmxDrawCommandBuffer& dcb, uint32_t timer);
				void begin(sce::Gnmx::GnmxDispatchCommandBuffer& dcb, uint32_t timer);
				void end(sce::Gnmx::GnmxDispatchCommandBuffer& dcb, uint32_t timer);
				uint64_t readTimerInGpuClocks(uint32_t timer) const;
				double readTimerInMilliseconds(uint32_t timer) const;
			};

			/** @brief This submits a command buffer on the GPU, and then stalls the CPU until the GPU is done.
			 */
			void submitAndStall(sce::Gnmx::GnmxDrawCommandBuffer& dcb);

			/** @brief This submits a command buffer on the GPU, and then stalls the CPU until the GPU is done.
			 */
			void submitAndStall(GnmxGfxContext& gfxc);

			/** @brief This rounds a byte size up to the specified alignment.
			 */
			uint64_t roundUpToAlignment(sce::Gnm::Alignment alignment, uint64_t bytes);

			/** @brief This increments a void* until it has the specified alignment.
			 */
			void *roundUpToAlignment(sce::Gnm::Alignment alignment, void *addr);

			/** @brief Initializes a 1D Texture array with a single slice.
			@param[in] width            Width of the Texture in pixels. Must be in the range [1..16384].
			@param[in] numMipLevels     The expected number of MIP levels (including the base level). Must be in the range [1..16].
			@param[in] format           Format of the Texture.
			@param[in] tileModeHint     Requested tile mode for this Texture. The actual tile mode may be different to accommodate hardware restrictions;
										use Texture::getTileMode() to determine the object's final tiling mode.
			@return Minimum size and alignment to use when allocating surface data for this Texture.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign object (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTotalTiledTextureSize()</c> to get the full texture size.
			*/
			sce::Gnm::SizeAlign initAs1d(sce::Gnm::Texture *texture, uint32_t width, uint32_t numMipLevels, sce::Gnm::DataFormat format, sce::Gnm::TileMode tileModeHint);

			/** @brief Initializes a 1D Texture array.
			@param[in] width            Width of the Texture in pixels. Must be in the range [1..16384].
			@param[in] numSlices        Number of slices of the Texture. Must be in the range [1..8192].
			@param[in] numMipLevels     The expected number of MIP levels (including the base level). Must be in the range [1..16].
			@param[in] format           Format of the Texture.
			@param[in] tileModeHint     Requested tile mode for this Texture. The actual tile mode may be different to accommodate hardware restrictions;
										use Texture::getTileMode() to determine the object's final tiling mode.
			@return Minimum size and alignment to use when allocating surface data for this Texture.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign object (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTotalTiledTextureSize()</c> to get the full texture size.
			*/
			sce::Gnm::SizeAlign initAs1dArray(sce::Gnm::Texture *texture, uint32_t width, uint32_t numSlices, uint32_t numMipLevels, sce::Gnm::DataFormat format, sce::Gnm::TileMode tileModeHint);

			/** @brief Initializes a Texture with one slice.
			@param[in] width            Width of the Texture in pixels. Must be in the range [1..16384].
			@param[in] height           Height of the Texture in pixels. Must be in the range [1..16384].
			@param[in] numMipLevels     The expected number of MIP levels (including the base level). Must be in the range [1..16]. This must be set to 1 if <c><i>numFragments</i></c> is greater than <c>kNumFragments1</c>.
			@param[in] format           Format of the Texture.
			@param[in] tileModeHint     Requested tile mode for this Texture. The actual tile mode may be different to accommodate hardware restrictions;
										use Texture::getTileMode() to determine the object's final tiling mode.
			@param[in] numFragments     Number of fragments to store per pixel. This must be set to <c>kNumFragments1</c> if <c><i>numMipLevels</i></c> is greater than 1.
			@return Minimum size and alignment to use when allocating surface data for this Texture.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign object (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTotalTiledTextureSize()</c> to get the full texture size.
			*/
			sce::Gnm::SizeAlign initAs2d(sce::Gnm::Texture *texture, uint32_t width, uint32_t height, uint32_t numMipLevels, sce::Gnm::DataFormat format, sce::Gnm::TileMode tileModeHint, sce::Gnm::NumFragments numFragments);

			/** @brief Initializes a Texture Array with a specified number of slices.
			@param[in] width            Width of the Texture in pixels. Must be in the range [1..16384].
			@param[in] height           Height of the Texture in pixels. Must be in the range [1..16384].
			@param[in] numSlices        Number of slices of the Texture. Must be in the range [1..8192].
			@param[in] numMipLevels     The expected number of MIP levels (including the base level). Must be in the range [1..16]. This must be set to 1 if <c><i>numFragments</i></c> is greater than kNumFragments1.
			@param[in] format           Format of the Texture.
			@param[in] tileModeHint     Requested tile mode for this Texture. The actual tile mode may be different to accommodate hardware restrictions;
										use Texture::getTileMode() to determine the object's final tiling mode.
			@param[in] numFragments     Number of fragments to store per pixel. This must be set to kNumFragments1 if <c><i>numMipLevels</i></c> is greater than 1.
			@param[in] isCubemap        Whether the texture array should be treated as a cubemap.
			@return Minimum size and alignment to use when allocating surface data for this Texture.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign object (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTotalTiledTextureSize()</c> to get the full texture size.
			*/
			sce::Gnm::SizeAlign initAs2dArray(sce::Gnm::Texture *texture, uint32_t width,uint32_t height,uint32_t numSlices,uint32_t pitch,uint32_t numMipLevels, sce::Gnm::DataFormat format, sce::Gnm::TileMode tileModeHint, sce::Gnm::NumFragments numFragments,bool isCubemap);

			sce::Gnm::SizeAlign initAs2dArray(sce::Gnm::Texture *texture,uint32_t width,uint32_t height,uint32_t numSlices,uint32_t numMipLevels,sce::Gnm::DataFormat format,sce::Gnm::TileMode tileModeHint,sce::Gnm::NumFragments numFragments,bool isCubemap);

			/** @brief Initializes a Texture as a cubic environment map.
			@param[in] width            Width of each cube face in pixels. Must be in the range [1..16384] and equal to <c><i>height</i></c>.
			@param[in] height           Height of each cube face in pixels. Must be in the range [1..16384] and equal to <c><i>width</i></c>.
			@param[in] numMipLevels     The expected number of MIP levels (including the base level). Must be in the range [1..16].
			@param[in] format           Format of the Texture.
			@param[in] tileModeHint     Requested tile mode for this Texture. The actual tile mode may be different to accommodate hardware restrictions;
										use Texture::getTileMode() to determine the object's final tiling mode.
			@return Minimum size and alignment to use when allocating surface data for this Texture.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign object (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTotalTiledTextureSize()</c> to get the full texture size.
			@note If numMipLevels > 1, the memory required for a cubemap will increase by 33% due to GPU padding requirements.
			*/
			sce::Gnm::SizeAlign initAsCubemap(sce::Gnm::Texture *texture, uint32_t width, uint32_t height, uint32_t numMipLevels, sce::Gnm::DataFormat format, sce::Gnm::TileMode tileModeHint);

			/** @brief Initializes a Texture as an array of cubic environment maps.
			@param[in] width            Width of each cube face in pixels. Must be in the range [1..16384] and equal to <c><i>height</i></c>.
			@param[in] height           Height of each cube face in pixels. Must be in the range [1..16384] and equal to <c><i>width</i></c>.
			@param[in] numCubemaps      The number of slices in the cubemap array. Note that the hardware will effectively "flatten" an array of cubemaps,
										so the slice index of a particular face of the cubemap is <c>6*<i>sliceIndex</i> + <i>faceIndex</i></c>. See the sce::Gnm::CubemapFace enum for
										the face ordering. Must be in the range [1..1365].
			@param[in] numMipLevels     The expected number of MIP levels (including the base level). Must be in the range [1..16].
			@param[in] format           Format of the Texture.
			@param[in] tileModeHint     Requested tile mode for this Texture. The actual tile mode may be different to accommodate hardware restrictions;
										use Texture::getTileMode() to determine the object's final tiling mode.
			@return Minimum size and alignment to use when allocating surface data for this Texture.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign object (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTotalTiledTextureSize()</c> to get the full texture size.
			@note If numMipLevels > 1, the memory required for a cubemap array will increase by up to 50% due to GPU padding requirements.
			@deprecated This function can not create textures optimized for NEO mode; the variant that takes a TextureSpec should be used instead.
			@see CubemapFace
			*/
			sce::Gnm::SizeAlign initAsCubemapArray(sce::Gnm::Texture *texture, uint32_t width, uint32_t height, uint32_t numCubemaps, uint32_t numMipLevels, sce::Gnm::DataFormat format, sce::Gnm::TileMode tileModeHint);

			/** @brief Initializes a 3D Texture.
			@param[in] width            Width of the Texture in pixels. Must be in the range [1..16384].
			@param[in] height           Height of the Texture in pixels. Must be in the range [1..16384].
			@param[in] depth            Depth of the Texture. Must be in the range [1..8192].
			@param[in] numMipLevels     The expected number of MIP levels (including the base level). Must be in the range [1..16].
			@param[in] format           Format of the Texture.
			@param[in] tileModeHint     Requested tile mode for this Texture. The actual tile mode may be different to accommodate hardware restrictions;
										use Texture::getTileMode() to determine the object's final tiling mode.
			@return Minimum size and alignment to use when allocating surface data for this Texture.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign object (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTotalTiledTextureSize()</c> to get the full texture size.
			*/
			sce::Gnm::SizeAlign initAs3d(sce::Gnm::Texture *texture, uint32_t width, uint32_t height, uint32_t depth, uint32_t numMipLevels, sce::Gnm::DataFormat format, sce::Gnm::TileMode tileModeHint);

			/** @brief Initializes a RenderTarget object with a specific pitch.

			@param[in] width			The desired width in pixels. The actual surface width may be padded to accommodate hardware restrictions. The valid range is [1..16384].
			@param[in] height			The desired height in pixels. The actual surface height may be padded to accommodate hardware restrictions. The valid range is [1..16384].
			@param[in] numSlices		The desired number of slices (elements in an array of RenderTarget objects). The actual number of slices may be padded to accommodate hardware restrictions. The valid range is [1..2048].
			@param[in] pitch            The desired pitch in pixels. If this value is zero, the library will compute the minimum legal pitch for the surface given the restrictions
										imposed by other surface parameters; otherwise the provided pitch will be used, provided it also conforms to hardware restrictions. A non-zero
										pitch that does not conform to hardware restrictions will cause initialization to fail. The valid range is [0..16384] subject to hardware restrictions.
			@param[in] rtFormat			The desired pixel data format. This format must be a supported format for render targets.
			@param[in] tileModeHint		The desired tiling mode. The actual tiling mode may be different in order to accommodate hardware restrictions;
										use RenderTarget::getTileMode() to determine the object's final tiling mode.
			@param[in] numSamples		A log2 of the number of samples per pixel. This must not be less than <c><i>numFragments</i></c>.
			@param[in] numFragments		A log2 of the number of fragments per pixel. This must not be greater than <c><i>numSamples</i></c>.
			@param[out] cmaskSizeAlign	If non-<c>NULL</c>, fast-clear will be enabled for this RenderTarget. This feature is used to quickly clear the contents of a RenderTarget to a solid color.
										This feature is not supported for 128bpp RenderTargets.
										The caller must allocate the CMASK surface using the SizeAlign object stored at this address and bind it using #setCmaskAddress().
			@param[out] fmaskSizeAlign	If non-<c>NULL</c>, color compression will be enabled for this RenderTarget. This feature is used to reduce memory traffic for multisampled render targets.
										The caller must allocate the FMASK surface using the SizeAlign object stored at this address and bind it using #setFmaskAddress().
										If this feature is enabled, <c><i>cmaskSizeAlign</i></c> must also be non-<c>NULL</c> (FMASKs require CMASKs), and <c><i>numSamples</i></c> must be greater than <c>kNumSamples1</c>.
			@return The minimum size and alignment required for the render target's pixel data.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign object (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTiledSurfaceSize()</c> to get the full surface size.
			@deprecated This function cannot create targets optimized for NEO mode; the variant that takes a RenderTargetSpec should be used instead.
			*/
			sce::Gnm::SizeAlign init(sce::Gnm::RenderTarget *renderTarget, uint32_t width, uint32_t height, uint32_t numSlices, uint32_t pitch, sce::Gnm::DataFormat rtFormat, sce::Gnm::TileMode tileModeHint, sce::Gnm::NumSamples numSamples, sce::Gnm::NumFragments numFragments, sce::Gnm::SizeAlign *cmaskSizeAlign, sce::Gnm::SizeAlign *fmaskSizeAlign);

			sce::Gnm::SizeAlign init(sce::Gnm::RenderTarget *renderTarget,uint32_t width,uint32_t height,uint32_t numSlices,sce::Gnm::DataFormat rtFormat,sce::Gnm::TileMode tileModeHint,sce::Gnm::NumSamples numSamples,sce::Gnm::NumFragments numFragments,sce::Gnm::SizeAlign *cmaskSizeAlign,sce::Gnm::SizeAlign *fmaskSizeAlign);

			/** @brief Initializes a DepthRenderTarget with a specified number of slices and a specific pitch.

			@param[in] width             Width of the DepthRenderTarget in pixels. Valid range is <c>[1..16384]</c>.
			@param[in] height            Height of the DepthRenderTarget in pixels. Valid range is <c>[1..16384]</c>.
			@param[in] numSlices         Number of slices of the DepthRenderTarget. Valid range is <c>[1..2048]</c>.
			@param[in] pitch             The requested pitch, in pixels. If this value is zero, the library will compute the minimum legal pitch for the surface given the restrictions
											imposed by other surface parameters; otherwise, the requested pitch will be used, provided it also conforms to hardware restrictions. A non-zero
											pitch that does not conform to hardware restrictions will cause initialization to fail. The valid range is [0..16384], subject to hardware restrictions.
			@param[in] zFormat           Z Format of the DepthRenderTarget.
			@param[in] stencilFormat     Stencil format of the DepthRenderTarget. This format will be ignored if <c><i>stencilSizeAlign</i></c> is <c>NULL</c>.
			@param[in] depthTileModeHint Requested tile mode of the DepthRenderTarget, used for both depth and stencil surfaces.
											Actual tile mode may be different due to hardware restrictions; use DepthRenderTarget::getTileMode() to determine what was actually used.
											Must be one of the <c>kTileModeDepth_*dThin</c> modes (indices 0-7).
			@param[in] numFragments      Number of sub-pixel values to store (1, 2, 4 or 8).
			@param[out] stencilSizeAlign Returns size and alignment need for the stencil buffer. If non-<c>NULL</c>, the user must provide a stencil buffer address using setStencilWriteAddress().
			@param[out] htileSizeAlign  Returns size and alignment need for the HTILE buffer if depth buffer is to be compressed. If non-NULL, HTILE acceleration will be
			enabled for this surface automatically, and you must call setHtileAddress() to provide an HTILE buffer address.
			@return The minimum required size and alignment for the depth surface.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTiledSurfaceSize()</c> to get the full surface size.
			@deprecated This function cannot create targets optimized for NEO mode; the variant that takes a DepthRenderTargetSpec should be used instead.
			*/
			sce::Gnm::SizeAlign init(sce::Gnm::DepthRenderTarget *depthRenderTarget, uint32_t width, uint32_t height, uint32_t numSlices, uint32_t pitch, sce::Gnm::ZFormat zFormat, sce::Gnm::StencilFormat stencilFormat, sce::Gnm::TileMode depthTileModeHint, sce::Gnm::NumFragments numFragments, sce::Gnm::SizeAlign *stencilSizeAlign, sce::Gnm::SizeAlign *htileSizeAlign);

			/** @brief Initializes a DepthRenderTarget.

			@param[in] width             Width of the DepthRenderTarget in pixels. Valid range is <c>[1..16384]</c>.
			@param[in] height            Height of the DepthRenderTarget in pixels. Valid range is <c>[1..16384]</c>.
			@param[in] zFormat           Z format of the DepthRenderTarget.
			@param[in] stencilFormat     Stencil format of the DepthRenderTarget. This format will be ignored if <c><i>stencilSizeAlign</i></c> is <c>NULL</c>.
			@param[in] depthTileModeHint Requested tile mode of the DepthRenderTarget. The actual tile mode may be different to accommodate hardware restrictions;
											use DepthRenderTarget::getTileMode() to determine the object's final tiling mode. Must be one of the <c>kTileModeDepth_*dThin</c> modes (indices 0-7).
			@param[in] numFragments      Number of sub-pixel values to store (1, 2, 4 or 8).
			@param[out] stencilSizeAlign Receives the size and alignment need for the stencil buffer if it exists. If this argument is set to a value other than <c>NULL</c>, the user must provide a stencil buffer address using setStencilWriteAddress().
			@param[out] htileSizeAlign   Receives the size and alignment need for the HTILE buffer if depth buffer is to be compressed. If this argument is set to a value other than <c>NULL</c>, HTILE acceleration will automatically be
											enabled for this surface, and the user must provide an HTILE buffer address using setHtileAddress().
			@return The minimum required size and alignment for the depth surface.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use <c>GpuAddress::computeTiledSurfaceSize()</c> to get the full surface size.
			@deprecated This function cannot create targets optimized for NEO mode; the variant that takes a DepthRenderTargetSpec should be used instead.
			*/
			sce::Gnm::SizeAlign init(sce::Gnm::DepthRenderTarget *depthRenderTarget, uint32_t width, uint32_t height, sce::Gnm::ZFormat zFormat, sce::Gnm::StencilFormat stencilFormat, sce::Gnm::TileMode depthTileModeHint, sce::Gnm::NumFragments numFragments, sce::Gnm::SizeAlign *stencilSizeAlign, sce::Gnm::SizeAlign *htileSizeAlign);

			/** @brief Initializes a DepthRenderTarget with a specified number of slices.

			@param[in] width             Width of the DepthRenderTarget in pixels. Valid range is <c>[1..16384]</c>.
			@param[in] height            Height of the DepthRenderTarget in pixels. Valid range is <c>[1..16384]</c>.
			@param[in] numSlices         Number of slices of the DepthRenderTarget. Valid range is <c>[1..2048]</c>.
			@param[in] zFormat           Z Format of the DepthRenderTarget.
			@param[in] stencilFormat     Stencil Format of the DepthRenderTarget. This format will be ignored if <c><i>stencilSizeAlign</i></c> is <c>NULL</c>.
			@param[in] depthTileModeHint Requested tile mode of the DepthRenderTarget, used for both depth and stencil surfaces.
											Actual tile mode may be different due to hardware restrictions; use DepthRenderTarget::getTileMode() to determine what was actually used.
											Must be one of the <c>kTileModeDepth_*dThin</c> modes (indices 0-7).
			@param[in] numFragments      Number of sub-pixel values to store (1, 2, 4 or 8).
			@param[out] stencilSizeAlign Returns size and alignment need for the stencil buffer. If non-<c>NULL</c>, the user must provide a stencil buffer address using setStencilWriteAddress().
			@param[out] htileSizeAlign  Returns size and alignment need for the HTILE buffer if depth buffer is to be compressed. If non-<c>NULL</c>, HTILE acceleration will be
			enabled for this surface automatically, and you must call setHtileAddress() to provide an HTILE buffer address.
			@return The minimum required size and alignment for the depth surface.
					If initialization fails, the SizeAlign result will contain <c>(0,0)</c>.
					If initialization succeeds, but the size is too large for a SizeAlign (4 GB or larger), the result will contain <c>(0,alignment)</c>.
					In this case, use the GpuAddress library function <c>computeTiledSurfaceSize()</c> to get the full surface size.
			@deprecated This function cannot create targets optimized for NEO mode; the variant that takes a DepthRenderTargetSpec should be used instead.
			*/
			sce::Gnm::SizeAlign init(sce::Gnm::DepthRenderTarget *depthRenderTarget, uint32_t width, uint32_t height, uint32_t numSlices, sce::Gnm::ZFormat zFormat, sce::Gnm::StencilFormat stencilFormat, sce::Gnm::TileMode depthTileModeHint, sce::Gnm::NumFragments numFragments, sce::Gnm::SizeAlign *stencilSizeAlign, sce::Gnm::SizeAlign *htileSizeAlign);

		    /** @brief Which buffers to copy, when doing a one-fragment copy from a DepthRenderTarget
		    */
            typedef enum BuffersToCopy
            {
                kBuffersToCopyDepth           = 0x1, ///< Copy only the DEPTH buffer.
                kBuffersToCopyStencil         = 0x2, ///< Copy only the STENCIL buffer.
                kBuffersToCopyDepthAndStencil = 0x3, ///< Copy both the DEPTH and the STENCIL buffers.
            } BuffersToCopy;

			/** @brief Efficiently copies one sample of a DepthRenderTarget to the COLOR buffer of a RenderTarget.

	 	    @param[out] dcb             The command buffer to which commands should be written.
			@param[in]  source          The DepthRenderTarget to copy from.
			@param[in]  destination     The RenderTarget to copy to.
            @param[in]  buffersToCopy   Whether to copy the DEPTH buffer, the STENCIL buffer, or both.
            @param[in]  sampleIndex     The index of the sample to be copied.
			@return                     If the copy is successful, the return value will be 0. Otherwise, a non-zero error code will be returned.
			*/
			SCE_GNM_API_DEPRECATED_MSG_NOINLINE("Please use Gnmx::copyOneFragment instead.")
            int copyOneSample(sce::Gnmx::GnmxDrawCommandBuffer *dcb, Gnm::DepthRenderTarget source, Gnm::RenderTarget destination, BuffersToCopy buffersToCopy, int sampleIndex);

			/** @brief Efficiently copies one sample of a DepthRenderTarget to the DEPTH buffer of a DepthRenderTarget.

	 	    @param[out] dcb             The command buffer to which commands should be written.
			@param[in]  source          The DepthRenderTarget to copy from.
			@param[in]  destination     The DepthRenderTarget to copy to.
            @param[in]  buffersToCopy   Whether to copy the DEPTH buffer, the STENCIL buffer, or both.
            @param[in]  sampleIndex     The index of the sample to be copied.
			@return                     If the copy is successful, the return value will be 0. Otherwise, a non-zero error code will be returned.
			*/
			SCE_GNM_API_DEPRECATED_MSG_NOINLINE("Please use Gnmx::copyOneFragment instead.")
            int copyOneSample(sce::Gnmx::GnmxDrawCommandBuffer *dcb, Gnm::DepthRenderTarget source, Gnm::DepthRenderTarget destination, BuffersToCopy buffersToCopy, int sampleIndex);

			/** @brief Efficiently copies one sample of a DepthRenderTarget to the COLOR buffer of a RenderTarget.

	 	    @param[out] gfxc            The command buffer to which commands should be written.
			@param[in]  source          The DepthRenderTarget to copy from.
			@param[in]  destination     The RenderTarget to copy to.
            @param[in]  buffersToCopy   Whether to copy the DEPTH buffer, the STENCIL buffer, or both.
            @param[in]  sampleIndex     The index of the sample to be copied.
			@return                     If the copy is successful, the return value will be 0. Otherwise, a non-zero error code will be returned.
			*/
			SCE_GNM_API_DEPRECATED_MSG_NOINLINE("Please use Gnmx::copyOneFragment instead.")
            int copyOneSample(sce::Gnmx::GnmxGfxContext *gfxc, Gnm::DepthRenderTarget source, Gnm::RenderTarget destination, BuffersToCopy buffersToCopy, int sampleIndex);

			/** @brief Efficiently copies one sample of a DepthRenderTarget to the DEPTH buffer of a DepthRenderTarget.

	 	    @param[out] gfxc            The command buffer to which commands should be written.
			@param[in]  source          The DepthRenderTarget to copy from.
			@param[in]  destination     The DepthRenderTarget to copy to.
            @param[in]  buffersToCopy   Whether to copy the DEPTH buffer, the STENCIL buffer, or both.
            @param[in]  sampleIndex     The index of the sample to be copied.
			@return                     If the copy is successful, the return value will be 0. Otherwise, a non-zero error code will be returned.
			*/
			SCE_GNM_API_DEPRECATED_MSG_NOINLINE("Please use Gnmx::copyOneFragment instead.")
            int copyOneSample(sce::Gnmx::GnmxGfxContext *gfxc, Gnm::DepthRenderTarget source, Gnm::DepthRenderTarget destination, BuffersToCopy buffersToCopy, int sampleIndex);
		}
	}
}
#endif /* _SCE_GNM_TOOLKIT_H */
