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
		EVOLUTION_ON_PARAM,
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

	int evolutionMapping [MAX_SEQ_LENGTH];
	int randomEvolution [MAX_SEQ_LENGTH];
	bool evolveUpOrDownBias;

	float seqLengthScalar;

	//Non Persistant State
	
	bool resetHigh;
	float previewNote;	

	bool hasHadFirstClockHigh;	

	int currentEvolvedPulse;
	bool evolveOn;

	Sequencer3() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(CLOCK_INPUT,"Clock");
		configInput(RESET_INPUT,"Reset");
		configOutput(GATE_OUTPUT,"Gate");
		configOutput(CV_OUTPUT,"CV");

		auto evoOnQ = configSwitch(EVOLUTION_ON_PARAM, 0, 1, 0, "Evolution", std::vector<std::string>{"off","ON"});		
		evoOnQ->randomizeEnabled = false;

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

		seqLengthScalar = 1;

		currentEvolvedPulse = -1;
		evolveUpOrDownBias = true;
		for(int bi = 0; bi < MAX_SEQ_LENGTH; bi++){
			evolutionMapping[bi] = -1;
			randomEvolution[bi] = -1;
		}
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

			bool _evolveOn = params[EVOLUTION_ON_PARAM].getValue() == 1;

			if(evolveOn != _evolveOn){
				evolveOn = _evolveOn;
				if(evolveOn){
					//Clear Evolution when the switch is turned on so we get a fresh run
					for(int bi = 0; bi < MAX_SEQ_LENGTH; bi++){
						evolutionMapping[bi] = -1;
						randomEvolution[bi] = -1;
					}
				}
			}

			int maxPulse = params[SEQ_LENGTH_PARAM].getValue() * seqLengthScalar * 24; //24 Pulses per Sixtenth Note			

			//Wrap Pulse
			if(currentPulse >= maxPulse){
				currentPulse = 0;
				if(evolveOn){
					doEvolve();
				}
			}

			float cv;
			bool updateCV;
			bool gateHigh;

			int pulse = currentPulse;

			if(evolveOn){
				int block = pulse/24;
				int pulseInBlock = pulse - block * 24;
				int rndBlock = randomEvolution[block];
				int evolvedBlock = evolutionMapping[block];
				if(rndBlock != -1) block = rndBlock;
				else if(evolvedBlock != -1)  block = evolvedBlock;
				pulse = pulseInBlock + block * 24;
				currentEvolvedPulse = currentPulse == pulse ? - 1 : pulse;
			}else{
				currentEvolvedPulse = -1;
			}

			getOuputValues(this,NOTE_BLOCK_PARAM,pulse,cv,updateCV,gateHigh);
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

	void doEvolve(){
		using rack::random::uniform;

		int maxBlock = params[SEQ_LENGTH_PARAM].getValue() * seqLengthScalar;

		int evolvedBlocks = 0;
		for(int bi = 0; bi < maxBlock; bi++){
			if(evolutionMapping[bi] != -1) evolvedBlocks++;
		}
		float percentEvolved = evolvedBlocks / (float) maxBlock;

		float evolveUpChance = 1 - percentEvolved;

		DEBUG("percentEvolved:%f evolveUpChance:%f",percentEvolved,evolveUpChance);

		//Temp Evolutions
		{
			//Mirror Chance
			for(int bi = 0; bi < MAX_SEQ_LENGTH; bi++){
				randomEvolution[bi] = -1;
				//Ranomd chance to ghost to the corresponding block on the other row
				if(uniform() < 0.2){
					randomEvolution[bi] = (bi + 8) % 16;
				}
			}
			
			//Random One-Time
			{
				//Randomly Map one to another temporarily
				int x = rndInt(maxBlock);
				int y = rndInt(MAX_SEQ_LENGTH);
				randomEvolution[x] = y;
				DEBUG("randomEvolution %i -> %i",x,y);
			}
		}


		//Semi-Permeanant Evolution Chance
		if(uniform() < 0.5){

			if(evolveUpOrDownBias){
				//DnD Advantage
				evolveUpChance = 1-evolveUpChance;
				evolveUpChance = evolveUpChance * evolveUpChance;
				evolveUpChance = 1-evolveUpChance;
				DEBUG("Up/Advantage evolveUpChance:%f",evolveUpChance);
			}else{
				//DnD Disadvantage
				evolveUpChance = evolveUpChance * evolveUpChance;
				DEBUG("Down/Disadvantage evolveUpChance:%f",evolveUpChance);
			}

			if(uniform() < evolveUpChance){
				addEvolution();
			}else{
				removeEvolution();
			}

			if(percentEvolved > 0.8 && evolveUpOrDownBias){
				evolveUpOrDownBias = false;
			}else if(percentEvolved <= 0 && !evolveUpOrDownBias){
				evolveUpOrDownBias = true;
			}
		}
	}

	void addEvolution(){
		using rack::random::uniform;

		std::vector<int> indexes;
		int maxBlock = params[SEQ_LENGTH_PARAM].getValue() * seqLengthScalar;
		for(int bi = 0; bi < maxBlock; bi++){
			if(evolutionMapping[bi] == -1) indexes.push_back(bi);
		}
		if(indexes.size() > 0){
			int inBlock = indexes[rndInt(indexes.size())];
			int outBlock = rndInt(MAX_SEQ_LENGTH);
			evolutionMapping[inBlock] = outBlock;

			//Chance to map a run of sequential blocks
			while(uniform() < 0.5){
				outBlock++;
				if(outBlock >= MAX_SEQ_LENGTH) return;

				inBlock++;
				if(inBlock >= MAX_SEQ_LENGTH) return;
				
				//Note this allows mapping over existing evolutions

				evolutionMapping[inBlock] = outBlock;
			}
		}
	}

	void removeEvolution(){
		using rack::random::uniform;

		std::vector<int> indexes;
		int maxBlock = params[SEQ_LENGTH_PARAM].getValue();
		for(int bi = 0; bi < maxBlock; bi++){
			if(evolutionMapping[bi] != -1) indexes.push_back(bi);
		}
		if(indexes.size() > 0){
			int inBlock = indexes[rndInt(indexes.size())];
			evolutionMapping[inBlock] = -1;

			//Chance to clear a run of sequential blocks
			while(uniform() < 0.5){
				inBlock++;
				if(inBlock >= MAX_SEQ_LENGTH) return;

				evolutionMapping[inBlock] = -1;
			}
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
	  					if(cv <= NoteEntryWidget_MAX - 1){
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
	  			params[NOTE_BLOCK_PARAM + bi * NOTE_BLOCK_PARAM_COUNT + 1 + ni * 2].setValue(cv);
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
  			params[NOTE_BLOCK_PARAM + bi * NOTE_BLOCK_PARAM_COUNT].setValue(subdivision);

  			//Set Note Extra
	  		for(int ni = 0; ni < 4; ni++){
	  			NoteExtra noteExtra = NE_NONE;
	  			if(uniform() < 0.1) noteExtra = NE_MUTE;
	  			if(ni == 0 && bi != 0 && uniform() < 0.05)  noteExtra = NE_TIE;
	  			params[NOTE_BLOCK_PARAM + bi * NOTE_BLOCK_PARAM_COUNT + 2 + ni * 2].setValue(noteExtra);
	  		}
	  	}
  	}

  	void shiftBlocks(int delta){
  		//Also shift current pulse to prevent weird hickups in play back
  		currentPulse += delta * 24;

  		delta *= NOTE_BLOCK_PARAM_COUNT;
  		const int MAX = MAX_SEQ_LENGTH * NOTE_BLOCK_PARAM_COUNT;
  		float paramValues [MAX] = {};
  		for(int i = 0; i < MAX; i++){
  			paramValues[i] = params[NOTE_BLOCK_PARAM + i].getValue();
  		}
  		for(int i = 0; i < MAX; i++){
  			int i2 = i - delta;
  			i2 = mod_0_max(i2,MAX);
  			params[NOTE_BLOCK_PARAM + i].setValue(paramValues[i2]);
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

			x += dx;

			x += dx;
			addParam(createParamCentered<CKSS>(Vec(x,y), module, Sequencer3::EVOLUTION_ON_PARAM));
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
		int pulseEvolved = module->currentEvolvedPulse;
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

			int blockEvolved, noteEvolved;
			getNoteAndBlock(module,Sequencer3::NOTE_BLOCK_PARAM,pulseEvolved,blockEvolved,noteEvolved);

			//DEBUG("pulse:%i block:%i blockType:%i pulseInBlock:%i noteIndexInBlock:%i ",pulse,block,blockType,pulseInBlock,noteIndexInBlock);
			
			//DEBUG("lastBlockIndex:%i lastNoteIndex:%i",lastBlockIndex,lastNoteIndex);

			for(int bi = 0; bi < MAX_SEQ_LENGTH; bi ++){
				for(int ni = 0; ni < 4; ni++){
					NVGcolor color = COLOR_TRANSPARENT;
					if(bi == block && ni == noteIndex){
						if(pulseEvolved == -1){
							color = COLOR_ACTIVE_NOTE;
						}else{
							color = COLOR_ACTIVE_GHOST_NOTE;
						}
					}else if(bi == blockEvolved && ni == noteEvolved){
						color = COLOR_EVOLVED;
					}else if(bi == lastBlockIndex && ni == lastNoteIndex){
						color = COLOR_LAST_NOTE;
					}
					noteBlocks[bi]->setColor(ni,color);
				}
				noteBlocks[bi]->updateDisplay();
			}
		}
	}

	void appendContextMenu(Menu* menu) override {
		Sequencer3* module = dynamic_cast<Sequencer3*>(this->module);

		menu->addChild(new MenuEntry); //Blank Row
		menu->addChild(createMenuLabel("Sequencer3"));

		menu->addChild(createSubmenuItem("Randomize", "",
			[module](Menu* menu) {
				menu->addChild(createMenuItem("CVs", "",
					[=]() {
						module->randomizeCVs();
					}
				));

				menu->addChild(createMenuItem("Rythm", "",
					[=]() {
						module->randomizeRythm();
					}
				));
			}
		));

		menu->addChild(createSubmenuItem("Shift", "",
			[module](Menu* menu) {
				menu->addChild(createMenuItem("Block Forward", "",
					[=]() {
						module->shiftBlocks(+1);
					}
				));

				menu->addChild(createMenuItem("Block Backward", "",
					[=]() {
						module->shiftBlocks(-1);
					}
				));
			}
		));

		
	}


};


Model* modelSequencer3 = createModel<Sequencer3, Sequencer3Widget>("Sequencer3");