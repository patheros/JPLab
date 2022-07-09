#pragma once

#include "rack.hpp"
#include "util.hpp"

using namespace rack;

#define ROOT_OFFSET 5.f/12.f
#define NoteEntryWidget_MIN (0.f - ROOT_OFFSET)
#define NoteEntryWidget_MAX (3.f - ROOT_OFFSET)

struct NoteEntryButton : OpaqueWidget {
	ParamWidget * parent;
	float value;
	void draw(const DrawArgs& args) override {

		Rect r = box.zeroPos();
		const float margin = mm2px(2.0);
		Rect rMargin = r.grow(Vec(margin, margin));

		nvgBeginPath(args.vg);
		nvgRect(args.vg, RECT_ARGS(rMargin));
		nvgFillColor(args.vg, nvgRGB(0x12, 0x12, 0x12));
		nvgFill(args.vg);

		nvgBeginPath(args.vg);
		nvgRect(args.vg, RECT_ARGS(r));
		float brightness = 0;
		if (parent) {
			brightness = 1 - std::abs(value - parent->getParamQuantity()->getValue()) * 12;
			if(brightness < 0) brightness = 0;
		}
		int c = 64 + 191 * brightness;
		nvgFillColor(args.vg, nvgRGB(c, c, c));
		nvgFill(args.vg);
	}
	void onButton(const widget::Widget::ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && (e.mods & RACK_MOD_MASK) == 0) {
			//Handle Left Button Press
			parent->getParamQuantity()->setValue(value);
		}
	}
};

struct NoteEntryWidget : LedDisplay {
	ParamWidget * parent;
	void init() {
		static const Vec QUANTIZER_DISPLAY_SIZE = Vec(2,2);
		static const Vec QUANTIZER_DISPLAY_OFFSET = mm2px(Vec(0, 15.931 - 2.242));

		this->box.size = mm2px(QUANTIZER_DISPLAY_SIZE * Vec(38.25, 55.88));

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

		static const Vec OCTAVE_OFFSET = mm2px(Vec(2*11.734, 0));

		// White notes
		for (int octave = 0; octave < 3; octave++){			
			static const std::vector<int> whiteNotes = {1, 3, 5, 6, 8, 10, 12};
			for (int note : whiteNotes) {
				NoteEntryButton* button = new NoteEntryButton();
				button->box.pos = QUANTIZER_DISPLAY_SIZE * (noteAbsPositions[note] - QUANTIZER_DISPLAY_OFFSET) - box.pos + OCTAVE_OFFSET * octave;
				button->box.size = QUANTIZER_DISPLAY_SIZE * noteSizes[note];
				button->value = note / 12.f + octave - ROOT_OFFSET;
				button->parent = this->parent;
				addChild(button);
			}
		}
		// Black notes
		for (int octave = 0; octave < 3; octave++){			
			static const std::vector<int> blackNotes = {2, 4, 7, 9, 11};
			for (int note : blackNotes) {
				NoteEntryButton* button = new NoteEntryButton();
				button->box.pos = QUANTIZER_DISPLAY_SIZE * (noteAbsPositions[note] - QUANTIZER_DISPLAY_OFFSET) - box.pos + OCTAVE_OFFSET * octave;
				button->box.size = QUANTIZER_DISPLAY_SIZE * noteSizes[note];
				button->value = note / 12.f + octave - ROOT_OFFSET;
				button->parent = this->parent;
				addChild(button);
			}
		}
	}
};

template <typename TBase = rack::app::ParamWidget>
struct NoteWidget : TBase {
	NoteWidget() {
	}
	void onButton(const widget::Widget::ButtonEvent& e) override {
		// Right click to open context menu
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT && (e.mods & RACK_MOD_MASK) == 0) {
			TBase::destroyTooltip();
			createContextMenu();
			e.consume(this);
		}else{
			TBase::onButton(e);
		}
	}
	void createContextMenu() {
		ui::Menu* menu = createMenu();
		NoteEntryWidget* noteSelector = createWidget<NoteEntryWidget>(mm2px(Vec(0.0, 0)));
		noteSelector->parent = this; 
		noteSelector->init();
		menu->addChild(noteSelector);
	}
};