#include "plugin.hpp"
#include "util.hpp"
#include "widgets.hpp"
#include "scales.hpp"

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

		auto lengthQ = configParam(SEQ_LENGTH_PARAM, 1, MAX_SEQ_LENGTH, 8, "Sequence Length");
		lengthQ->randomizeEnabled = false;

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
			int maxPulse = params[SEQ_LENGTH_PARAM].getValue() * 24; //24 Pulses per Sixtenth Note			
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

	void onRandomize (const RandomizeEvent& e) override {
  		Module::onRandomize(e);

		using rack::random::uniform;

		randomizeCVs();
		randomizeRythm();

	  	//Randomize Notes
  		int length = 1;
  		length += rndInt(7);
  		if(uniform() < 0.3) length += rndInt(7);
  		//if(uniform() < 0.1) length += rndInt(4); Causes issues if last block is a tripplet, leaving this out for now
  		params[SEQ_LENGTH_PARAM].setValue(length);
  	}

  	void randomizeCVs(){
		using rack::random::uniform;  		
  		
  		//Get Scale
  		int scale = std::floor(uniform()*NUM_OF_SCALES);
  		std::vector<int> notes = SCALES[scale];
  		int semitoneOffset = rndInt(12)-6;
  		if(uniform() < 0.25) semitoneOffset -= 12;
  		int size = notes.size();

  		//Misc
  		float highVsLowOctaveOdds = 0.25 + 0.5 * uniform();
  		float octaveShiftOdds = uniform() * 0.15 + (uniform() < 0.3 ? 0.15 : 0);

  		float wholeBlockSameOdds = uniform() * 0.15 + (uniform() < 0.3 ? 0.15 : 0);

  		float extrRootNoteOdds = uniform() * 0.15 + (uniform() < 0.3 ? 0.15 : 0);
  		float lowNoteOdds = uniform() * 0.3 + (uniform() < 0.3 ? 0.3 : 0);
  		float hiteNoteOdds = uniform() * 0.3 + (uniform() < 0.3 ? 0.3 : 0);

  		DEBUG("randomizeCVs");

  		//Walk Block
  		for(int bi = 0; bi < MAX_SEQ_LENGTH; bi++){
  			bool wholeBlockSame = uniform() < wholeBlockSameOdds;
  			float prevCV = 0;
	  		for(int ni = 0; ni < 4; ni++){
	  			int ri;
	  			if(uniform() < extrRootNoteOdds){
	  				//Extra Chance to be root note
	  				ri = 0;
	  			}else{
		  			ri = rndInt(size);
		  			//Extra Chance to be a low note
		  			if(uniform() < lowNoteOdds){
		  				ri /= 2;
		  			}
		  			if(uniform() < hiteNoteOdds){
		  				ri = ri * 2;
		  				if(ri >= size) ri = size-1;
		  			}
		  		}

	  			float cv = (notes[ri]+semitoneOffset) / 12.f;
	  			DEBUG("ri: %i cv: %f",ri,cv);

	  			//Chance for octave shift
	  			if(uniform() < octaveShiftOdds){
	  				if(uniform() < highVsLowOctaveOdds){
	  					if(cv <= NoteEntryWidget_MAX + 1){
	  						cv += 1;
	  						DEBUG("+1 octave cv: %f",cv);
	  					}
	  				}else{
	  					if(cv >= NoteEntryWidget_MIN + 1){
	  						cv -= 1;
	  						DEBUG("-1 octave cv: %f",cv);
	  					}
	  				}
	  			}


	  			//Replace cv with previous if wholeBlockSame and not the first note in the block
	  			if(ni > 0 && wholeBlockSame){
	  				cv = prevCV;
	  				DEBUG("wholeBlockSame cv: %f",cv);
	  			}
	  			params[bi * NOTE_BLOCK_PARAM_COUNT + 1 + ni * 2].setValue(cv);
	  			prevCV = cv;
	  		}
	  	}
  	}

  	void randomizeRythm(){
		using rack::random::uniform;  		
  		
		bool allowTripplets = uniform() < 0.3;
		bool allTippletsAndQuarter = uniform() < 0.15;

		float extraQuarterOdds = uniform() * 0.3 + (uniform() < 0.3 ? 0.3 : 0);
		float extraEighthOdds = uniform() * 0.3 + (uniform() < 0.3 ? 0.3 : 0);
		//Extra Extra Low Energy Chance
		if(uniform() < 0.3){
			extraQuarterOdds += 0.5;
			extraEighthOdds += 0.5;
		}
		//Extra Extra High Energy Chance
		if(uniform() < 0.3){
			extraQuarterOdds /= 3;
			extraEighthOdds /= 3;
		}

  		//Walk Block
  		for(int bi = 0; bi < MAX_SEQ_LENGTH; bi++){
  			//Set Sub Division
  			int subdivision;
  			if(allTippletsAndQuarter){
  				subdivision = uniform() < 0.3 ?  SubDiv_Quarter : SubDiv_Tipplets;
  			}else{
  				subdivision = 1 + rndInt(7);
  				if(!allowTripplets && subdivision == SubDiv_Tipplets) subdivision = SubDiv_Eighth;
  				if(uniform() < extraQuarterOdds) subdivision = SubDiv_Quarter;
  				if(uniform() < extraEighthOdds) subdivision = SubDiv_Eighth;
  			}
  			params[bi * NOTE_BLOCK_PARAM_COUNT].setValue(subdivision);

  			//Set Note Extra
	  		for(int ni = 0; ni < 4; ni++){
	  			NoteExtra noteExtra = NE_NONE;
	  			if(uniform() < 0.1) noteExtra = NE_MUTE;
	  			if(ni == 0 && bi != 0 && uniform() < 0.05)  noteExtra = NE_TIE;
	  			params[bi * NOTE_BLOCK_PARAM_COUNT + 2 + ni * 2].setValue(noteExtra);
	  		}
	  	}
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
			noteEntry->module = module;
			noteEntry->baseParamIndex = Sequencer3::NOTE_BLOCK_PARAM;
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
					noteBlock->init(module, noteEntry, i, Sequencer3::NOTE_BLOCK_PARAM + i2);
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
	int prevLastBlock;
	int prevLastNote;

	void step() override {
		ModuleWidget::step();
		
		Sequencer3* module = dynamic_cast<Sequencer3*>(this->module);
		if(module == NULL) return;

		int pulse = module->currentPulse;
		int lastBlockIndex = this->noteEntry->lastBlockIndex;
		int lastNoteIndex = this->noteEntry->lastNoteIndex;

		bool dirty = false;;

		if(prevPulse != pulse){
			prevPulse = pulse;
			dirty = true;
		}

		if(prevLastBlock != lastBlockIndex){
			prevLastBlock = lastBlockIndex;
			dirty = true;
		}

		if(prevLastNote != lastNoteIndex){
			prevLastNote = lastNoteIndex;
			dirty = true;
		}

		if(dirty){
			int block, noteIndex;
			getNoteAndBlock(module,Sequencer3::NOTE_BLOCK_PARAM,pulse,block,noteIndex);

			//DEBUG("pulse:%i block:%i blockType:%i pulseInBlock:%i noteIndexInBlock:%i ",pulse,block,blockType,pulseInBlock,noteIndexInBlock);
			
			//DEBUG("lastBlockIndex:%i lastNoteIndex:%i",lastBlockIndex,lastNoteIndex);

			for(int bi = 0; bi < MAX_SEQ_LENGTH; bi ++){
				for(int ni = 0; ni < 4; ni++){
					NVGcolor color = COLOR_TRANSPARENT;
					if(bi == block && ni == noteIndex){
						color = COLOR_ACTIVE_NOTE;
					}else if(bi == lastBlockIndex && ni == lastNoteIndex){
						color = COLOR_LAST_NOTE;
					}
					noteBlocks[bi]->setColor(ni,color);
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