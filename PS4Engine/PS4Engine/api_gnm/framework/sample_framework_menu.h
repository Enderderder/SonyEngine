/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#ifndef _SCE_GNM_FRAMEWORK_SAMPLE_FRAMEWORK_MENU_H_
#define _SCE_GNM_FRAMEWORK_SAMPLE_FRAMEWORK_MENU_H_

#include "../dbg_font/dbg_font.h"
#include "../toolkit/toolkit.h"
#include "controller.h"
#include <stdio.h>

namespace Framework
{
	class MenuHost;
	class MenuCommand;
	class MenuStackEntry;

	struct MenuItemText
	{
		const char* m_name;
		const char* m_documentation;
	};

	class MenuItem
	{
	public:
		MenuItemText m_menuItemText;
		MenuCommand* m_menuCommand;
	};

	class Menu
	{
	public:
		static sce::DbgFont::Cell s_boxColor;		
		// 0: kUnselected,
		// 1: kSelected,
		// 2: kUnselectedDisabled,
		// 3: kSelectedDisabled
		static sce::DbgFont::Cell s_color[4];

		Menu();
		virtual ~Menu();
		enum {kMaximumMenuItems = 32};
		MenuItem m_menuItem[kMaximumMenuItems];
		uint32_t m_menuItems;
		uint32_t m_selectedMenuItemIndex;
		uint32_t m_menuItemNameLength;
		uint32_t m_reserved;
		MenuItem* getSelectedMenuItem();
		void appendMenuItems(                uint32_t count, const MenuItem* menuItem);
		void insertMenuItems(uint32_t index, uint32_t count, const MenuItem* menuItem);
		void removeMenuItems(uint32_t index, uint32_t count);
		void removeAllMenuItems();
		void recalculateMenuWidth();
		virtual void menuDisplay(sce::DbgFont::Window* dest, uint32_t x, uint32_t y);
		virtual void menuInput(MenuHost*);
		virtual uint32_t menuSelectedItem() { return m_selectedMenuItemIndex; }
		virtual void menuDisplayDocumentation(sce::DbgFont::Window* dest);
		virtual void adjustChildMenuPlacement(MenuStackEntry* menuStackEntry) const;
		void justBeenPushed();
		void print(FILE* file);
	};

	class MenuStackEntry
	{
	public:
		Menu* m_menu; // the menu object itself
		uint32_t m_x; // distance in character cells from left side of window
		uint32_t m_y; // distance in character cells from right side of window
	};

	class MenuStack
	{
	public:
		enum { kMaximumMenus = 8 };
		MenuStackEntry m_entry[kMaximumMenus];
		uint32_t m_count;
		uint32_t m_reserved;
		MenuStack()
		: m_count(0)
		{
		}
		MenuStackEntry* getEntry(uint32_t i)
		{
			return &m_entry[i];
		}
		MenuStackEntry* top() 
		{ 
			SCE_GNM_INLINE_ASSERT(m_count > 0);
			return &m_entry[m_count-1]; 
		}
		void pop() 
		{ 
			SCE_GNM_INLINE_ASSERT(m_count > 0);
			--m_count; 
		}
		void push(MenuStackEntry entry)
		{
			SCE_GNM_INLINE_ASSERT(m_count < kMaximumMenus);
			m_entry[m_count++] = entry;
		}
	};

	class MenuHost
	{
	public:
		MenuStack m_menuStack;
		double *m_actualSecondsPerFrame;
        uint32_t m_cameraControlMode;
		void clearMenuItems();
		void addMenuItems( const MenuItem* menuItem, uint32_t menuItems );
		Input::ControllerContext *m_controllerContext;
	};

	class MenuCommand
	{
	protected:
		bool matches(const MenuItem* menuItem) const;
	public:
		MenuItemText m_menuItemText;
		virtual ~MenuCommand() {}
		virtual void adjust(int) const = 0;
		virtual bool isEnabled() const {return true;}  
		virtual bool isDocumented() const {return isEnabled();}
		virtual void input(MenuHost*);
		virtual Menu* getMenu() { return 0; }
		virtual void printName(sce::DbgFont::Window* dest);
		virtual void printValue(sce::DbgFont::Window* dest);
		void print(sce::DbgFont::Window *left, sce::DbgFont::Window *right);
		virtual void menuJustBeenPushed(Menu*, uint32_t) {}
		void print(FILE* file);
	};

	class Validator
	{
	public:
		virtual bool isValid(uint32_t value) const = 0;
		virtual ~Validator();
	};

	class MenuCommandSetUint : public MenuCommand
	{
	protected:
		uint32_t* m_target;
		Validator* m_validator;
	public:
		uint32_t m_value;
		uint32_t m_reserved;
		void set(uint32_t* target, uint32_t value, MenuItemText const *menuItemText, Validator* validator);
		virtual void adjust(int) const;
		virtual bool isEnabled() const; 
		virtual void printName(sce::DbgFont::Window* window);
		virtual void menuJustBeenPushed(Menu*, uint32_t);
	};

