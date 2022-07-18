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
		DEBUG("NoteEntryButton onButton %i on btn:%i",e.action,e.button);
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
		float margin;
		if(inMenu){
			margin = 1.5f;
			
		}else{
			margin = 1.0f;
		}
		Vec QUANTIZER_DISPLAY_SIZE = Vec(margin,margin);
		Vec QUANTIZER_DISPLAY_OFFSET = mm2px(Vec(15.931 - 2.742,0));

		box.size = mm2px(QUANTIZER_DISPLAY_SIZE * Vec(158.88,15.25));

		std::vector<Vec> noteAbsPositions = {
			Vec(),
			mm2px(Vec(60.54, 2.242)),
			mm2px(Vec(58.416, 2.242)),
			mm2px(Vec(52.043, 2.242)),
			mm2px(Vec(49.919, 2.242)),
			mm2px(Vec(45.67, 2.242)),
			mm2px(Vec(39.298, 2.242)),
			mm2px(Vec(37.173, 2.242)),
			mm2px(Vec(30.801, 2.242)),
			mm2px(Vec(28.677, 2.242)),
			mm2px(Vec(22.304, 2.242)),
			mm2px(Vec(20.18, 2.242)),
			mm2px(Vec(15.931, 2.242)),
		};
		std::vector<Vec> noteSizes = {
			Vec(),
			mm2px(Vec(5.644, 10.734)),
			mm2px(Vec(3.52, 8.231)),
			mm2px(Vec(7.769, 10.734)),
			mm2px(Vec(3.52, 8.231)),
			mm2px(Vec(5.644, 10.734)),
			mm2px(Vec(5.644, 10.734)),
			mm2px(Vec(3.52, 8.231)),
			mm2px(Vec(7.769, 10.734)),
			mm2px(Vec(3.52, 8.231)),
			mm2px(Vec(7.768, 10.734)),
			mm2px(Vec(3.52, 8.231)),
			mm2px(Vec(5.644, 10.734)),
		};

		Vec OCTAVE_OFFSET = mm2px(Vec(QUANTIZER_DISPLAY_SIZE.x*51.04,0));

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

//Allows you to right click things, mainly other NoteWidgets, while the menu from the first NoteWidget is up.
struct PassThroughMenuOverlay : ui::MenuOverlay {
	//Based on MenuOverlay::onButton with the event consum commented out
	void onButton(const widget::Widget::ButtonEvent& e) override{
		DEBUG("Overlay onButton %i on btn:%i",e.action,e.button);
		Widget::onButton(e);
		if (e.action == GLFW_PRESS && !e.isConsumed()){
			widget::Widget::ActionEvent eAction;
			ui::MenuOverlay::onAction(eAction);
		}
	}
};

struct NoteWidget : Trimpot {
	NoteEntryWidgetPanel * panelSetter = NULL;
	bool displayed = true;
	NoteWidget() {
	}
	void onButton(const widget::Widget::ButtonEvent& e) override {	
		if(!displayed) return;
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
				Trimpot::destroyTooltip();
				createContextMenu();
				e.consume(this);
				return;
			}
		}
		Trimpot::onButton(e);
	}
	void onHover(const HoverEvent& e) override{
		if(displayed) Trimpot::onHover(e);
	}
	void onDragStart(const DragStartEvent& e) override{
		if(displayed) Trimpot::onDragStart(e);
	}
	//void onHoverScroll(const HoverScrollEvent& e) override;
	void createContextMenu() {
		//Pulled from helper.hpp : createMenu
		Menu* menu;
		{
			menu = new Menu;
			menu->box.pos = APP->scene->mousePos - mm2px(Vec(158.88f/2.f*1.5f,0)); //Add offset to move the menu into the middle of the click position

			ui::MenuOverlay* menuOverlay = new PassThroughMenuOverlay;
			menuOverlay->addChild(menu);

			APP->scene->addChild(menuOverlay);
		}
		NoteEntryWidgetMenu* noteSelector = createWidget<NoteEntryWidgetMenu>(mm2px(Vec(0, 0)));
		noteSelector->parent = this; 
		noteSelector->init();
		menu->addChild(noteSelector);
	}
	void draw(const DrawArgs& args) override {
		if(displayed) Trimpot::draw(args);
	}
};

