/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2017 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include "sample_framework_menu.h"
#include "framework.h"
using namespace sce;

void Framework::MenuCommand::print(FILE* file)
{
	fprintf(file, "{\"name\" : \"%s\", \"documentation\" : \"%s\", \"documented\" : %s", m_menuItemText.m_name, m_menuItemText.m_documentation, isDocumented() ? "true" : "false");
	Menu* menu = getMenu();
	if(menu != 0)
	{
		fprintf(file,", \"menu\" : ");
		menu->print(file);
	}
	fprintf(file, "}");
}

void Framework::Menu::print(FILE* file)
{
	fprintf(file, "{ \"menuItem\" : [");
	for(uint32_t i=0; i<m_menuItems; ++i)
	{
		m_menuItem[i].m_menuCommand->print(file);
		if(i != m_menuItems-1)
		{
			fprintf(file, ", ");
		}
	}
	fprintf(file, "] }");
}

Framework::Validator::~Validator()
{
}

void Framework::MenuCommandSetUint::set(uint32_t* target, uint32_t value, MenuItemText const* menuItemText, Validator* validator)
{
	m_target = target;
	m_value = value;
	m_menuItemText = *menuItemText;
	m_validator = validator;
}

bool Framework::MenuCommandSetUint::isEnabled() const
{
	if(m_validator == 0)
		return true;
	return m_validator->isValid(m_value);
}

void Framework::MenuCommandSetUint::adjust(int) const
{
	*m_target = m_value;
}

void Framework::MenuCommandSetUint::menuJustBeenPushed(Menu* menu, uint32_t index)
{
	if(*m_target == m_value)
	{
		menu->m_selectedMenuItemIndex = index;
	}
}

void Framework::MenuCommandEnum::initialize(MenuItemText const* menuItemText, uint32_t menuItems, uint32_t* target, Validator* validator)
{
	m_menuItemText = menuItemText;
	m_menuItems = menuItems;
	m_target = target;
	m_validator = validator;
	SCE_GNM_ASSERT(m_menuItems <= kMaximumMenuItems);
	for(uint32_t i=0; i<m_menuItems; ++i )
	{
		m_menuItem[i].m_menuItemText = m_menuItemText[i];
		m_menuItem[i].m_menuCommand = &m_menuCommand[i];
		m_menuCommand[i].set(m_target, i, m_menuItemText+i, validator);
	}
	m_menu.appendMenuItems(m_menuItems, &m_menuItem[0]);
	++m_menu.m_menuItemNameLength;
}

bool Framework::MenuCommand::matches(const MenuItem* menuItem) const
{
	return menuItem->m_menuCommand == this;
}

void Framework::MenuCommandEnum::printValue(DbgFont::Window* window)
{
	uint32_t target = *m_target;
	window->printf("%s", m_menuItemText[target].m_name);
}

void Framework::MenuCommandEnum::adjust(int delta) const
{
	do
	{
		*m_target = Modulo(*m_target + delta, m_menuItems);
	} while((m_validator != 0) && (!m_validator->isValid(*m_target)));
}

Framework::MenuCommandBool::MenuCommandBool(bool* target)
: m_target(target)
{
}
void Framework::MenuCommandBool::printValue(DbgFont::Window* window)
{
	window->printf("%s", *m_target ? "true" : "false");
}
void Framework::MenuCommandBool::adjust(int delta) const
{
	if(delta)
		*m_target ^= true;
}

Framework::MenuCommandInt::MenuCommandInt(int32_t* target, int32_t min, int32_t max, const char *format)
: m_target(target)
, m_format(format)
, m_min(min)
, m_max(max)
, m_step(1)
, m_wrapMode(kWrap)
{
}
void Framework::MenuCommandInt::printValue(DbgFont::Window* window)
{
	const char* format = m_format ? m_format : "%d";
	window->printf(format, *m_target);
}
void Framework::MenuCommandInt::adjust(int delta) const
{
	delta *= m_step;
	int32_t target = *m_target;
	target += delta;
	if(m_min < m_max)
	{
		switch(m_wrapMode)
		{
		case kClamp:
			target = std::max(m_min, target);
			target = std::min(m_max, target);
			break;
		case kWrap:
			if(target > m_max)
				target = m_min;
			if(target < m_min)
				target = m_max;
			break;
		}
	}
	*m_target = target;
}

