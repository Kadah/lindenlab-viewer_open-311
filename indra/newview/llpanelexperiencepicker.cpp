/** 
* @file llpanelexperiencepicker.cpp
* @brief Implementation of llpanelexperiencepicker
* @author dolphin@lindenlab.com
*
* $LicenseInfo:firstyear=2014&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2014, Linden Research, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"

#include "llpanelexperiencepicker.h"


#include "lllineeditor.h"
#include "llfloaterreg.h"
#include "llscrolllistctrl.h"
#include "llviewerregion.h"
#include "llagent.h"
#include "llexperiencecache.h"
#include "llslurl.h"
#include "llavatarnamecache.h"
#include "llcombobox.h"
#include "llviewercontrol.h"
#include "llfloater.h"

#define BTN_FIND		"find"
#define BTN_OK			"ok_btn"
#define BTN_CANCEL		"cancel_btn"
#define BTN_PROFILE		"profile_btn"
#define BTN_LEFT		"left_btn"
#define BTN_RIGHT		"right_btn"
#define TEXT_EDIT		"edit"
#define TEXT_MATURITY	"maturity"
#define LIST_RESULTS	"search_results"
#define PANEL_SEARCH	"search_panel"

const static std::string columnSpace = " ";

static LLPanelInjector<LLPanelExperiencePicker> t_panel_status("llpanelexperiencepicker");

class LLExperienceSearchResponder : public LLHTTPClient::Responder
{
public:
	LLUUID mQueryID;
	LLHandle<LLPanelExperiencePicker> mParent;

	LLExperienceSearchResponder(const LLUUID& id, const LLHandle<LLPanelExperiencePicker>& parent) : mQueryID(id), mParent(parent) { }

	void completed(U32 status, const std::string& reason, const LLSD& content)
	{
		if(mParent.isDead())
			return;
		if (isGoodStatus(status))
		{
			LLPanelExperiencePicker* panel =mParent.get();
			if (panel)
			{
				panel->processResponse(mQueryID, content);
			}
		}
		else
		{
			LLPanelExperiencePicker* panel =mParent.get();
			if (panel)
			{
				panel->processResponse(mQueryID, LLSD());
			}
			LL_WARNS() << "experience picker failed [status:" << status << "]: " << content << LL_ENDL;

		}
	}
};

LLPanelExperiencePicker::LLPanelExperiencePicker()
	:LLPanel()
{
	buildFromFile("panel_experience_search.xml");
	setDefaultFilters();
}

LLPanelExperiencePicker::~LLPanelExperiencePicker()
{
}

BOOL LLPanelExperiencePicker::postBuild()
{
	getChild<LLLineEditor>(TEXT_EDIT)->setKeystrokeCallback( boost::bind(&LLPanelExperiencePicker::editKeystroke, this, _1, _2),NULL);

	childSetAction(BTN_FIND, boost::bind(&LLPanelExperiencePicker::onBtnFind, this));
	getChildView(BTN_FIND)->setEnabled(FALSE);

	LLScrollListCtrl* searchresults = getChild<LLScrollListCtrl>(LIST_RESULTS);
	searchresults->setDoubleClickCallback( boost::bind(&LLPanelExperiencePicker::onBtnSelect, this));
	searchresults->setCommitCallback(boost::bind(&LLPanelExperiencePicker::onList, this));
	getChildView(LIST_RESULTS)->setEnabled(FALSE);
	getChild<LLScrollListCtrl>(LIST_RESULTS)->setCommentText(getString("no_results"));

	childSetAction(BTN_OK, boost::bind(&LLPanelExperiencePicker::onBtnSelect, this));
	getChildView(BTN_OK)->setEnabled(FALSE);
	childSetAction(BTN_CANCEL, boost::bind(&LLPanelExperiencePicker::onBtnClose, this));
	childSetAction(BTN_PROFILE, boost::bind(&LLPanelExperiencePicker::onBtnProfile, this));
	getChildView(BTN_PROFILE)->setEnabled(FALSE);

	getChild<LLComboBox>(TEXT_MATURITY)->setCurrentByIndex(2);
	getChild<LLComboBox>(TEXT_MATURITY)->setCommitCallback(boost::bind(&LLPanelExperiencePicker::onMaturity, this));
	getChild<LLUICtrl>(TEXT_EDIT)->setFocus(TRUE);

	childSetAction(BTN_LEFT, boost::bind(&LLPanelExperiencePicker::onPage, this, -1));
	childSetAction(BTN_RIGHT, boost::bind(&LLPanelExperiencePicker::onPage, this, 1));

	LLPanel* search_panel = getChild<LLPanel>(PANEL_SEARCH);
	if (search_panel)
	{
		// Start searching when Return is pressed in the line editor.
		search_panel->setDefaultBtn(BTN_FIND);
	}
	return TRUE;
}

void LLPanelExperiencePicker::editKeystroke( class LLLineEditor* caller, void* user_data )
{
	getChildView(BTN_FIND)->setEnabled(caller->getText().size() > 0);
}

void LLPanelExperiencePicker::onBtnFind()
{
	mCurrentPage=1;
	find();
}

void LLPanelExperiencePicker::onList()
{
	bool enabled = isSelectButtonEnabled();
	getChildView(BTN_OK)->setEnabled(enabled);

	enabled = enabled && getChild<LLScrollListCtrl>(LIST_RESULTS)->getNumSelected() == 1;
	getChildView(BTN_PROFILE)->setEnabled(enabled);
}

void LLPanelExperiencePicker::find()
{
	std::string text = getChild<LLUICtrl>(TEXT_EDIT)->getValue().asString();
	mQueryID.generate();
	std::ostringstream url;

	LLViewerRegion* region = gAgent.getRegion();
	std::string cap = region->getCapability("FindExperienceByName");
	if (!cap.empty())
	{
		url << cap << "?page=" << mCurrentPage << "&page_size=30&query=" << LLURI::escape(text);
		LLHTTPClient::get(url.str(), new LLExperienceSearchResponder(mQueryID, getDerivedHandle<LLPanelExperiencePicker>()));

	}
	getChild<LLScrollListCtrl>(LIST_RESULTS)->deleteAllItems();
	getChild<LLScrollListCtrl>(LIST_RESULTS)->setCommentText(getString("searching"));
	
	getChildView(BTN_OK)->setEnabled(FALSE);
	getChildView(BTN_PROFILE)->setEnabled(FALSE);

	getChildView(BTN_RIGHT)->setEnabled(FALSE);
	getChildView(BTN_LEFT)->setEnabled(FALSE);
}


bool LLPanelExperiencePicker::isSelectButtonEnabled()
{
	LLScrollListCtrl* list=getChild<LLScrollListCtrl>(LIST_RESULTS);
	return list->getFirstSelectedIndex() >=0;
}

void LLPanelExperiencePicker::getSelectedExperienceIds( const LLScrollListCtrl* results, uuid_vec_t &experience_ids )
{
	std::vector<LLScrollListItem*> items = results->getAllSelected();
	for(std::vector<LLScrollListItem*>::iterator it = items.begin(); it != items.end(); ++it)
	{
		LLScrollListItem* item = *it;
		if (item->getUUID().notNull())
		{
			experience_ids.push_back(item->getUUID());
		}
	}
}

void LLPanelExperiencePicker::setAllowMultiple( bool allow_multiple )
{
	getChild<LLScrollListCtrl>(LIST_RESULTS)->setAllowMultipleSelection(allow_multiple);
}


void name_callback(const LLHandle<LLPanelExperiencePicker>& floater, const LLUUID& experience_id, const LLUUID& agent_id, const LLAvatarName& av_name)
{
	if(floater.isDead())
		return;
	LLPanelExperiencePicker* picker = floater.get();
	LLScrollListCtrl* search_results = picker->getChild<LLScrollListCtrl>(LIST_RESULTS);

	LLScrollListItem* item = search_results->getItem(experience_id);
	if(!item)
		return;

	item->getColumn(2)->setValue(columnSpace+av_name.getDisplayName());

}

void LLPanelExperiencePicker::processResponse( const LLUUID& query_id, const LLSD& content )
{
	if(query_id != mQueryID)
	{
		return;
	}

	mResponse = content;

	const LLSD& experiences=mResponse["experience_keys"];
	LLSD::array_const_iterator it = experiences.beginArray();
	for ( ; it != experiences.endArray(); ++it)
	{
		LLExperienceCache::insert(*it);
	}

	getChildView(BTN_RIGHT)->setEnabled(content.has("next_page_url"));
	getChildView(BTN_LEFT)->setEnabled(content.has("previous_page_url"));

	filterContent();

}

void LLPanelExperiencePicker::onBtnSelect()
{
	if(!isSelectButtonEnabled())
	{
		return;
	}

	if(mSelectionCallback)
	{
		const LLScrollListCtrl* results = getChild<LLScrollListCtrl>(LIST_RESULTS);
		uuid_vec_t experience_ids;
		
		getSelectedExperienceIds(results, experience_ids);
		mSelectionCallback(experience_ids);
		getChild<LLScrollListCtrl>(LIST_RESULTS)->deselectAllItems(TRUE);
		if(mCloseOnSelect)
		{
			mCloseOnSelect = FALSE;
			onBtnClose();
		}
	}
	else 
	{
		onBtnProfile();
	}
}

void LLPanelExperiencePicker::onBtnClose()
{
	LLFloater* floater = getParentByType<LLFloater>();
	if (floater)
	{
		floater->closeFloater();
	}
}

void LLPanelExperiencePicker::onBtnProfile()
{
	LLScrollListItem* item = getChild<LLScrollListCtrl>(LIST_RESULTS)->getFirstSelected();
	if(item)
	{
		LLFloaterReg::showInstance("experience_profile", item->getUUID(), true);
	}
}

std::string LLPanelExperiencePicker::getMaturityString(int maturity)
{
	if(maturity <= SIM_ACCESS_PG)
	{
		return getString("maturity_icon_general");
	}
	else if(maturity <= SIM_ACCESS_MATURE)
	{
		return getString("maturity_icon_moderate");
	}
	return getString("maturity_icon_adult");
}

void LLPanelExperiencePicker::filterContent()
{
	LLScrollListCtrl* search_results = getChild<LLScrollListCtrl>(LIST_RESULTS);

	const LLSD& experiences=mResponse["experience_keys"];

	search_results->deleteAllItems();

	LLSD item;
	LLSD::array_const_iterator it = experiences.beginArray();
	for ( ; it != experiences.endArray(); ++it)
	{
		const LLSD& experience = *it;

		if(isExperienceHidden(experience))
			continue;

		item["id"]=experience[LLExperienceCache::EXPERIENCE_ID];
		LLSD& columns = item["columns"];
		columns[0]["column"] = "maturity";
		columns[0]["value"] = getMaturityString(experience[LLExperienceCache::MATURITY].asInteger());
		columns[0]["type"]="icon";
		columns[0]["halign"]="right";
		columns[1]["column"] = "experience_name";
		columns[1]["value"] = columnSpace+experience[LLExperienceCache::NAME].asString();
		columns[2]["column"] = "owner";
		columns[2]["value"] = columnSpace+getString("loading");
		search_results->addElement(item);
		LLAvatarNameCache::get(experience[LLExperienceCache::AGENT_ID], boost::bind(name_callback, getDerivedHandle<LLPanelExperiencePicker>(), experience[LLExperienceCache::EXPERIENCE_ID], _1, _2));
	}
	
	if (search_results->isEmpty())
	{
		LLStringUtil::format_map_t map;
		std::string search_text = getChild<LLUICtrl>(TEXT_EDIT)->getValue().asString();
		map["[TEXT]"] = search_text;
		if (search_text.empty())
		{
			getChild<LLScrollListCtrl>(LIST_RESULTS)->setCommentText(getString("no_results"));
		}
		else
		{
			getChild<LLScrollListCtrl>(LIST_RESULTS)->setCommentText(getString("not_found", map));
		}
		search_results->setEnabled(false);
		getChildView(BTN_OK)->setEnabled(false);
		getChildView(BTN_PROFILE)->setEnabled(false);
	}
	else
	{
		getChildView(BTN_OK)->setEnabled(true);
		search_results->setEnabled(true);
		search_results->sortByColumnIndex(1, TRUE);
		std::string text = getChild<LLUICtrl>(TEXT_EDIT)->getValue().asString();
		if (!search_results->selectItemByLabel(text, TRUE, 1))
		{
			search_results->selectFirstItem();
		}			
		onList();
		search_results->setFocus(TRUE);
	}
}

void LLPanelExperiencePicker::onMaturity()
{
	if(mResponse.has("experience_keys") && mResponse["experience_keys"].beginArray() != mResponse["experience_keys"].endArray())
	{
		filterContent();
	}
}

bool LLPanelExperiencePicker::isExperienceHidden( const LLSD& experience) const
{
	bool hide=false;
	filter_list::const_iterator it = mFilters.begin();
	for(/**/;it != mFilters.end(); ++it)
	{
		if((*it)(experience)){
			return true;
		}
	}

	return hide;
}

