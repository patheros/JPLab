#include "plugin.hpp"
#include "util.hpp"
#include "widgets.hpp"

#define ROW_COUNT 2
#define COL_COUNT 8
#define MAX_SEQ_LENGTH (ROW_COUNT * COL_COUNT)

#define MIN_NOTE_DUR -3
#define MAX_NOTE_DUR 4
#define DEFAULT_NOTE_DUR 2

struct Sequencer3 : Module, NotePreviewer {
	enum ParamId {
		ENUMS(NOTE_BLOCK_PARAM, MAX_SEQ_LENGTH * NOTE_BLOCK_PARAM_COUNT),
		SEQ_LENGTH_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,	
		INPUTS_LEN
	};
	enum OutputId {
		GATE_OUTPUT,
		CV_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		//ENUMS(MAIN_SEQ_ACTIVE_LIGHT, MAX_SEQ_LENGTH * 3),
		LIGHTS_LEN
	};

	//Persistant State

	int clockCounter;
	int clockLength;
	int currentPulse;
	int pulseCounter;
	bool clockHigh;

	//Non Persistant State
	
	bool resetHigh;
	float previewNote;	

	bool hasHadFirstClockHigh;	

	Sequencer3() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(CLOCK_INPUT,"Clock");
		configInput(RESET_INPUT,"Reset");
		configOutput(GATE_OUTPUT,"Gate");
		configOutput(CV_OUTPUT,"CV");

		configParam(SEQ_LENGTH_PARAM, 1, MAX_SEQ_LENGTH * 4, 8, "Sequence Length");

		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){
			configNoteBlock(this,NOTE_BLOCK_PARAM + ni * NOTE_BLOCK_PARAM_COUNT, ni == 0);
		}
		initalize();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		initalize();
	}

	void initalize(){
		hasHadFirstClockHigh = false;

		clockHigh = false;
		resetHigh = false;

		clockCounter = 0;
		clockLength = 0;

		previewNote = NoteEntryWidget_OFF;

		currentPulse = -1;
		pulseCounter = 0;
	}

	json_t *dataToJson() override{
		json_t *jobj = json_object();

		json_object_set_new(jobj, "clockCounter", json_integer(clockCounter));
		json_object_set_new(jobj, "clockLength", json_integer(clockLength));
		json_object_set_new(jobj, "currentPulse", json_integer(currentPulse));
		json_object_set_new(jobj, "pulseCounter", json_integer(pulseCounter));
		json_object_set_new(jobj, "clockHigh", json_bool(clockHigh));

		return jobj;
	}

	void dataFromJson(json_t *jobj) override {		

		clockCounter = json_integer_value(json_object_get(jobj, "clockCounter"));
		clockLength = json_integer_value(json_object_get(jobj, "clockLength"));
		currentPulse = json_integer_value(json_object_get(jobj, "currentPulse"));
		pulseCounter = json_integer_value(json_object_get(jobj, "pulseCounter"));	
		clockHigh = json_is_true(json_object_get(jobj, "clockHigh"));	
	}

	void process(const ProcessArgs& args) override {

		bool clockHighEvent = schmittTrigger(clockHigh,inputs[CLOCK_INPUT].getVoltage());	
		if(clockHighEvent && !hasHadFirstClockHigh){
			clockHighEvent = false;
			hasHadFirstClockHigh = true;
			clockCounter = 0;
		}
		countClockLength(clockCounter,clockLength,clockHighEvent);

		if(clockLength <= 0) return;
		// DEBUG("clockLength:%i clockCounter:%i",clockLength,clockCounter);

		bool pulseEvent = false;
		if(pulseCounter > 0){
			pulseCounter--;
		}else{
			pulseCounter = clockLength / 24; //Pulses are 24 Pulses per Quarter Note
			pulseEvent = true;
		}

		//Reset Logic
		if(schmittTrigger(resetHigh,inputs[RESET_INPUT].getVoltage())){
			currentPulse = -1;
			pulseCounter = 0;
		}

		//Clock Logic
		if(pulseEvent){
			//Incremnt Pulse
			currentPulse++;

			//Wrap Pulse
			int maxPulse = params[SEQ_LENGTH_PARAM].getValue() * 6; //6 Pulses per Sixtenth Note			
			if(currentPulse >= maxPulse){
				currentPulse = 0;
			}

			float cv;
			bool updateCV;
			bool gateHigh;
			getOuputValues(this,NOTE_BLOCK_PARAM,currentPulse,cv,updateCV,gateHigh);
			if(updateCV){
				outputs[CV_OUTPUT].setVoltage(cv);
			}
			outputs[GATE_OUTPUT].setVoltage(gateHigh ? 10 : 0);		
		}

		//Overide Output when preview is high
		if(previewNote != NoteEntryWidget_OFF){
			//Preview Note
			outputs[CV_OUTPUT].setVoltage(previewNote);
			outputs[GATE_OUTPUT].setVoltage(clockHigh ? 10 : 0);
		}
	}

	void setPreviewNote(float note) override{
		previewNote = note;
	}
};