Framework::MenuCommandUint::MenuCommandUint(uint32_t* target, uint32_t min, uint32_t max, const char* format)
: m_target(target)
, m_format(format)
, m_min(min)
, m_max(max)
, m_step(1)
, m_wrapMode(kWrap)
{
}
void Framework::MenuCommandUint::printValue(DbgFont::Window* window)
{
	const char* format = m_format ? m_format : "%u";
	window->printf(format, *m_target);
}
void Framework::MenuCommandUint::adjust(int delta) const
{
	delta *= m_step;
	int64_t target = *m_target;
	target += delta;
	if(m_min < m_max)
	{
		switch(m_wrapMode)
		{
		case kClamp:
			target = std::max((int64_t)m_min, target);
			target = std::min((int64_t)m_max, target);
			break;
		case kWrap:
			if(target > m_max)
				target = m_min;
			if(target < m_min)
				target = m_max;
			break;
		}
	}
	*m_target = (uint32_t)target;
}

Framework::MenuCommandUint8::MenuCommandUint8(uint8_t* target, uint8_t min, uint8_t max, const char* format)
: m_target(target)
, m_format(format)
, m_min(min)
, m_max(max)
, m_step(1)
, m_wrapMode(kWrap)
{
}
void Framework::MenuCommandUint8::printValue(DbgFont::Window* window)
{
	const char* format = m_format ? m_format : "%d";
	window->printf(format, *m_target);
}
void Framework::MenuCommandUint8::adjust(int delta) const
{
	delta *= m_step;
	int64_t target = *m_target;
	target += delta;
	if(m_min < m_max)
	{
		switch(m_wrapMode)
		{
		case kClamp:
			target = std::max((int64_t)m_min, target);
			target = std::min((int64_t)m_max, target);
			break;
		case kWrap:
			if(target > m_max)
				target = m_min;
			if(target < m_min)
				target = m_max;
			break;
		}
	}
	*m_target = (uint32_t)target;
}

Framework::MenuCommandUintPowerOfTwo::MenuCommandUintPowerOfTwo(uint32_t* target, uint32_t min, uint32_t max)
: MenuCommandUint(target, min, max)
{
}
void Framework::MenuCommandUintPowerOfTwo::printValue(DbgFont::Window* window)
{
	window->printf("%d", 1 << *m_target);
}

Framework::MenuCommandFloat::MenuCommandFloat(float* target, float min, float max, float step)
: m_target(target)
, m_min(min)
, m_max(max)
, m_step(step)
{
}
void Framework::MenuCommandFloat::printValue(DbgFont::Window* window)
{
    window->printf("%.2f", *m_target);
}
void Framework::MenuCommandFloat::adjust(int delta) const
{
	*m_target += delta * m_step;
	if(m_min < m_max)
	{
		*m_target = std::max(m_min, *m_target);
		*m_target = std::min(m_max, *m_target);
	}
}
void Framework::MenuCommandFloat::input(MenuHost *src)
{
	*m_target += (float)*src->m_actualSecondsPerFrame * m_step * src->m_controllerContext->getLeftStick().getX();
}

Framework::MenuCommandVector2::MenuCommandVector2(Vector2* target, Vector2 min, Vector2 max, Vector2 step)
: m_target(target)
, m_min(min)
, m_max(max)
, m_step(step)
{
}
void Framework::MenuCommandVector2::printValue(DbgFont::Window *window)
{
	const float x = m_target->getX();
	const float y = m_target->getY();
	window->printf("%.2f %.2f", x, y);
}
void Framework::MenuCommandVector2::input(MenuHost *src)
{
	*m_target += mulPerElem((float)*src->m_actualSecondsPerFrame * m_step, src->m_controllerContext->getLeftStick());
	*m_target = clampPerElem(*m_target, m_min, m_max);
}

Framework::MenuCommandDouble::MenuCommandDouble(double* target, double min, double max, double step)
: m_target(target)
, m_min(min)
, m_max(max)
, m_step(step)
{
}
void Framework::MenuCommandDouble::printValue(DbgFont::Window *window)
{
    window->printf("%.2f", *m_target);
}
void Framework::MenuCommandDouble::adjust(int delta) const
{
	*m_target += delta * m_step;
	if(m_min < m_max)
	{
		*m_target = std::max(m_min, *m_target);
		*m_target = std::min(m_max, *m_target);
	}
}
void Framework::MenuCommandDouble::input(MenuHost *src)
{
	MenuCommand::input(src);
	*m_target += m_step * src->m_controllerContext->getLeftStick().getX();
}

