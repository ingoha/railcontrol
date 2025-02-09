/*
RailControl - Model Railway Control Software

Copyright (c) 2017-2024 by Teddy / Dominik Mahrer - www.railcontrol.org

RailControl is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

RailControl is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RailControl; see the file LICENCE. If not see
<http://www.gnu.org/licenses/>.
*/

#include <sstream>

#include "DataModel/Accessory.h"
#include "Server/Web/HtmlTagAccessory.h"

using std::string;
using std::to_string;

namespace Server { namespace Web
{
	HtmlTagAccessory::HtmlTagAccessory(const DataModel::Accessory* accessory)
	:	HtmlTagLayoutItem(dynamic_cast<const DataModel::LayoutItem*>(accessory))
	{
		switch (accessory->GetAccessoryType())
		{
			case DataModel::AccessoryTypeStraight:
				image = "<polygon class=\"track\" points=\"15,0 21,0 21,36 15,36\"/>";
				break;

			case DataModel::AccessoryTypeTurn:
				image = "<polygon class=\"track\" points=\"0,21 0,15 21,36 15,36\"/>";
				break;

			case DataModel::AccessoryTypeDefault:
			case DataModel::AccessoryTypeOnPush:
			default:
				break;
		}
		image += "<polygon class=\"accessory\" points=\"10,10 26,10 26,26 10,26\" />";

		const DataModel::AccessoryState state = accessory->GetAccessoryState();

		string accessoryIdString = to_string(accessory->GetID());
		imageDiv.AddClass("accessory_item");
		imageDiv.AddClass(state == DataModel::AccessoryStateOn ? "accessory_on" : "accessory_off");
		switch (accessory->GetAccessoryType())
		{
			case DataModel::AccessoryTypeOnPush:
				imageDiv.AddAttribute("onmousedown", "return onMousePressAccessory(" + accessoryIdString + ");");
				imageDiv.AddAttribute("onmouseup", "return onMouseReleaseAccessory(" + accessoryIdString + ");");
				break;

			case DataModel::AccessoryTypeDefault:
			case DataModel::AccessoryTypeStraight:
			case DataModel::AccessoryTypeTurn:
			default:
				imageDiv.AddAttribute("onclick", "return onClickAccessory(" + accessoryIdString + ");");
				break;
		}

		const string& accessoryName = accessory->GetName();
		AddToolTip(accessoryName + " (addr=" + to_string(accessory->GetAddress()) + ")");
		AddContextMenuEntry(accessoryName);
		AddContextMenuEntry(Languages::TextReleaseAccessory, "fireRequestAndForget('/?cmd=accessoryrelease&accessory=" + accessoryIdString + "');");
		AddContextMenuEntry(Languages::TextEditAccessory, "loadPopup('/?cmd=accessoryedit&accessory=" + accessoryIdString + "');");
		AddContextMenuEntry(Languages::TextDeleteAccessory, "loadPopup('/?cmd=accessoryaskdelete&accessory=" + accessoryIdString + "');");
		FinishInit();
	}
}} // namespace Server::Web
