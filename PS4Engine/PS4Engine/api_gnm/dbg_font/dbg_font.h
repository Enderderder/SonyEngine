/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef SCE_DBGFONT_H
#define SCE_DBGFONT_H

#include <gnm.h>
#include <gnmx.h>
#include "../toolkit/allocators.h"

namespace sce
{
	namespace DbgFont
	{
		/** @brief These are the colors for text character cells.
		 */
		typedef enum CellColor
		{
			kBlack = 0,
			kBlue = 1,
			kGreen = 2,
			kCyan = 3,
			kRed = 4,
			kMagenta = 5,
			kBrown = 6,
			kLightGray = 7,
			kGray = 8,
			kLightBlue = 9,
			kLightGreen = 10,
			kLightCyan = 11,
			kLightRed = 12,
			kLightMagenta = 13,
			kYellow = 14,
			kWhite = 15
		} CellColor;

		/** @brief These are the character codes for graphical characters.
		 */
		typedef enum CharacterCode
		{
			kFirstLineDrawCode = 128,
			kLastLineDrawCode  = 143,
			kDot               = 128,
			kLeft              = 129,
			kUp                = 130,
			kLowerRightCorner  = 131,
			kRight             = 132,
			kHorizontalLine    = 133,
			kLowerLeftCorner   = 134,
			kUpwardT           = 135,
			kDown              = 136,
			kUpperRightCorner  = 137,
			kVerticalLine      = 138,
			kLeftwardT         = 139,
			kUpperLeftCorner   = 140,
			kDownwardT         = 141,
			kRightwardT        = 142,
			kCross             = 143,
			kCircleLeft        = 144,
			kCrossLeft         = 145,
			kSquareLeft        = 146,
			kTriangleLeft      = 147,
			kCircleRight       = 148,
			kCrossRight        = 149,
			kSquareRight       = 150,
			kTriangleRight     = 151,
		} CharacterCode;

		/** @brief	Initializes DbgFont with an ONION and GARLIC allocator, and will call the allocators'
		 **			'allocate' function to obtain pointers to memory.
		 **
		 **	@param allocators a pointer to an Allocators object
		 **/
		void initializeWithAllocators(Gnmx::Toolkit::Allocators *allocators);

		/** @brief memory sufficient to represent a single character cell
		 */
		class Cell
		{
		public:
			uint32_t m_character:16; ///< extended ASCII code of character. code 0 means to not render.
			uint32_t m_foregroundColor:4; ///< offset into color table for foreground color of cell
			uint32_t m_foregroundAlpha:4; ///< alpha of foreground color 0..15
			uint32_t m_backgroundColor:4; ///< offset into color table for background color of cell
			uint32_t m_backgroundAlpha:4; ///< alpha of background color 0..15
		};

		/** @brief The screen position of a text cursor, in units of text characters
		 */
		class Cursor
		{
		public:
			uint32_t m_x; ///< The distance from the left side of the screen, in units of text characters
			uint32_t m_y; ///< The distance from the top of the screen, in units of text characters
		};

		class Screen;

		/** @brief A region in a screen with an associated cursor
		 */
		class Window
		{
			enum {kMaximumColorStackLength = 16};
			Cell m_colorStack[kMaximumColorStackLength];
			uint64_t m_colorStackTop;
		public:
			Screen *m_screen;
			Cursor m_cursor;
			Cell m_cell;
			uint32_t m_positionXInCharactersRelativeToScreen;
			uint32_t m_positionYInCharactersRelativeToScreen;
			uint32_t m_widthInCharacters;
			uint32_t m_heightInCharacters;
			uint32_t m_reserved;

			Cell getCell(uint32_t positionXInCells, uint32_t positionYInCells) const;
			void setCell(uint32_t positionXInCells, uint32_t positionYInCells, Cell cell);

			void initialize(Screen *screen);

			void initialize(Window *window, uint32_t positionXInCharacters, uint32_t positionYInCharacters, uint32_t widthInCharacters, uint32_t heightInCharacters);

			/** @brief Clears the screen of text, and sets the text color of each cell to the same value.
				@param color the color to set all cells of the screen to
				*/
			void clear(Cell cell);

			/** @brief Moves the cursor one text character to the right.
				*/
			void advanceCursor();

			/** @brief Causes the cursor position to move to the left side of the screen, and advance downward by one line.
				*/
			void newLine();

			/** @brief Writes one ASCII character to the screen at the cursor position, then advances the cursor.			   
				@param c the ASCII character to write to the screen at the cursor position.
				*/
			void putCell(Cell cell);

			/** @brief Writes a null-terminated ASCII string to the screen at the cursor position, advancing the cursor as it does.
				@param c the null-terminated ASCII string to write to the screen at the cursor position.
				*/
			void putString(const char* c);

			/** @brief Moves the cursor to the desired screen position, in units of text characters
				@param x the number of text characters from the left of the screen
				@param y the number of text characters from the top of the screen
				*/
			void setCursor(int x, int y);

			/** @brief This sets a box-shaped region of a display to a requested color, without disturbing its ASCII value
			    @param left The distance in character cells from the left side of the screen to the left side of the box
				@param top The distance in character cells from the top of the screen to the top of the box
				@param right The distance in character cells from the left side of the screen to the right side of the box
				@param bottom The distance in character cells from the top of the screen to the bottom side of the box
				@param color The color to set the cells to
				*/
			void setCells(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, Cell cell);