void Framework::MenuCommandMatrix::input(Framework::MenuHost *src)
{
	const float step = m_step;
	const float trans_x = step * src->m_controllerContext->getLeftStick().getX();
	const float trans_y = step * src->m_controllerContext->getLeftStick().getY();
	const float rot_x = m_useRightStick ?  src->m_controllerContext->getRightStick().getY() : 0.f;
	const float rot_y = m_useRightStick ? -src->m_controllerContext->getRightStick().getX() : 0.f;
	if (trans_x == 0.0f && trans_y == 0.0f && rot_x == 0.0f && rot_y == 0.0f)
		return;
	m_bUpdatedByInput = true;
	Matrix4 toWorld = *m_target;
	{
		toWorld.setCol3(
			toWorld.getCol3() 
			+ toWorld.getCol0() * trans_x
			+ toWorld.getCol2() * trans_y
			);
	}
	{
        Vector3 up = (src->m_cameraControlMode  == 1) ? Vector3(0,1,0) : toWorld.getCol1().getXYZ();
        Vector3 vector = toWorld.getCol0().getXYZ() * rot_x + up * rot_y;
        
		const Vector3 axis = normalize(vector);
		const float amount = length(vector);
		if(amount > 0.1f)
		{
			const float radians = amount * step;
			toWorld.setCol0(Matrix4::rotation(radians, axis) * toWorld.getCol0());
			toWorld.setCol1(Matrix4::rotation(radians, axis) * toWorld.getCol1());
			toWorld.setCol2(Matrix4::rotation(radians, axis) * toWorld.getCol2());
		}
	}
	*m_target = toWorld;
}

void Framework::Menu::justBeenPushed()
{
	for(uint32_t i=0; i<m_menuItems; ++i)
		m_menuItem[i].m_menuCommand->menuJustBeenPushed(this, i);
}

void Framework::Menu::removeAllMenuItems()
{
	removeMenuItems(0, m_menuItems);
}

Framework::Menu::Menu()
: m_menuItems(0)
, m_selectedMenuItemIndex(0)
, m_menuItemNameLength(0)
{
}

Framework::Menu::~Menu()
{
}

void Framework::Menu::adjustChildMenuPlacement(MenuStackEntry* menuStackEntry) const
{
	menuStackEntry->m_x += 1 + m_menuItemNameLength;
	menuStackEntry->m_y += 1 + m_selectedMenuItemIndex;
}

void Framework::MenuCommand::input(MenuHost *src)
{
	if(src->m_controllerContext->isButtonPressed(Input::BUTTON_CROSS))
	{
		Menu* menu = getMenu();
		if(menu != 0)
		{
			MenuStackEntry* parent = src->m_menuStack.top();

			MenuStackEntry child;
			child.m_menu = menu;
			child.m_x = parent->m_x;
			child.m_y = parent->m_y;
			
			parent->m_menu->adjustChildMenuPlacement(&child);
			src->m_menuStack.push(child);
			child.m_menu->justBeenPushed();
		}
		else
		{
			adjust(1);
		}
	}
	if(src->m_controllerContext->isButtonPressed(Input::BUTTON_RIGHT))
	{
		adjust(1);
	}
	if(src->m_controllerContext->isButtonPressed(Input::BUTTON_LEFT))
	{
		adjust(-1);
	}
}