bool LLPanelExperiencePicker::FilterOverRating( const LLSD& experience )
{
	int maturity = getChild<LLComboBox>(TEXT_MATURITY)->getSelectedValue().asInteger();
	return experience[LLExperienceCache::MATURITY].asInteger() > maturity;
}

bool LLPanelExperiencePicker::FilterWithProperty( const LLSD& experience, S32 prop)
{
	return (experience[LLExperienceCache::PROPERTIES].asInteger() & prop) != 0;
}

bool LLPanelExperiencePicker::FilterWithoutProperty( const LLSD& experience, S32 prop )
{
	return (experience[LLExperienceCache::PROPERTIES].asInteger() & prop) == 0;
}

void LLPanelExperiencePicker::setDefaultFilters()
{
	mFilters.clear();
	addFilter(boost::bind(&LLPanelExperiencePicker::FilterOverRating, this, _1));
}

bool LLPanelExperiencePicker::FilterMatching( const LLSD& experience, const LLUUID& id )
{
	if(experience.isUUID())
	{
		return experience.asUUID() == id;
	}
	return experience[LLExperienceCache::EXPERIENCE_ID].asUUID() == id;
}

void LLPanelExperiencePicker::onPage( S32 direction )
{
	mCurrentPage += direction;
	if(mCurrentPage < 1)
	{
		mCurrentPage = 1;
	}
	find();
}