struct Sequencer3Widget : ModuleWidget {
	NoteEntryWidgetPanel * noteEntry;
	NoteBlockWidget* noteBlocks[MAX_SEQ_LENGTH];
	Sequencer3Widget(Sequencer3* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Blank36hp.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float dx = RACK_GRID_WIDTH * 2;
		float dy = RACK_GRID_WIDTH * 2;

		float yStart = RACK_GRID_WIDTH*2;
		float xStart = RACK_GRID_WIDTH;

		{
			float x = xStart;
			float y = yStart + dy * 10;

			addInput(createInputCentered<PJ301MPort>(Vec(x,y), module, Sequencer3::CLOCK_INPUT));
			x += dx;
			addInput(createInputCentered<PJ301MPort>(Vec(x,y), module, Sequencer3::RESET_INPUT));
			x += dx;
			addOutput(createOutputCentered<PJ3410Port>(Vec(x,y), module, Sequencer3::GATE_OUTPUT));
			x += dx;
			addOutput(createOutputCentered<PJ3410Port>(Vec(x,y), module, Sequencer3::CV_OUTPUT));
			x += dx;
			addParam(createParamCentered<RotarySwitch<RoundSmallBlackKnob>>(Vec(x,y), module, Sequencer3::SEQ_LENGTH_PARAM));
		}

		{
			noteEntry = createWidget<NoteEntryWidgetPanel>(Vec(1.25f * dx, yStart + dy * 5.2f));
			noteEntry->init();
			noteEntry->previewer = module;
			addChild(noteEntry);
		}

		// for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){			
		// 	auto noteWidget = createParamCentered<NoteWidget>(Vec(x,y), module, Sequencer3::MAIN_SEQ_NOTE_CV_PARAM + ni);
		// 	noteWidget->panelSetter = noteEntry;
		// 	addParam(noteWidget);
		// 	addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(x + dx*0.55f, y), module, Sequencer3::MAIN_SEQ_ACTIVE_LIGHT + ni * 3));
		// 	addParam(createParamCentered<RotarySwitch<Trimpot>>(Vec(x + dx,y), module, Sequencer3::MAIN_SEQ_DURATION_PARAM + ni));

		// 	y += dy;

		// 	if(ni == 7){
		// 		y = yStart;
		// 		x += dx * 3;
		// 	}
		// }

		{
			float dx2 = dx * 2.16;
			float dy2 = dy * 2.65;
			for(int row = 0; row < ROW_COUNT; row ++){
				for(int col = 0; col < COL_COUNT; col++){
					int i = (row * COL_COUNT + col);
					int i2 = i * NOTE_BLOCK_PARAM_COUNT;
					NoteBlockWidget* noteBlock = createWidget<NoteBlockWidget>(Vec(0.2 * dx + dx2 * col, dy * 0.5 + dy2 * row));
					noteBlock->init(module, noteEntry, i2, Sequencer3::NOTE_BLOCK_PARAM + i2);
					addChild(noteBlock);		
					noteBlocks[i] = noteBlock;
				}
			}
		}
		


		// QuantizerDisplay* quantizerDisplay = createWidget<QuantizerDisplay>(Vec(x,y));
		// QuantizerDisplay* quantizerDisplay = createWidget<QuantizerDisplay>(mm2px(Vec(0.0, 13.039)));
		// quantizerDisplay->box.size = mm2px(Vec(15.24, 55.88));
		// quantizerDisplay->setModule(module);
		// addChild(quantizerDisplay);
	}

	int prevPulse;

	void step() override {
		ModuleWidget::step();
		
		Sequencer3* module = dynamic_cast<Sequencer3*>(this->module);
		if(module == NULL) return;

		int pulse = module->currentPulse;

		if(prevPulse != pulse){
			prevPulse = pulse;
			int block, noteIndex;
			getNoteAndBlock(module,Sequencer3::NOTE_BLOCK_PARAM,pulse,block,noteIndex);


			//DEBUG("pulse:%i block:%i blockType:%i pulseInBlock:%i noteIndexInBlock:%i ",pulse,block,blockType,pulseInBlock,noteIndexInBlock);
			
			for(int bi = 0; bi < MAX_SEQ_LENGTH; bi ++){
				if(bi == block){
					switch(noteIndex){
						case 0:	noteBlocks[bi]->setColors(COLOR_ACTIVE_NOTE,COLOR_TRANSPARENT,COLOR_TRANSPARENT,COLOR_TRANSPARENT); break;
						case 1:	noteBlocks[bi]->setColors(COLOR_TRANSPARENT,COLOR_ACTIVE_NOTE,COLOR_TRANSPARENT,COLOR_TRANSPARENT); break;
						case 2:	noteBlocks[bi]->setColors(COLOR_TRANSPARENT,COLOR_TRANSPARENT,COLOR_ACTIVE_NOTE,COLOR_TRANSPARENT); break;
						case 3:	noteBlocks[bi]->setColors(COLOR_TRANSPARENT,COLOR_TRANSPARENT,COLOR_TRANSPARENT,COLOR_ACTIVE_NOTE); break;
					}
				}else{
					noteBlocks[bi]->setColors(COLOR_TRANSPARENT,COLOR_TRANSPARENT,COLOR_TRANSPARENT,COLOR_TRANSPARENT);	
				}
				noteBlocks[bi]->updateDisplay();
			}
		}
	}

	// void appendContextMenu(Menu* menu) override {
	// 	Sequencer3* module = dynamic_cast<Sequencer3*>(this->module);

	// 	menu->addChild(new MenuEntry); //Blank Row
	// 	menu->addChild(createMenuLabel("Sequencer3"));
	// }
};


Model* modelSequencer3 = createModel<Sequencer3, Sequencer3Widget>("Sequencer3");