void Framework::Menu::menuInput(MenuHost *src)
{
	if(src->m_controllerContext->isButtonPressed(Input::BUTTON_UP))
		m_selectedMenuItemIndex = Modulo((int32_t)m_selectedMenuItemIndex-1, m_menuItems);
	if(src->m_controllerContext->isButtonPressed(Input::BUTTON_DOWN))
		m_selectedMenuItemIndex = Modulo((int32_t)m_selectedMenuItemIndex+1, m_menuItems);
	if(src->m_controllerContext->isButtonPressed(Input::BUTTON_CIRCLE))
	{
		if(src->m_menuStack.m_count > 1)
			src->m_menuStack.pop();
	}

	MenuItem* selectedMenuItem = getSelectedMenuItem();
	for( uint32_t i=0; i<m_menuItems; ++i )
	{
		if(&m_menuItem[i] != selectedMenuItem) // don't send input to a menu item that isn't currently selected
			continue;
		if(m_menuItem[i].m_menuCommand == 0) // don't send input to a menu item that has no command object to accept the input
			continue;
		if(m_menuItem[i].m_menuCommand->isEnabled() == false) // don't send input to a menu item that is disabled
			continue;
		m_menuItem[i].m_menuCommand->input(src); // ok, send input to a menu item
	}
}

namespace 
{
	enum Status
	{
		kUnselected,
		kSelected,
		kUnselectedDisabled,
		kSelectedDisabled
	};
}

DbgFont::Cell Framework::Menu::s_boxColor = {' ', DbgFont::kLightGray, 15, DbgFont::kBlack, 7};
DbgFont::Cell Framework::Menu::s_color[4] = 
{
	{' ', DbgFont::kWhite, 15, DbgFont::kBlack, 7},
	{' ', DbgFont::kWhite, 15, DbgFont::kGray,  7},
	{' ', DbgFont::kBrown, 15, DbgFont::kBlack, 7},
	{' ', DbgFont::kBrown, 15, DbgFont::kGray,  7},
};

void Framework::Menu::menuDisplay(DbgFont::Window* dest, uint32_t x, uint32_t y)
{
	Status status = kUnselected;

	const uint32_t kMenuLeft = x;
	const uint32_t middle = kMenuLeft+m_menuItemNameLength+1;
	const uint32_t kMenuRight = middle + 48;

	DbgFont::Window left, right;

	left.initialize(dest, kMenuLeft, y, middle - kMenuLeft + 1, m_menuItems + 2);
	left.clear(s_color[kUnselected]);
	left.renderOutlineBox(s_boxColor);

	right.initialize(dest, middle, y, kMenuRight - middle + 1, m_menuItems + 2);
	right.clear(s_color[kUnselected]);
	right.renderOutlineBox(s_boxColor);

	left.initialize(&left, 1, 1, left.m_widthInCharacters - 2, left.m_heightInCharacters - 2);
	right.initialize(&right, 1, 1, right.m_widthInCharacters - 2, right.m_heightInCharacters - 2);

	for( uint32_t i=0; i<m_menuItems; ++i )
	{
		status = kUnselected;
		if((m_menuItem[i].m_menuCommand != 0) && (m_menuItem[i].m_menuCommand->isEnabled() == false))
			status = kUnselectedDisabled;
		if(&m_menuItem[i] == getSelectedMenuItem())
		{
			status = kSelected;
			if((m_menuItem[i].m_menuCommand != 0) && (m_menuItem[i].m_menuCommand->isEnabled() == false))
				status = kSelectedDisabled;
		}
		const DbgFont::Cursor cursor = {0, i};
		left.m_cursor = right.m_cursor = cursor;
		left.m_cell = right.m_cell = s_color[status];
		if(status != kUnselected)
		{
			left.setCells(0, left.m_cursor.m_y, left.m_widthInCharacters - 1, left.m_cursor.m_y, s_color[status]);
			right.setCells(0, right.m_cursor.m_y, right.m_widthInCharacters - 1, right.m_cursor.m_y, s_color[status]);
		}
		m_menuItem[i].m_menuCommand->print(&left, &right);
	}
	status = kUnselected;
}

Framework::MenuItem* Framework::Menu::getSelectedMenuItem()
{
	return m_menuItem + m_selectedMenuItemIndex;
}

void Framework::Menu::menuDisplayDocumentation(DbgFont::Window* dest)
{
	DbgFont::Cell cell;
	cell.m_character       = ' ';
	cell.m_foregroundColor = DbgFont::kWhite;
	cell.m_foregroundAlpha = 15;
	cell.m_backgroundColor = DbgFont::kBlack;
	cell.m_backgroundAlpha = 12;
	for( uint32_t i=0; i<m_menuItems; ++i )
	{
		if(&m_menuItem[i] == getSelectedMenuItem())
		{
			if(m_menuItem[i].m_menuItemText.m_documentation != NULL)
			{
				DbgFont::Window window;
				enum {kHeight = 10};
				window.initialize(dest, 0, dest->m_heightInCharacters - kHeight, dest->m_widthInCharacters, kHeight);
				window.renderOutlineBox(cell);
				window.initialize(&window, 1, 1, window.m_widthInCharacters - 2, window.m_heightInCharacters - 2);
				window.clear(cell);
				window.printf(m_menuItem[i].m_menuItemText.m_documentation);
			}
		}
	}
}

