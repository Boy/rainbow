/** 
 * @file llfloaterpostprocess.cpp
 * @brief LLFloaterPostProcess class definition
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterpostprocess.h"

#include "llsliderctrl.h"
#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"
#include "llviewerdisplay.h"
#include "llpostprocess.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llviewerwindow.h"


LLFloaterPostProcess* LLFloaterPostProcess::sPostProcess = NULL;


LLFloaterPostProcess::LLFloaterPostProcess() : LLFloater(std::string("Post-Process Floater"))
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_post_process.xml");

	/// Color Filter Callbacks
	childSetCommitCallback("ColorFilterToggle", &LLFloaterPostProcess::onBoolToggle, (char*)"enable_color_filter");
	//childSetCommitCallback("ColorFilterGamma", &LLFloaterPostProcess::onFloatControlMoved, &(gPostProcess->tweaks.gamma()));
	childSetCommitCallback("ColorFilterBrightness", &LLFloaterPostProcess::onFloatControlMoved, (char*)"brightness");
	childSetCommitCallback("ColorFilterSaturation", &LLFloaterPostProcess::onFloatControlMoved, (char*)"saturation");
	childSetCommitCallback("ColorFilterContrast", &LLFloaterPostProcess::onFloatControlMoved, (char*)"contrast");

	childSetCommitCallback("ColorFilterBaseR", &LLFloaterPostProcess::onColorControlRMoved, (char*)"contrast_base");
	childSetCommitCallback("ColorFilterBaseG", &LLFloaterPostProcess::onColorControlGMoved, (char*)"contrast_base");
	childSetCommitCallback("ColorFilterBaseB", &LLFloaterPostProcess::onColorControlBMoved, (char*)"contrast_base");
	childSetCommitCallback("ColorFilterBaseI", &LLFloaterPostProcess::onColorControlIMoved, (char*)"contrast_base");

	/// Night Vision Callbacks
	childSetCommitCallback("NightVisionToggle", &LLFloaterPostProcess::onBoolToggle, (char*)"enable_night_vision");
	childSetCommitCallback("NightVisionBrightMult", &LLFloaterPostProcess::onFloatControlMoved, (char*)"brightness_multiplier");
	childSetCommitCallback("NightVisionNoiseSize", &LLFloaterPostProcess::onFloatControlMoved, (char*)"noise_size");
	childSetCommitCallback("NightVisionNoiseStrength", &LLFloaterPostProcess::onFloatControlMoved, (char*)"noise_strength");

	/// Bloom Callbacks
	childSetCommitCallback("BloomToggle", &LLFloaterPostProcess::onBoolToggle, (char*)"enable_bloom");
	childSetCommitCallback("BloomExtract", &LLFloaterPostProcess::onFloatControlMoved, (char*)"extract_low");
	childSetCommitCallback("BloomSize", &LLFloaterPostProcess::onFloatControlMoved, (char*)"bloom_width");
	childSetCommitCallback("BloomStrength", &LLFloaterPostProcess::onFloatControlMoved, (char*)"bloom_strength");

	// Effect loading and saving.
	LLComboBox* comboBox = getChild<LLComboBox>("PPEffectsCombo");
	childSetAction("PPLoadEffect", &LLFloaterPostProcess::onLoadEffect, comboBox);
	comboBox->setCommitCallback(onChangeEffectName);

	LLLineEditor* editBox = getChild<LLLineEditor>("PPEffectNameEditor");
	childSetAction("PPSaveEffect", &LLFloaterPostProcess::onSaveEffect, editBox);

	syncMenu();
	
}

LLFloaterPostProcess::~LLFloaterPostProcess()
{


}

LLFloaterPostProcess* LLFloaterPostProcess::instance()
{
	// if we don't have our singleton instance, create it
	if (!sPostProcess)
	{
		sPostProcess = new LLFloaterPostProcess();
		sPostProcess->open();
		sPostProcess->setFocus(TRUE);
	}
	return sPostProcess;
}

// Bool Toggle
void LLFloaterPostProcess::onBoolToggle(LLUICtrl* ctrl, void* userData)
{
	char const * boolVariableName = (char const *)userData;
	
	// check the bool
	LLCheckBoxCtrl* cbCtrl = static_cast<LLCheckBoxCtrl*>(ctrl);
	gPostProcess->tweaks[boolVariableName] = cbCtrl->getValue();
}

// Float Moved
void LLFloaterPostProcess::onFloatControlMoved(LLUICtrl* ctrl, void* userData)
{
	char const * floatVariableName = (char const *)userData;
	LLSliderCtrl* sldrCtrl = static_cast<LLSliderCtrl*>(ctrl);
	gPostProcess->tweaks[floatVariableName] = sldrCtrl->getValue();
}

// Color Moved
void LLFloaterPostProcess::onColorControlRMoved(LLUICtrl* ctrl, void* userData)
{
	char const * floatVariableName = (char const *)userData;
	LLSliderCtrl* sldrCtrl = static_cast<LLSliderCtrl*>(ctrl);
	gPostProcess->tweaks[floatVariableName][0] = sldrCtrl->getValue();
}

// Color Moved
void LLFloaterPostProcess::onColorControlGMoved(LLUICtrl* ctrl, void* userData)
{
	char const * floatVariableName = (char const *)userData;
	LLSliderCtrl* sldrCtrl = static_cast<LLSliderCtrl*>(ctrl);
	gPostProcess->tweaks[floatVariableName][1] = sldrCtrl->getValue();
}

// Color Moved
void LLFloaterPostProcess::onColorControlBMoved(LLUICtrl* ctrl, void* userData)
{
	char const * floatVariableName = (char const *)userData;
	LLSliderCtrl* sldrCtrl = static_cast<LLSliderCtrl*>(ctrl);
	gPostProcess->tweaks[floatVariableName][2] = sldrCtrl->getValue();
}

// Color Moved
void LLFloaterPostProcess::onColorControlIMoved(LLUICtrl* ctrl, void* userData)
{
	char const * floatVariableName = (char const *)userData;
	LLSliderCtrl* sldrCtrl = static_cast<LLSliderCtrl*>(ctrl);
	gPostProcess->tweaks[floatVariableName][3] = sldrCtrl->getValue();
}

void LLFloaterPostProcess::onLoadEffect(void* userData)
{
	LLComboBox* comboBox = static_cast<LLComboBox*>(userData);

	LLSD::String effectName(comboBox->getSelectedValue().asString());

	gPostProcess->setSelectedEffect(effectName);

	sPostProcess->syncMenu();
}

void LLFloaterPostProcess::onSaveEffect(void* userData)
{
	LLLineEditor* editBox = static_cast<LLLineEditor*>(userData);

	LLSD::String effectName(editBox->getValue().asString());

	if (gPostProcess->mAllEffects.has(effectName))
	{
		gViewerWindow->alertXml("PPSaveEffectAlert", &LLFloaterPostProcess::saveAlertCallback, userData);
	}
	else
	{
		gPostProcess->saveEffect(effectName);
		sPostProcess->syncMenu();
	}
}

void LLFloaterPostProcess::onChangeEffectName(LLUICtrl* ctrl, void * userData)
{
	// get the combo box and name
	LLComboBox * comboBox = static_cast<LLComboBox*>(ctrl);
	LLLineEditor* editBox = sPostProcess->getChild<LLLineEditor>("PPEffectNameEditor");

	// set the parameter's new name
	editBox->setValue(comboBox->getSelectedValue());
}

void LLFloaterPostProcess::saveAlertCallback(S32 option, void* userData)
{
	LLLineEditor* editBox = static_cast<LLLineEditor*>(userData);

	// if they choose save, do it.  Otherwise, don't do anything
	if (option == 0)
	{
		LLSD::String effectName(editBox->getValue().asString());

		gPostProcess->saveEffect(effectName);

		sPostProcess->syncMenu();
	}

}

void LLFloaterPostProcess::show()
{
	// get the instance, make sure the values are synced
	// and open the menu
	LLFloaterPostProcess* postProcess = instance();
	postProcess->syncMenu();
	postProcess->open();
}

// virtual
void LLFloaterPostProcess::onClose(bool app_quitting)
{
	// just set visibility to false, don't get fancy yet
	if (sPostProcess)
	{
		sPostProcess->setVisible(FALSE);
	}
}

void LLFloaterPostProcess::syncMenu()
{
	// add the combo boxe contents
	LLComboBox* comboBox = getChild<LLComboBox>("PPEffectsCombo");

	comboBox->removeall();

	LLSD::map_const_iterator currEffect;
	for(currEffect = gPostProcess->mAllEffects.beginMap();
		currEffect != gPostProcess->mAllEffects.endMap();
		++currEffect) 
	{
		comboBox->add(currEffect->first);
	}

	// set the current effect as selected.
	comboBox->selectByValue(gPostProcess->getSelectedEffect());

	/// Sync Color Filter Menu
	childSetValue("ColorFilterToggle", gPostProcess->tweaks.useColorFilter());
	//childSetValue("ColorFilterGamma", gPostProcess->tweaks.gamma());
	childSetValue("ColorFilterBrightness", gPostProcess->tweaks.brightness());
	childSetValue("ColorFilterSaturation", gPostProcess->tweaks.saturation());
	childSetValue("ColorFilterContrast", gPostProcess->tweaks.contrast());
	childSetValue("ColorFilterBaseR", gPostProcess->tweaks.contrastBaseR());
	childSetValue("ColorFilterBaseG", gPostProcess->tweaks.contrastBaseG());
	childSetValue("ColorFilterBaseB", gPostProcess->tweaks.contrastBaseB());
	childSetValue("ColorFilterBaseI", gPostProcess->tweaks.contrastBaseIntensity());
	
	/// Sync Night Vision Menu
	childSetValue("NightVisionToggle", gPostProcess->tweaks.useNightVisionShader());
	childSetValue("NightVisionBrightMult", gPostProcess->tweaks.brightMult());
	childSetValue("NightVisionNoiseSize", gPostProcess->tweaks.noiseSize());
	childSetValue("NightVisionNoiseStrength", gPostProcess->tweaks.noiseStrength());

	/// Sync Bloom Menu
	childSetValue("BloomToggle", LLSD(gPostProcess->tweaks.useBloomShader()));
	childSetValue("BloomExtract", gPostProcess->tweaks.extractLow());
	childSetValue("BloomSize", gPostProcess->tweaks.bloomWidth());
	childSetValue("BloomStrength", gPostProcess->tweaks.bloomStrength());
}