#define NOTE_BLOCK_PARAM_COUNT 5

void configNoteBlock(Module * module, int paramIndex);

struct SubdivisionWidget : app::SvgSwitch
{
	int index;	

	SubdivisionWidget() {
		shadow->opacity = 0.0;
		addFrame(Svg::load(asset::plugin(pluginInstance,"res/subdiv/1.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance,"res/subdiv/2.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance,"res/subdiv/3.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance,"res/subdiv/4.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance,"res/subdiv/5.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance,"res/subdiv/6.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance,"res/subdiv/7.svg")));
	}

	void draw(const DrawArgs& args) override {
		
		// Background
		{
			const NVGcolor BG_COLORS[] = {nvgRGB(0xcc, 0xcc, 0xcc), nvgRGB(0xb8, 0xb8, 0xb8)};
			nvgBeginPath(args.vg);
			nvgRect(args.vg, RECT_ARGS(box.zeroPos()));
			nvgFillColor(args.vg, BG_COLORS[index % 2]);
			nvgFill(args.vg);
		}

		SvgSwitch::draw(args);
	}
};

struct NoteBlockWidget : widget::Widget{
	SubdivisionWidget* subdivWidget;
	NoteWidget* noteWidget[4];
	void init(Module * module, NoteEntryWidgetPanel * panelSetter, int index, int paramIndex){		
		subdivWidget = createParam<SubdivisionWidget>(mm2px(Vec(0,12)), module, paramIndex);
		subdivWidget->index = index;
		this->addChild(subdivWidget);

		const Vec POS[] = {mm2px(Vec(0,0)),mm2px(Vec(5,5)),mm2px(Vec(10,0)),mm2px(Vec(15,5))};
		
		for(int i = 0; i < 4; i++){
			noteWidget[i] = createParam<NoteWidget>(POS[i], module, paramIndex + 1 + i);
			noteWidget[i]->panelSetter = panelSetter;
			this->addChild(noteWidget[i]);
		}
	}

	int subdiv = -1;

	void step() override {
		int newSubdiv = static_cast<int>(subdivWidget->getParamQuantity()->getValue());
		if(subdiv != newSubdiv){
			subdiv = newSubdiv;
			updateKnobs();
		}
		Widget::step();
	}

	void updateKnobs(){
		const Vec GONE = Vec(-1,-1);
		const Vec POS[8][4] = {
			GONE,GONE,GONE,GONE, //Not Used
			mm2px(Vec(7.5,2.5)),GONE,GONE,GONE,
			mm2px(Vec(2.5,2.5)),GONE,mm2px(Vec(12.5,2.5)),GONE,

			mm2px(Vec(0,0)),mm2px(Vec(5,5)),mm2px(Vec(12.5,2.5)),GONE,
			mm2px(Vec(0,0)),GONE,mm2px(Vec(7.5,2.5)),mm2px(Vec(15,5)),
			mm2px(Vec(2.5,2.5)),GONE,mm2px(Vec(10,0)),mm2px(Vec(15,5)),

			mm2px(Vec(0,2.5)),mm2px(Vec(7.5,2.5)),mm2px(Vec(15,2.5)),GONE,

			mm2px(Vec(0,0)),mm2px(Vec(5,5)),mm2px(Vec(10,0)),mm2px(Vec(15,5)),
		};

		for(int i = 0; i < 4; i++){
			noteWidget[i]->box.pos = POS[subdiv][i];
			noteWidget[i]->displayed = POS[subdiv][i] != GONE;
		}
	}
};