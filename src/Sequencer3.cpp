#include "plugin.hpp"
#include "util.hpp"
#include "widgets.hpp"

#define MAX_SEQ_LENGTH 16

#define MIN_NOTE_DUR -3
#define MAX_NOTE_DUR 4
#define DEFAULT_NOTE_DUR 2

struct Sequencer3 : Module {
	enum ParamId {
		ENUMS(MAIN_SEQ_NOTE_CV_PARAM, MAX_SEQ_LENGTH),
		ENUMS(MAIN_SEQ_DURATION_PARAM, MAX_SEQ_LENGTH),
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
		ENUMS(MAIN_SEQ_ACTIVE_LIGHT, MAX_SEQ_LENGTH * 3),
		LIGHTS_LEN
	};

	//Persistant State

	int currentStep;
	int currentBeat;
	int currentDur;
	bool muted;

	//Non Persistant State

	bool clockHigh = false;
	bool resetHigh = false;

	Sequencer3() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(CLOCK_INPUT,"Clock");
		configInput(RESET_INPUT,"Reset");
		configOutput(GATE_OUTPUT,"Gate");
		configOutput(CV_OUTPUT,"CV");

		configParam(SEQ_LENGTH_PARAM, 1, MAX_SEQ_LENGTH * 4, 8, "Sequence Length");

		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){

			std::string str_ni = std::to_string(ni+1);

			configParam(MAIN_SEQ_NOTE_CV_PARAM + ni, NoteEntryWidget_MIN, NoteEntryWidget_MAX, 0.0f, "CV " + str_ni, "V");
			configParam(MAIN_SEQ_DURATION_PARAM + ni, MIN_NOTE_DUR, MAX_NOTE_DUR, DEFAULT_NOTE_DUR, "Duration " + str_ni);

		}
		initalize();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		initalize();
	}

	void initalize(){
		clockHigh = false;
		resetHigh = false;

		currentStep = -1;
		currentBeat = -1;
		currentDur = 0;

		muted = false;
	}

	void process(const ProcessArgs& args) override {
		//Reset Logic
		if(schmittTrigger(resetHigh,inputs[RESET_INPUT].getVoltage())){
			currentStep = -1;
			currentBeat = -1;
			currentDur = 0;
			muted = false;
		}

		//Clock Logic
		if(schmittTrigger(clockHigh,inputs[CLOCK_INPUT].getVoltage())){
			int maxStep = params[SEQ_LENGTH_PARAM].getValue();

			bool resetCycle = false;
			bool doNextStep = false;

			currentBeat++;
			if(currentBeat >= maxStep) resetCycle = true;	

			if(currentDur > 1){
				currentDur--;
			}else{
				doNextStep = true;
				currentStep++;
			}					

			if(resetCycle){
				currentStep = 0;
				currentBeat = 0;
			}

			if(doNextStep){
				//Use Duration from Current Step
				int dur = params[MAIN_SEQ_DURATION_PARAM + currentStep].getValue();
				muted = dur <= 0;
				if(muted) dur = -dur + 1;
				currentDur = dur;
			}
		}

		//Update Outputs
		if(muted){
			//CV Holds Value
			outputs[GATE_OUTPUT].setVoltage(0);
		}else{
			float val = 0;
			int stepIndex = currentStep;
			if(stepIndex >= 0 && stepIndex < MAX_SEQ_LENGTH){ 
				val = params[MAIN_SEQ_NOTE_CV_PARAM + stepIndex].getValue();
			}
			float cv = val;
			outputs[CV_OUTPUT].setVoltage(cv);
			outputs[GATE_OUTPUT].setVoltage((clockHigh || currentDur > 1) ? 10 : 0);
		}

		//Update Lights
		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){
			lights[MAIN_SEQ_ACTIVE_LIGHT + ni * 3 + 2].setBrightness(currentStep == ni ? 1 : 0);
		}
	}

	int countSteps(){
		int maxBeats = params[SEQ_LENGTH_PARAM].getValue();
		int step = 0;
		while(maxBeats > 0){
			int dur = params[MAIN_SEQ_DURATION_PARAM + step].getValue();
			muted = dur <= 0;
			if(muted) dur = -dur + 1;
			maxBeats-=dur;
			step++;
		}
		return step;
	}
};


struct Sequencer3Widget : ModuleWidget {
	Sequencer3Widget(Sequencer3* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Blank26hp.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float dx = RACK_GRID_WIDTH * 2;
		float dy = RACK_GRID_WIDTH * 2;

		float yStart = RACK_GRID_WIDTH*2;
		float xStart = RACK_GRID_WIDTH;

		float x = xStart;
		float y = yStart;

		addInput(createInputCentered<PJ301MPort>(Vec(x,y), module, Sequencer3::CLOCK_INPUT));
		y += dy;
		addInput(createInputCentered<PJ301MPort>(Vec(x,y), module, Sequencer3::RESET_INPUT));
		y += dy;
		addOutput(createOutputCentered<PJ3410Port>(Vec(x,y), module, Sequencer3::GATE_OUTPUT));
		y += dy;
		addOutput(createOutputCentered<PJ3410Port>(Vec(x,y), module, Sequencer3::CV_OUTPUT));
		y += dy;
		addParam(createParamCentered<RotarySwitch<RoundSmallBlackKnob>>(Vec(x,y), module, Sequencer3::SEQ_LENGTH_PARAM));
		

		x += dx * 2;
		y = yStart;
		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){			

			addParam(createParamCentered<NoteWidget<RoundSmallBlackKnob>>(Vec(x,y), module, Sequencer3::MAIN_SEQ_NOTE_CV_PARAM + ni));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(x + dx*0.55f, y), module, Sequencer3::MAIN_SEQ_ACTIVE_LIGHT + ni * 3));
			addParam(createParamCentered<RotarySwitch<Trimpot>>(Vec(x + dx,y), module, Sequencer3::MAIN_SEQ_DURATION_PARAM + ni));

			y += dy;

			if(ni == 7){
				y = yStart;
				x += dx * 3;
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