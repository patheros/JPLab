#pragma once

#include "rack.hpp"
#include "util.hpp"

using namespace rack;

#define ROOT_OFFSET 5.f/12.f
#define NoteEntryWidget_MIN (0.f - ROOT_OFFSET)
#define NoteEntryWidget_MAX (3.f - ROOT_OFFSET)
static const int NoteEntryWidget_OFF = -19;

struct NoteControler {
	virtual float getValue(){ return 0; }
	virtual void setValue(float value){}
};

struct NotePreviewer {
	virtual void setPreviewNote(float cv){};
};

struct NoteEntryButton : OpaqueWidget {
	NoteControler * parent;
	float value;
	float margin;
	void draw(const DrawArgs& args) override {

		Rect r = box.zeroPos();
		const float pMargin = mm2px(margin);
		Rect rMargin = r.grow(Vec(pMargin, pMargin));

		nvgBeginPath(args.vg);
		nvgRect(args.vg, RECT_ARGS(rMargin));
		nvgFillColor(args.vg, nvgRGB(0x12, 0x12, 0x12));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgRect(args.vg, RECT_ARGS(r));
		float brightness = 0;
		if (parent) {
			brightness = 1 - std::abs(value - parent->getValue()) * 12;
			if(brightness < 0) brightness = 0;
		}
		int c = 64 + 191 * brightness;
		nvgFillColor(args.vg, nvgRGB(c, c, c));
		nvgFill(args.vg);
	}
	void onButton(const widget::Widget::ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && (e.mods & RACK_MOD_MASK) == 0) {
			//Handle Left Button Press
			parent->setValue(value);
			e.consume(this);
		}
	}
};

struct NoteEntryWidget : LedDisplay, NoteControler {
	bool inMenu;
	void init() {
		Vec QUANTIZER_DISPLAY_SIZE;
		Vec QUANTIZER_DISPLAY_OFFSET;
		float margin;
		if(inMenu){
			margin = 2.0f;
			QUANTIZER_DISPLAY_SIZE = Vec(2,2);
			QUANTIZER_DISPLAY_OFFSET = mm2px(Vec(0, 15.931 - 2.742));
		}else{
			margin = 1.0f;
			QUANTIZER_DISPLAY_SIZE = Vec(1,1);
			QUANTIZER_DISPLAY_OFFSET = mm2px(Vec(0, 15.931 - 2.742));
		}

		box.size = mm2px(QUANTIZER_DISPLAY_SIZE * Vec(38.25, 55.88));

		std::vector<Vec> noteAbsPositions = {
			Vec(),
			mm2px(Vec(2.242, 60.54)),
			mm2px(Vec(2.242, 58.416)),
			mm2px(Vec(2.242, 52.043)),
			mm2px(Vec(2.242, 49.919)),
			mm2px(Vec(2.242, 45.67)),
			mm2px(Vec(2.242, 39.298)),
			mm2px(Vec(2.242, 37.173)),
			mm2px(Vec(2.242, 30.801)),
			mm2px(Vec(2.242, 28.677)),
			mm2px(Vec(2.242, 22.304)),
			mm2px(Vec(2.242, 20.18)),
			mm2px(Vec(2.242, 15.931)),
		};
		std::vector<Vec> noteSizes = {
			Vec(),
			mm2px(Vec(10.734, 5.644)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 7.769)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 5.644)),
			mm2px(Vec(10.734, 5.644)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 7.769)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 7.768)),
			mm2px(Vec(8.231, 3.52)),
			mm2px(Vec(10.734, 5.644)),
		};

		Vec OCTAVE_OFFSET = mm2px(Vec(QUANTIZER_DISPLAY_SIZE.x*11.734, 0));

		// White notes
		for (int octave = 0; octave < 3; octave++){			
			static const std::vector<int> whiteNotes = {1, 3, 5, 6, 8, 10, 12};
			for (int note : whiteNotes) {
				NoteEntryButton* button = new NoteEntryButton();
				button->box.pos = QUANTIZER_DISPLAY_SIZE * (noteAbsPositions[note] - QUANTIZER_DISPLAY_OFFSET) + OCTAVE_OFFSET * octave;
				button->box.size = QUANTIZER_DISPLAY_SIZE * noteSizes[note];
				// button->box.pos = noteAbsPositions[note] - box.pos;
				// button->box.size = noteSizes[note];
				button->margin = margin;
				button->value = note / 12.f + octave - ROOT_OFFSET;
				button->parent = this;
				addChild(button);
			}
		}
		// Black notes
		for (int octave = 0; octave < 3; octave++){			
			static const std::vector<int> blackNotes = {2, 4, 7, 9, 11};
			for (int note : blackNotes) {
				NoteEntryButton* button = new NoteEntryButton();
				button->box.pos = QUANTIZER_DISPLAY_SIZE * (noteAbsPositions[note] - QUANTIZER_DISPLAY_OFFSET) + OCTAVE_OFFSET * octave;
				button->box.size = QUANTIZER_DISPLAY_SIZE * noteSizes[note];
				// button->box.pos = noteAbsPositions[note] - box.pos;
				// button->box.size = noteSizes[note];
				button->margin = margin;
				button->value = note / 12.f + octave - ROOT_OFFSET;
				button->parent = this;
				addChild(button);
			}
		}
	}
};

struct NoteEntryWidgetMenu : NoteEntryWidget{
	ParamWidget * parent;
	NoteEntryWidgetMenu(){
		inMenu = true;
	}
	float getValue() override{
		return parent->getParamQuantity()->getValue();
	}
	void setValue(float value) override{
		parent->getParamQuantity()->setValue(value);
	}
};

struct NoteEntryWidgetPanel : NoteEntryWidget{
	float value;
	NotePreviewer * previewer;
	NoteEntryWidgetPanel(){
		inMenu = false;		
		value = NoteEntryWidget_OFF;
	}
	float getValue() override{
		return value;
	}
	void setValue(float newVal) override{
		if(newVal == value) newVal = NoteEntryWidget_OFF; //Clicking a 2nd time turns it off
		value = newVal;
		if(previewer){
			previewer->setPreviewNote(newVal);
		}
	}
};

template <typename TBase = rack::app::ParamWidget>
struct NoteWidget : TBase {
	NoteEntryWidgetPanel * panelSetter = NULL;
	NoteWidget() {
	}
	void onButton(const widget::Widget::ButtonEvent& e) override {
		
		if (e.action == GLFW_PRESS && (e.mods & RACK_MOD_MASK) == 0) {
			if(e.button == GLFW_MOUSE_BUTTON_LEFT){
				// Left Click pulls from panelSetter if present and selected
				if(panelSetter && panelSetter->value != NoteEntryWidget_OFF){
					this->getParamQuantity()->setValue(panelSetter->value);
					panelSetter->setValue(NoteEntryWidget_OFF);
					e.consume(this);
					return;
				}				
			}else if(e.button == GLFW_MOUSE_BUTTON_RIGHT){
				// Right click to open context menu
				TBase::destroyTooltip();
				createContextMenu();
				e.consume(this);
				return;
			}
		}
		TBase::onButton(e);
	}
	void createContextMenu() {
		ui::Menu* menu = createMenu();
		NoteEntryWidgetMenu* noteSelector = createWidget<NoteEntryWidgetMenu>(mm2px(Vec(0.0, 0)));
		noteSelector->parent = this; 
		noteSelector->init();
		menu->addChild(noteSelector);
	}
};