#pragma once

#include "rack.hpp"
#include "util.hpp"

using namespace rack;

void svgDraw_colorOverride(NVGcontext* vg, NSVGimage* svg);

struct ColoredSvgWidget : widget ::SvgWidget
{
	NVGcolor color;
	void draw(const DrawArgs& args) override{
		nvgFillColor(args.vg, color);
		svgDraw_colorOverride(args.vg, svg->handle);
	}
};

#define ROOT_OFFSET 7.f/12.f
#define NoteEntryWidget_MIN (0.f - ROOT_OFFSET)
#define NoteEntryWidget_MAX (3.f - ROOT_OFFSET - 1.f/12.f)
static const int NoteEntryWidget_OFF = -19;

enum NoteExtra{
	NE_NONE,
	NE_MUTE,
	NE_TIE,
};

struct NoteControler {
	virtual float getValue(){ return 0; }
	virtual void setValue(float value){}

	virtual NoteExtra getExtra() { return NE_NONE; }
	virtual bool setExtra(NoteExtra extra) { return true; }
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

struct NoteExtraButton : SvgWidget {
	NoteControler * parent;
	NoteExtra effect;
	std::shared_ptr<window::Svg> on;
	std::shared_ptr<window::Svg> off;
	void draw(const DrawArgs& args) override {
		setSvg(parent->getExtra() == effect ? on : off);
		SvgWidget::draw(args);
	}
	void onButton(const widget::Widget::ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && (e.mods & RACK_MOD_MASK) == 0) {
			parent->setExtra(effect);
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
		Vec QUANTIZER_DISPLAY_OFFSET = mm2px(Vec(-5 + 15.931 - 2.742,0));

		box.size = mm2px(QUANTIZER_DISPLAY_SIZE * Vec(163.88,15.25));

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

		//Extra Buttons
		{
			NoteExtraButton* mute = new NoteExtraButton();
			mute->box.pos = QUANTIZER_DISPLAY_SIZE * mm2px(Vec(1.5, 1.5));
			mute->box.size = QUANTIZER_DISPLAY_SIZE * mm2px(Vec(4, 4)); //Note this doesn't scale the SVG, just scales the clickable box
			mute->effect = NE_MUTE;
			mute->on = Svg::load(asset::plugin(pluginInstance,"res/mute_on.svg"));
			mute->off = Svg::load(asset::plugin(pluginInstance,"res/mute_off.svg"));
			mute->parent = this;
			addChild(mute);

			NoteExtraButton* tie = new NoteExtraButton();
			tie->box.pos = QUANTIZER_DISPLAY_SIZE * mm2px(Vec(1.5, 8));
			tie->box.size = mute->box.size;
			tie->effect = NE_TIE;
			tie->on = Svg::load(asset::plugin(pluginInstance,"res/tie_on.svg"));
			tie->off = Svg::load(asset::plugin(pluginInstance,"res/tie_off.svg"));
			tie->parent = this;
			addChild(tie);
		}

		// White notes
		for (int octave = 0; octave < 3; octave++){			
			static const std::vector<int> whiteNotes = {1, 3, 5, 6, 8, 10, 12};
			for (int note : whiteNotes) {
				NoteEntryButton* button = new NoteEntryButton();
				button->box.pos = QUANTIZER_DISPLAY_SIZE * (noteAbsPositions[note] - QUANTIZER_DISPLAY_OFFSET) + OCTAVE_OFFSET * (2-octave);
				button->box.size = QUANTIZER_DISPLAY_SIZE * noteSizes[note];
				// button->box.pos = noteAbsPositions[note] - box.pos;
				// button->box.size = noteSizes[note];
				button->margin = margin;
				button->value = 3 - (note / 12.f + octave) - ROOT_OFFSET;
				button->parent = this;
				addChild(button);
			}
		}
		// Black notes
		for (int octave = 0; octave < 3; octave++){			
			static const std::vector<int> blackNotes = {2, 4, 7, 9, 11};
			for (int note : blackNotes) {
				NoteEntryButton* button = new NoteEntryButton();
				button->box.pos = QUANTIZER_DISPLAY_SIZE * (noteAbsPositions[note] - QUANTIZER_DISPLAY_OFFSET) + OCTAVE_OFFSET * (2-octave);
				button->box.size = QUANTIZER_DISPLAY_SIZE * noteSizes[note];
				// button->box.pos = noteAbsPositions[note] - box.pos;
				// button->box.size = noteSizes[note];
				button->margin = margin;
				button->value = 3 - (note / 12.f + octave) - ROOT_OFFSET;
				button->parent = this;
				addChild(button);
			}
		}
	}
};

struct NoteEntryWidgetMenu : NoteEntryWidget{
	NoteControler * parent;
	NoteEntryWidgetMenu(){
		inMenu = true;
	}
	float getValue() override{
		if(parent->getExtra() != NE_NONE) return NoteEntryWidget_OFF; //Hide CV when extra is set
		else return parent->getValue();
	}
	void setValue(float value) override{
		parent->setExtra(NE_NONE); //Setting a CV clears extra
		parent->setValue(value);
	}
	NoteExtra getExtra() override{
		return parent->getExtra();
	}
	bool setExtra(NoteExtra value) override{
		if(parent->getExtra() == value) value = NE_NONE; //Clicking a 2nd time turns it off
		return parent->setExtra(value);
	}

};

struct NoteEntryWidgetPanel : NoteEntryWidget{
	float value;
	NoteExtra extra;
	NotePreviewer * previewer;
	NoteEntryWidgetPanel(){
		inMenu = false;		
		value = NoteEntryWidget_OFF;
		extra = NE_NONE;
	}
	float getValue() override{
		if(extra != NE_NONE) return NoteEntryWidget_OFF; //Hide CV when extra is set
		else return value;
	}
	void setValue(float newVal) override{
		extra = NE_NONE; //Clicking a note should clear the extra flag
		if(newVal == value) newVal = NoteEntryWidget_OFF; //Clicking a 2nd time turns it off
		value = newVal;
		if(previewer){
			previewer->setPreviewNote(newVal);
		}
	}
	NoteExtra getExtra() override{
		return extra;
	}
	bool setExtra(NoteExtra newExtra) override{
		if(newExtra == extra) newExtra = NE_NONE; //Clicking a 2nd time turns it off
		extra = newExtra;
		return true;
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

struct NoteBlockWidgetParent{
	virtual void updateDisplay();
};

struct CustomTrimpot : app::SvgKnob{
	ColoredSvgWidget* ring;
	widget::SvgWidget* bg;
	NoteBlockWidgetParent * parent = NULL;
	CustomTrimpot() {
		minAngle = -0.75 * M_PI;
		maxAngle = 0.75 * M_PI;

		bg = new widget::SvgWidget;
		ring = new ColoredSvgWidget;
		ring->color = nvgRGB(0xff,0x00,0x00);
		fb->addChildBelow(ring, tw);
		fb->addChildBelow(bg, tw);

		setSvg(Svg::load(asset::system("res/ComponentLibrary/Trimpot.svg")));
		bg->setSvg(Svg::load(asset::system("res/ComponentLibrary/Trimpot_bg.svg")));

		ring->setSvg(Svg::load(asset::plugin(pluginInstance,"res/Trimpot_ring.svg")));
		ring->box.pos.x -= 2.095f;
		ring->box.pos.y -= 2.095f;
		//auto ringSize = ring->box.size;
		// sw->box.size = ringSize;
		// tw->box.size = ringSize;
		// fb->box.size = ringSize;
		// bg->box.size = ringSize;
	}
	void onChange(const ChangeEvent& e) override {
		parent->updateDisplay();
		SvgKnob::onChange(e);
	}	
};

struct NoteWidget : widget::OpaqueWidget, NoteControler {
	NoteBlockWidgetParent * parent = NULL;
	NoteEntryWidgetPanel * panelSetter = NULL;
	bool displayed = true;
	bool canTie;
	CustomTrimpot* knob = NULL;
	ColoredSvgWidget* extra = NULL;
	std::shared_ptr<window::Svg> mute;
	std::shared_ptr<window::Svg> tie;
	NoteWidget() {
	}
	void init(NoteBlockWidgetParent* parent, Module* module, int paramId){
		this->parent = parent;

		knob = new CustomTrimpot();
		knob->box.pos = Vec(0,0);
		knob->module = module;
		knob->paramId = paramId;
		knob->initParamQuantity();
		knob->parent = parent;
		addChild(knob);

		extra = new ColoredSvgWidget();
		extra->box.pos = Vec(0,0);
		extra->color = nvgRGB(0xff,0x00,0x00);
		addChild(extra);

		mute = Svg::load(asset::plugin(pluginInstance,"res/mute_off.svg"));
		tie = Svg::load(asset::plugin(pluginInstance,"res/tie_off.svg"));

		box = knob->box;
	}
	void onButton(const widget::Widget::ButtonEvent& e) override {	
		if(!displayed) return;
		if (e.action == GLFW_PRESS && (e.mods & RACK_MOD_MASK) == 0) {
			if(e.button == GLFW_MOUSE_BUTTON_LEFT){
				// Left Click pulls from panelSetter if present and selected
				if(panelSetter && (panelSetter->value != NoteEntryWidget_OFF || panelSetter->extra != NE_NONE)){
					if(panelSetter->value != NoteEntryWidget_OFF){
						this->setValue(panelSetter->value);
						panelSetter->setValue(NoteEntryWidget_OFF);
					}
					if(this->setExtra(panelSetter->extra)){
						panelSetter->setExtra(NE_NONE);
						e.consume(this);
					}
					return;
				}				
			}else if(e.button == GLFW_MOUSE_BUTTON_RIGHT){
				// Right click to open context menu
				knob->destroyTooltip();
				createContextMenu();
				e.consume(this);
				return;
			}
		}
		OpaqueWidget::onButton(e);
	}
	void onHover(const HoverEvent& e) override{
		if(displayed) OpaqueWidget::onHover(e);
	}
	void onDragStart(const DragStartEvent& e) override{
		if(displayed) OpaqueWidget::onDragStart(e);
	}
	float getValue() override{
		return knob->getParamQuantity()->getValue();
	}
	void setValue(float value) override{
		knob->getParamQuantity()->setValue(value);
	}
	NoteExtra getExtra() override{
		return static_cast<NoteExtra>(knob->module->paramQuantities[knob->paramId + 1]->getValue());
	}
	bool setExtra(NoteExtra value) override{
		if(!canTie && value==NE_TIE) return false; 
		knob->module->paramQuantities[knob->paramId + 1]->setValue(value);
		parent->updateDisplay();
		return true;
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
		if(displayed){
			NoteExtra extraValue = NE_NONE;
			if(knob->module != NULL) extraValue = static_cast<NoteExtra>(knob->module->paramQuantities[knob->paramId + 1]->getValue());
			switch(extraValue){
				case NE_NONE:
					knob->draw(args);
					break;
				case NE_MUTE:
					extra->setSvg(mute);
					extra->draw(args);
					break;
				case NE_TIE:
					extra->setSvg(tie);
					extra->draw(args);
					break;
			}
		}
	}
};

#define NOTE_BLOCK_PARAM_COUNT 9

void configNoteBlock(Module * module, int paramIndex, bool firstBlock);

struct SubdivisionWidget : app::Switch
{
	struct Frame{
		std::shared_ptr<window::Svg> main;
		std::shared_ptr<window::Svg> notes [4];
	};

	Frame makeFrame(std::string root, std::string n1, std::string n2, std::string n3, std::string n4){
		Frame frame;
		frame.main = Svg::load(asset::plugin(pluginInstance,"res/subdiv/" + root + ".svg"));
		frame.notes[0] = Svg::load(asset::plugin(pluginInstance,"res/subdiv/" + n1 + ".svg"));
		if(n2 == "") n2 = n1;
		frame.notes[1] = Svg::load(asset::plugin(pluginInstance,"res/subdiv/" + n2 + ".svg"));
		if(n3 == "") n3 = n2;
		frame.notes[2] = Svg::load(asset::plugin(pluginInstance,"res/subdiv/" + n3 + ".svg"));
		if(n4 == "") n4 = n3;
		frame.notes[3] = Svg::load(asset::plugin(pluginInstance,"res/subdiv/" + n4 + ".svg"));
		return frame;
	}

	NoteBlockWidgetParent * parent = NULL;
	widget::FramebufferWidget* fb;
	widget::SvgWidget* sw;
	ColoredSvgWidget* noteLights [4];
	std::map<int,Frame> frames;
	int index;	

	SubdivisionWidget() {
		fb = new widget::FramebufferWidget;
		addChild(fb);
		sw = new widget::SvgWidget;
		fb->addChild(sw);
		for(int i = 0; i < 4; i++){
			noteLights[i] = new ColoredSvgWidget;
			noteLights[i]->color = nvgRGB(0xff,0x00,0x00);
			fb->addChild(noteLights[i]);
		}

		//1 Note
		Frame _1 = 		makeFrame("1",		"1_a1",		"",			"",			"");
		Frame _1_r = 	makeFrame("1_r",	"1_r1",		"",			"",			"");

		//2 Notes
		Frame _2 = 		makeFrame("2",		"2_a1",		"",			"2_a2",		"");
		Frame _2_ra = 	makeFrame("2_ra",	"2_r1",		"",			"2_a2f",	"");
		Frame _2_ar = 	makeFrame("2_ar",	"2_a1f",	"",			"2_r2",		"");

		//3 Notes
		Frame _3 = 		makeFrame("3",		"3_a1",		"3_a2",		"3_a3",		"");
		Frame _3_raa = 	makeFrame("3_raa",	"3_r1",		"3_a2",		"3_a3",		"");
		Frame _3_ara = 	makeFrame("3_ara",	"3_a1f",	"3_r2",		"3_a3f",	"");
		Frame _3_aar = 	makeFrame("3_aar",	"3_a1",		"3_a2",		"3_r3",		"");
		Frame _3_rar = 	makeFrame("3_rar",	"3_r1",		"3_a2f",	"3_r3",		"");
		Frame _3_arr = 	makeFrame("3_arr",	"3_a1f",	"3_r2d",	"",			"");

		Frame _4 = 		makeFrame("4",		"4_a1",		"4_a2",		"",			"4_a3");
		Frame _4_raa = 	makeFrame("4_raa",	"3_r1",		"4_a2",		"",			"4_a3");
		Frame _4_ara = 	makeFrame("4_ara",	"4_a1f",	"4_r2",		"",			"4_a3f");
		Frame _4_aar = 	makeFrame("4_aar",	"4_a1",		"4_a2",		"",			"4_r3");
		Frame _4_rra = 	makeFrame("4_rra",	"4_r1d",	"",			"",			"4_a3f");
		Frame _4_rar = 	makeFrame("4_rar",	"3_r1",		"4_a2f",	"",			"4_r3");
		Frame _4_arr = 	makeFrame("4_arr",	"4_a1f",	"4_r2",		"",			"4_r3");

		Frame _5 = 		makeFrame("5",		"5_a1",		"",			"5_a2",		"5_a3");
		Frame _5_raa = 	makeFrame("5_raa",	"5_r1",		"",			"5_a2",		"5_a3");
		Frame _5_ara = 	makeFrame("5_ara",	"5_a1f",	"",			"5_r2",		"5_a3f");
		Frame _5_aar = 	makeFrame("5_aar",	"5_a1",		"",			"5_a2",		"5_r3");
		Frame _5_rar = 	makeFrame("5_rar",	"5_r1",		"",			"5_a2f",	"5_r3");

		Frame _6 = 		makeFrame("6",		"6_a1",		"6_a2",		"6_a3",		"");
		Frame _6_raa = 	makeFrame("6_raa",	"6_r1",		"6_a2",		"6_a3",		"");
		Frame _6_ara = 	makeFrame("6_ara",	"6_a1f",	"6_r2",		"6_a3f",	"");
		Frame _6_aar = 	makeFrame("6_aar",	"6_a1",		"6_a2",		"6_r3",		"");
		Frame _6_rra = 	makeFrame("6_rra",	"6_r1",		"6_r2",		"6_a3f",	"");
		Frame _6_rar = 	makeFrame("6_rar",	"6_r1",		"6_a2f",	"6_r3",		"");
		Frame _6_arr = 	makeFrame("6_arr",	"6_a1f",	"6_r2",		"6_r3",		"");

		//4 Notes
		Frame _7 = 		makeFrame("7",		"7_a1",		"7_a2",		"7_a3",		"7_a4");

		Frame _7_raaa = makeFrame("7_raaa",	"7_r1",		"7_a2",		"7_a3",		"7_a4");
		Frame _7_araa = makeFrame("7_araa",	"7_a1f",	"7_r2",		"7_a3",		"7_a4");
		Frame _7_aara = makeFrame("7_aara",	"7_a1",		"7_a2",		"7_r3b",	"7_a4f");
		Frame _7_aaar = makeFrame("7_aaar",	"7_a1",		"7_a2",		"7_a3",		"7_r4");

		Frame _7_arra = makeFrame("7_arra",	"7_a1f",	"6_r2",		"",			"7_a4f");

		Frame _7_arar = makeFrame("7_arar",	"7_a1f",	"7_r2",		"7_a3f",	"7_r4");
		Frame _7_raar = makeFrame("7_raar",	"7_r1",		"7_a2",		"7_a3",		"7_r4");
		Frame _7_rara = makeFrame("7_rara",	"7_r1",		"7_a2f",	"7_r3",		"7_a4f");

		Frame _7_rarr = makeFrame("7_rarr",	"7_r1",		"7_a2f",	"2_r2",		"");
		Frame _7_rrar = makeFrame("7_rrar",	"2_r1",		"",			"7_a3f",	"7_r4");

		//1 Note
		addFrame(0x00000, _1);
		addFrame(0x01000, _1_r);

		//2 Notes
		addFrame(0x10000, _2);
		addFrame(0x11000, _2_ra);
		addFrame(0x10100, _2_ar);
		addFrame(0x11100, _1_r);

		//3 Notes
		addFrame(0x20000, _3);
		addFrame(0x21000, _3_raa);
		addFrame(0x20100, _3_ara);
		addFrame(0x20010, _3_aar);
		addFrame(0x21100, _2_ra);
		addFrame(0x21010, _3_rar);
		addFrame(0x20110, _3_arr);
		addFrame(0x21110, _1_r);

		addFrame(0x30000, _4);
		addFrame(0x31000, _4_raa);
		addFrame(0x30100, _4_ara);
		addFrame(0x30010, _4_aar);
		addFrame(0x31100, _4_rra);
		addFrame(0x31010, _4_rar);
		addFrame(0x30110, _3_arr);
		addFrame(0x31110, _1_r);

		addFrame(0x40000, _5);
		addFrame(0x41000, _5_raa);
		addFrame(0x40100, _5_ara);
		addFrame(0x40010, _5_aar);
		addFrame(0x41100, _4_rra);
		addFrame(0x41010, _5_rar);
		addFrame(0x40110, _2_ar);
		addFrame(0x41110, _1_r);

		addFrame(0x50000, _6);
		addFrame(0x50000, _6);
		addFrame(0x51000, _6_raa);
		addFrame(0x50100, _6_ara);
		addFrame(0x50010, _6_aar);
		addFrame(0x51100, _6_rra);
		addFrame(0x51010, _6_rar);
		addFrame(0x50110, _6_arr);
		addFrame(0x51110, _1_r);

		//4 Notes
		addFrame(0x60000, _7);

		addFrame(0x61000, _7_raaa);
		addFrame(0x60100, _7_araa);
		addFrame(0x60010, _7_aara);
		addFrame(0x60001, _7_aaar);

		addFrame(0x60011, _3_aar);
		addFrame(0x60110, _7_arra);
		addFrame(0x61100, _5_raa);

		addFrame(0x60101, _7_arar);
		addFrame(0x61001, _7_raar);
		addFrame(0x61010, _7_rara);

		addFrame(0x60111, _3_arr);
		addFrame(0x61011, _7_rarr);
		addFrame(0x61101, _7_rrar);
		addFrame(0x61110, _4_rra);
	}

	void addFrame(int key, Frame frame) {
		frames[key] = frame;
		// If this is our first frame, automatically set SVG and size
		if (!sw->svg) {
			sw->setSvg(frame.main);
			box.size = sw->box.size;
			fb->box.size = sw->box.size;
		}
	}

	void onChange(const ChangeEvent& e) override {
		parent->updateDisplay();
		ParamWidget::onChange(e);
	}

	void updateDisplay(){		
		engine::ParamQuantity* pq = getParamQuantity();
		if (pq) {
			int index = (int) std::round(pq->getValue() - pq->getMinValue());
			int key = 0x10000 * index;

			bool mute1 = module->paramQuantities[paramId + 2]->getValue() == NE_MUTE;
			bool mute2 = module->paramQuantities[paramId + 4]->getValue() == NE_MUTE;
			bool mute3 = module->paramQuantities[paramId + 6]->getValue() == NE_MUTE;
			bool mute4 = module->paramQuantities[paramId + 8]->getValue() == NE_MUTE;

			switch(index){
				case 0:
					if(mute1) key |= 0x01000;
					break;
				case 1:
					if(mute1) key |= 0x01000;
					if(mute3) key |= 0x00100;
					break;
				case 2:
				case 5:
					if(mute1) key |= 0x01000;
					if(mute2) key |= 0x00100;
					if(mute3) key |= 0x00010;
					break;
				case 3:
					if(mute1) key |= 0x01000;
					if(mute2) key |= 0x00100;
					if(mute4) key |= 0x00010;
					break;
				case 4:
					if(mute1) key |= 0x01000;
					if(mute3) key |= 0x00100;
					if(mute4) key |= 0x00010;
					break;
				case 6:	
					if(mute1) key |= 0x01000;
					if(mute2) key |= 0x00100;
					if(mute3) key |= 0x00010;
					if(mute4) key |= 0x00001;
					break;
			}

			Frame frame = frames[key];
			sw->setSvg(frame.main);
			for(int i = 0; i < 4; i++){
				noteLights[i]->setSvg(frame.notes[i]);
			}
			fb->setDirty();
		}
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

		Switch::draw(args);
	}
};

struct NoteBlockWidget : widget::Widget, NoteBlockWidgetParent{
	SubdivisionWidget* subdivWidget;
	NoteWidget* noteWidget[4];
	void init(Module * module, NoteEntryWidgetPanel * panelSetter, int index, int paramIndex){		
		subdivWidget = createParam<SubdivisionWidget>(mm2px(Vec(0,12)), module, paramIndex);
		subdivWidget->index = index;
		subdivWidget->parent = this;
		this->addChild(subdivWidget);

		const Vec POS[] = {mm2px(Vec(0,0)),mm2px(Vec(5,5)),mm2px(Vec(10,0)),mm2px(Vec(15,5))};
		
		for(int i = 0; i < 4; i++){
			noteWidget[i] = createWidget<NoteWidget>(POS[i]);
			noteWidget[i]->init(this, module, paramIndex + 1 + i * 2);
			noteWidget[i]->panelSetter = panelSetter;
			noteWidget[i]->canTie = i == 0 && index != 0;
			this->addChild(noteWidget[i]);
		}
	}

	int subdiv = -1;

	void step() override {
		if(subdivWidget->module != NULL){
			int newSubdiv = static_cast<int>(subdivWidget->getParamQuantity()->getValue());
			if(subdiv != newSubdiv){
				subdiv = newSubdiv;
				updateKnobs();
			}
		}
		Widget::step();
	}

	void updateDisplay() override{
		subdivWidget->updateDisplay();
		updateKnobs();
	}

	void updateKnobs(){
		const Vec GONE = Vec(-1,-1);
		const Vec POS[8][4] = {
			GONE,GONE,GONE,GONE, //Not Used
			mm2px(Vec(7.5,2.5)),GONE,GONE,GONE,
			mm2px(Vec(2.5,2.5)),GONE,mm2px(Vec(12.5,2.5)),GONE,

			mm2px(Vec(0,0)),mm2px(Vec(5,5)),mm2px(Vec(12.5,2.5)),GONE,
			mm2px(Vec(0,0)),mm2px(Vec(7.5,2.5)),GONE,mm2px(Vec(15,5)),
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