void Framework::MenuCommand::printName(DbgFont::Window* dest)
{
	dest->printf("%s", m_menuItemText.m_name);
}

void Framework::MenuCommand::printValue(DbgFont::Window* dest)
{
	dest->printf("%s", m_menuItemText.m_documentation);
}

void Framework::MenuCommand::print(DbgFont::Window *left, DbgFont::Window *right)
{
	printName(left);
	printValue(right);
}

void Framework::MenuCommandSetUint::printName(DbgFont::Window* dest)
{
	DbgFont::Cell cell = dest->m_cell;
	if(*m_target == m_value)
		cell.m_character = DbgFont::kDot;
	else
		cell.m_character = ' ';
	dest->putCell(cell);
	dest->printf("%s", m_menuItemText.m_name);
}

void Framework::Menu::appendMenuItems(                uint32_t count, const MenuItem* menuItem)
{
	insertMenuItems(m_menuItems, count, menuItem);
}

void Framework::Menu::recalculateMenuWidth()
{
	m_menuItemNameLength = 0;
	for(uint32_t i=0; i<m_menuItems; ++i)
	{
		m_menuItemNameLength = std::max((uint32_t)strlen(m_menuItem[i].m_menuItemText.m_name), m_menuItemNameLength);
	}
}

void Framework::Menu::insertMenuItems(uint32_t index, uint32_t count, const MenuItem* menuItem)
{
	SCE_GNM_ASSERT_MSG(m_menuItems + count < kMaximumMenuItems, "Menu can hold only %d items but you tried to insert %d starting at index %d.", kMaximumMenuItems, count, index);
	for(uint32_t i = m_menuItems + count - 1; i > index + count - 1; --i)
		m_menuItem[i] = m_menuItem[i - count];
	for(uint32_t i = 0; i < count; ++i)
	{
		const char* name = menuItem[i].m_menuItemText.m_name;
		const char* documentation = menuItem[i].m_menuItemText.m_documentation;
		SCE_GNM_UNUSED(name);
		SCE_GNM_UNUSED(documentation);
		SCE_GNM_ASSERT_MSG(name != 0, "MenuItem must have a name");
		SCE_GNM_ASSERT_MSG(name[0] == toupper(name[0]), "MenuItem '%s' name mustn't begin with lowercase", name);
		SCE_GNM_ASSERT_MSG(documentation != 0, "MenuItem '%s' must have documentation", name);
		SCE_GNM_ASSERT_MSG(documentation[0] == toupper(documentation[0]), "MenuItem '%s' documentation '%s' mustn't begin with lowercase", name, documentation);
		SCE_GNM_ASSERT_MSG(documentation[strlen(documentation)-1] != '.', "MenuItem '%s' documentation '%s' must not end with a period", name, documentation);

		menuItem[i].m_menuCommand->m_menuItemText = menuItem[i].m_menuItemText;
		m_menuItem[index + i] = menuItem[i];
	}
	m_menuItems += count;
	recalculateMenuWidth();
}

void Framework::Menu::removeMenuItems(uint32_t index, uint32_t count)
{
	SCE_GNM_ASSERT_MSG(index <= m_menuItems, "Can't remove menu items starting at index %d when there are only %d items in menu.", index, m_menuItems);
	const uint32_t menuItemsAfterIndex = m_menuItems - index;
	SCE_GNM_ASSERT_MSG(count <= menuItemsAfterIndex, "Can't remove %d menu items starting at index %d - there are only %d items after that index.", count, index, menuItemsAfterIndex);
	for(uint32_t i = index + count; i < m_menuItems; ++i)
	{
		m_menuItem[i - count] = m_menuItem[i];
	}
	m_menuItems -= count;
	recalculateMenuWidth();
}