			/** @brief This renders the outline of a box using line-draw characters
			    @param left The distance in character cells from the left side of the screen to the left side of the box
				@param top The distance in character cells from the top of the screen to the top of the box
				@param right The distance in character cells from the left side of the screen to the right side of the box
				@param bottom The distance in character cells from the top of the screen to the bottom side of the box
				@param color The color to set the outline cells to
				*/
			void renderOutlineBox(int left, int top, int right, int bottom, Cell cell);
			void renderOutlineBox(Cell cell);

			/** @brief This renders the outline of a box using line-draw characters, with a vertical partition in its middle
			    @param left The distance in character cells from the left side of the screen to the left side of the box
				@param top The distance in character cells from the top of the screen to the top of the box
				@param right The distance in character cells from the left side of the screen to the right side of the box
				@param bottom The distance in character cells from the top of the screen to the bottom side of the box
				@param color The color to set the outline cells to
				@param middle the distance in character cells from the left side of the screen to the vertical partition
				*/
			void renderSplitOutlineBox(int left, int top, int right, int bottom, Cell cell, int middle);

			/** @brief Writes a formatted null-terminated ASCII string to the screen at the cursor position, advancing the cursor as it does.
				@param format the formatted null-terminated ASCII string to write to the screen at the cursor position.
				*/

			void printf(const char* format, ...);

			/** @brief This prints an ASCII string from within a graphics context on the GPU.
                @param gfxc graphics context in which to do printf
				@param length maximum possible length of string, after formatting
                @param format ASCII string format for subsequent arguments
                */
			void deferredPrintf(Gnmx::GnmxGfxContext& gfxc, uint32_t x, uint32_t y, const Cell& cell, uint32_t length, const char* format, ...);

			const char *processHexInteger(uint32_t *dest, const char* c);
			const char *processAlpha(const char *c);
			const char *processForegroundColor(const char *c);
			const char *processBackgroundColor(const char *c);
			const char *processCommand(const char *c, Cell *wordBuffer, uint32_t &wordLength);
		};

		/** @brief A screen of text characters
		 */
		class Screen
		{
		public:
			Cell *m_cell;
			Gnm::Buffer m_cellBuffer;
			uint32_t m_widthInCharacters;
			uint32_t m_heightInCharacters;
			struct HardwareCursors
			{
				float m_position[2][2];
				float m_size[2][2];
				uint32_t m_color[2][2];
			};
			HardwareCursors m_hardwareCursors;

			Gnm::SizeAlign calculateRequiredBufferSizeAlign(uint32_t widthInCharacters, uint32_t heightInCharacters);
			void initialize(void *addr, uint32_t widthInCharacters, uint32_t heightInCharacters);

			Cell getCell(uint32_t positionXInCells, uint32_t positionYInCells) const;

			/** @brief This returns a reference to a specific cell of a diplay.
			    @param x The distance in character cells from the left side of the screen to the cell
				@param y The distance in character cells from the top of the screen to the cell
				*/
			void setCell(uint32_t positionXInCells, uint32_t positionYInCells, Cell cell);

			/** @brief This returns a reference to a specific cell of a diplay.
			    @param x The distance in character cells from the left side of the screen to the cell
				@param y The distance in character cells from the top of the screen to the cell
				*/
			void setCells(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, Cell cell);

			/** @brief Render a full-screen quad in a graphics context, which will render a screen of text.
				@param gfxc the graphics context in which to render the full-screen quad.
				@param target the render target on which to render the screen of text.
				*/
			void render(Gnmx::GnmxGfxContext& gfxc, sce::Gnm::RenderTarget* target, uint32_t log2CharacterWidthInPixels, uint32_t log2CharacterHeightInPixels, int32_t horizontalPixelOffset, int32_t verticalPixelOffset);

			/** @brief Render a full-screen quad in a graphics context, which will render a screen of text.
				@param gfxc the graphics context in which to render the full-screen quad.
				@param target the render target on which to render the screen of text.
				*/
			void render(Gnmx::GnmxGfxContext& gfxc, sce::Gnm::RenderTarget* target, uint32_t log2CharacterWidthInPixels, uint32_t log2CharacterHeightInPixels);

			/** @brief Queue a single compute dispatch to a graphics context, which will render a screen of text.
				@param gfxc the graphics context in which to queue the compute dispatch.
				@param texture the texture on which to render the screen of text.
				*/
			void render(Gnmx::GnmxGfxContext& gfxc, sce::Gnm::Texture* texture, uint32_t log2CharacterWidthInPixels, uint32_t log2CharacterHeightInPixels, int32_t horizontalPixelOffset, int32_t verticalPixelOffset);

			/** @brief Queue a single compute dispatch to a graphics context, which will render a screen of text.
				@param gfxc the graphics context in which to queue the compute dispatch.
				@param texture the texture on which to render the screen of text.
				*/
			void render(Gnmx::GnmxGfxContext& gfxc, sce::Gnm::Texture* texture, uint32_t log2CharacterWidthInPixels, uint32_t log2CharacterHeightInPixels);

			// BEGINNING OF DEPRECATED THINGS ------------------------------------------------------------------------------

			// END OF DEPRECATED THINGS ------------------------------------------------------------------------------
		};

	}
}

#endif // #ifndef SCE_DBGFONT_H