	class MenuCommandEnum : public MenuCommand
	{
	protected:
		MenuItemText const* m_menuItemText;
		uint32_t m_menuItems;
		uint32_t m_reserved;
		uint32_t* m_target;
		Menu m_menu;
		enum {kMaximumMenuItems = 32};
		MenuCommandSetUint m_menuCommand[kMaximumMenuItems];
		MenuItem m_menuItem[kMaximumMenuItems];
		Validator* m_validator;
		void initialize(MenuItemText const* menuItemText, uint32_t menuItems, uint32_t* target, Validator* validator);
	public:
		template< typename T > MenuCommandEnum(MenuItemText const* menuItemText, uint32_t menuItems, T* target, Validator* validator = 0)
		{
			static_cast<void>(static_cast<uint32_t>(*target));
			SCE_GNM_INLINE_ASSERT(sizeof(T) == sizeof(uint32_t));
			initialize(menuItemText, menuItems, static_cast<uint32_t*>(static_cast<void*>(target)), validator);
		}
		MenuCommandEnum(MenuItemText* menuItemText, uint32_t menuItems, uint32_t* target, Validator* validator = 0)
		{
			initialize(menuItemText, menuItems, target, validator);
		}
		virtual void adjust(int) const;
		virtual Menu* getMenu() { return &m_menu; }
		virtual void printValue(sce::DbgFont::Window *window);
	};

	class MenuCommandBool : public MenuCommand
	{
	protected:
		bool* m_target;
	public:
		MenuCommandBool(bool* target);
		virtual void adjust(int) const;
		virtual void printValue(sce::DbgFont::Window *window);
	};

	class MenuCommandInt : public MenuCommand
	{
	protected:
		int32_t* m_target;
		const char* m_format;
		int32_t m_min;
		int32_t m_max;
        int32_t m_step;
	public:
		enum WrapMode
		{
			kClamp,
			kWrap,
		};
		WrapMode m_wrapMode;
		MenuCommandInt(int32_t* target, int32_t min=0, int32_t max=0, const char *format = 0);
		virtual void adjust(int) const;
		virtual void printValue(sce::DbgFont::Window *window);
	};

	class MenuCommandUint : public MenuCommand
	{
	protected:
		uint32_t* m_target;
		const char* m_format;
		uint32_t m_min;
		uint32_t m_max;
        uint32_t m_step;
	public:
		enum WrapMode
		{
			kClamp,
			kWrap,
		};
		WrapMode m_wrapMode;
		MenuCommandUint(uint32_t* target, uint32_t min=0, uint32_t max=0, const char *format = 0);
		virtual void adjust(int) const;
		virtual void printValue(sce::DbgFont::Window *window);
	};

	class MenuCommandUint8 : public MenuCommand
	{
	protected:
		uint8_t* m_target;
		const char* m_format;
		uint8_t m_min;
		uint8_t m_max;
        uint8_t m_step;
	public:
		uint8_t m_reserved;
		enum WrapMode
		{
			kClamp,
			kWrap,
		};
		WrapMode m_wrapMode;
		MenuCommandUint8(uint8_t* target, uint8_t min=0, uint8_t max=0, const char *format = 0);
		virtual void adjust(int) const;
		virtual void printValue(sce::DbgFont::Window *window);
	};

	class MenuCommandUintPowerOfTwo : public MenuCommandUint
	{
	public:
		MenuCommandUintPowerOfTwo(uint32_t* target, uint32_t min=0, uint32_t max=0);
		virtual void printValue(sce::DbgFont::Window *window);
	};

	class MenuCommandFloat : public MenuCommand
	{
	protected:
		float* m_target;
		float m_min;
		float m_max;
		float m_step;
	public:
		uint32_t m_reserved;
		MenuCommandFloat(float* target, float min, float max, float step);
		virtual void adjust(int) const;
		virtual void input(MenuHost*);
		virtual void printValue(sce::DbgFont::Window *window);
	};

	class MenuCommandVector2 : public MenuCommand
	{
	protected:
		Vector2* m_target;
		Vector2 m_min;
		Vector2 m_max;
		Vector2 m_step;
	public:
		MenuCommandVector2(Vector2* target, Vector2 min, Vector2 max, Vector2 step);
		virtual void input(MenuHost*);
		virtual void adjust(int) const {}
		virtual void printValue(sce::DbgFont::Window *window);
	};

	class MenuCommandDouble : public MenuCommand
	{
	protected:
		double* m_target;
		double m_min;
		double m_max;
		double m_step;
	public:
		MenuCommandDouble(double* target, double min, double max, double step);
		virtual void adjust(int) const;
		virtual void input(MenuHost*);
		virtual void printValue(sce::DbgFont::Window *window);
	};

	class MenuCommandMatrix : public Framework::MenuCommand
	{
	protected:
		Matrix4 *m_target;
		float m_step;
		bool m_bUpdatedByInput;
		bool m_useRightStick;
	public:
		uint8_t m_reserved[2];
		MenuCommandMatrix(Matrix4 *target, bool useRightStick = false)
		: m_target(target)
		, m_step(2.0f/60.0f)
		, m_bUpdatedByInput(false)
		, m_useRightStick(useRightStick)
		{
		}
		void input(Framework::MenuHost *src);
		bool updated_by_input_since_last_call() 
		{ 
			if (m_bUpdatedByInput) {
				m_bUpdatedByInput = false;
				return true;
			}
			return false;
		}
		virtual void adjust(int) const {}
	};

}

#endif // _SCE_GNM_FRAMEWORK_SAMPLE_FRAMEWORK_MENU_H_ 
