#include "plugin.hpp"
#include "util.hpp"

#define MAX_SEQ_LENGTH 16

#define MIN_NOTE_DUR -3
#define MAX_NOTE_DUR 4
#define DEFAULT_NOTE_DUR 2

#define CVRANGE_MAX 8
enum CVRange{
	Bipolar_10,
	Bipolar_5,
	Bipolar_3,
	Bipolar_1,
	Unipolar_10,
	Unipolar_5,
	Unipolar_3,
	Unipolar_1,
};

const std::string CVRange_Lables [] = {
	"+/-10V",
	"+/-5V",
	"+/-3V",
	"+/-1V",
	"0V-10V",
	"0V-5V",
	"0V-3V",
	"0V-1V",
};

float mapCVRange(float in, CVRange range){
	switch(range){
		case Bipolar_10:
			return in * 20.f - 10.f;
		case Bipolar_5:
			return in * 10.f - 5.f;
		case Bipolar_3:
			return in * 6.f - 3.f;
		case Bipolar_1:
			return in * 2.f - 1.f;
		case Unipolar_10:
			return in * 10.f;
		case Unipolar_5:
			return in * 5.f;
		case Unipolar_3:
			return in * 3.f;
		case Unipolar_1:
			return in * 1.f;
	}
	return 0;
}

float invMapCVRange(float in, CVRange range){
	switch(range){
		case Bipolar_10:
			return (in + 10.f) / 10.f;
		case Bipolar_5:
			return (in + 5.f) / 10.f;
		case Bipolar_3:
			return (in + 3.f) / 6.f;
		case Bipolar_1:
			return (in + 1.f) / 2.f;
		case Unipolar_10:
			return in / 10.f;
		case Unipolar_5:
			return in / 5.f;
		case Unipolar_3:
			return in / 3.f;
		case Unipolar_1:
			return in / 1.f;
	}
	return 0;
}



struct Sequencer1 : Module {
	enum ParamId {
		ENUMS(MAIN_SEQ_NOTE_CV_PARAM, MAX_SEQ_LENGTH),
		ENUMS(MAIN_SEQ_DURATION_PARAM, MAX_SEQ_LENGTH),
		SEQ_LENGTH_PARAM,
		EVOLUTION_LENGTH_PARAM,
		CYCLES_PER_EVOLUTION_PARAM,
		DURATION_EVOLUTION_CHANCE_PARAM,
		FULL_LENGTH_EVOLUTION_PARAM,
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

	struct CVRangeParamQuantity : ParamQuantity  {
		float getDisplayValue() override {
			float v = getValue();
			Sequencer1* snModule = dynamic_cast<Sequencer1*>(module);
			return mapCVRange(v,snModule->range);
		}
		void setDisplayValue(float v) override {
			Sequencer1* snModule = dynamic_cast<Sequencer1*>(module);
			setValue(invMapCVRange(v,snModule->range));
		}
	};

	//Persistant State

	int currentStep;
	int currentEvolvedStep;
	int currentDur;
	bool muted;
	int cyclesToEvolve;
	int evolutionCount;
	bool evolvingUp;
	int evolutionMapping [MAX_SEQ_LENGTH];
	bool evolveDur;

	//Non Persistant State

	bool clockHigh = false;
	bool resetHigh = false;

	CVRange range = Bipolar_3;

	Sequencer1() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		configInput(CLOCK_INPUT,"Clock");
		configInput(RESET_INPUT,"Reset");
		configOutput(GATE_OUTPUT,"Gate");
		configOutput(CV_OUTPUT,"CV");

		configParam(SEQ_LENGTH_PARAM, 1, MAX_SEQ_LENGTH, 8, "Sequence Length");
		configParam(EVOLUTION_LENGTH_PARAM, 0, MAX_SEQ_LENGTH, 0, "Evolution Length");
		configParam(CYCLES_PER_EVOLUTION_PARAM, 1, MAX_SEQ_LENGTH, 1, "Cycles Per Evlolution");
		configParam(DURATION_EVOLUTION_CHANCE_PARAM, 0, 1, 0, "Duration Evolution Chance");
		configSwitch(FULL_LENGTH_EVOLUTION_PARAM, 0, 1, 0, "Evolve uses Full Length", std::vector<std::string>{"No","Yes"});

		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){

			std::string str_ni = std::to_string(ni+1);

			configParam<CVRangeParamQuantity>(MAIN_SEQ_NOTE_CV_PARAM + ni, 0.f, 1.f, 0.5f, "CV " + str_ni, "V");
			configParam(MAIN_SEQ_DURATION_PARAM + ni, MIN_NOTE_DUR, MAX_NOTE_DUR, DEFAULT_NOTE_DUR, "Duration " + str_ni);

		}
		initalize();
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		initalize();
	}

	void initalize(){
		range = Bipolar_3;
		clockHigh = false;

		currentStep = -1;
		currentEvolvedStep = -1;
		currentDur = 0;

		muted = false;

		cyclesToEvolve = 0;
		evolutionCount = 0;
		evolvingUp = true;
		evolveDur = false;
		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++) evolutionMapping[ni] = -1;
	}

	void process(const ProcessArgs& args) override {
		//Reset Logic
		if(schmittTrigger(resetHigh,inputs[RESET_INPUT].getVoltage())){
			currentStep = -1;
			currentEvolvedStep = -1;
			currentDur = 0;

			cyclesToEvolve = 0;
			evolutionCount = 0;
			evolvingUp = true;
			evolveDur = false;
			for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++) evolutionMapping[ni] = -1;
		}

		int maxStep = params[SEQ_LENGTH_PARAM].getValue();

		//Clock Logic
		if(schmittTrigger(clockHigh,inputs[CLOCK_INPUT].getVoltage())){
			if(currentDur > 1){
				currentDur--;
			}else{
				currentStep++;				
				if(currentStep >= maxStep){
					currentStep = 0;
					//TODO EOC Logic

					evolveDur = rack::random::uniform() < params[DURATION_EVOLUTION_CHANCE_PARAM].getValue();

					if(cyclesToEvolve > 1){
						cyclesToEvolve --;
					}else{
						cyclesToEvolve = params[CYCLES_PER_EVOLUTION_PARAM].getValue();
						int maxEvolution = params[EVOLUTION_LENGTH_PARAM].getValue();
						if(evolutionCount >= maxEvolution){
							evolvingUp = false;
						}else if(evolutionCount <= 0){
							evolvingUp = true;
						}
						
						if(evolvingUp){
							evolutionCount++;
							std::vector<int> indexes;
							for(int ni = 0; ni < maxStep; ni++){
								if(evolutionMapping[ni] == -1) indexes.push_back(ni);
							}
							if(indexes.size() > 0){
								int index = indexes[std::floor(rack::random::uniform() * indexes.size())];
								bool fullLength = params[FULL_LENGTH_EVOLUTION_PARAM].getValue() == 1;
								int maxRnd = fullLength ? MAX_SEQ_LENGTH : maxStep;
								evolutionMapping[index] = std::floor(rack::random::uniform() * maxRnd);
							}
						}else{
							evolutionCount--;
							if(evolutionCount <= maxStep){
								std::vector<int> indexes;
								for(int ni = 0; ni < maxStep; ni++){
									if(evolutionMapping[ni] != -1) indexes.push_back(ni);
								}
								if(indexes.size() > 0){
									int index = indexes[std::floor(rack::random::uniform() * indexes.size())];
									evolutionMapping[index] = -1;
								}
							}
						}
					}
				}

				currentEvolvedStep = evolutionMapping[currentStep];
				if(currentEvolvedStep == -1) currentEvolvedStep = currentStep;

				//Use Duration from Current Step
				int dur = params[MAIN_SEQ_DURATION_PARAM + (evolveDur ? currentEvolvedStep : currentStep)].getValue();
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
			float val = params[MAIN_SEQ_NOTE_CV_PARAM + currentEvolvedStep].getValue();
			float cv = mapCVRange(val,range);
			outputs[CV_OUTPUT].setVoltage(cv);
			outputs[GATE_OUTPUT].setVoltage((clockHigh || currentDur > 1) ? 10 : 0);
		}

		//Update Lights
		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){
			lights[MAIN_SEQ_ACTIVE_LIGHT + ni * 3 + 0].setBrightness((evolutionMapping[ni] != -1 && currentStep == ni) ? 1 : 0);
			lights[MAIN_SEQ_ACTIVE_LIGHT + ni * 3 + 1].setBrightness((ni < maxStep && evolutionMapping[ni] != -1)? 1 : 0);
			lights[MAIN_SEQ_ACTIVE_LIGHT + ni * 3 + 2].setBrightness((currentStep == ni || currentEvolvedStep == ni) ? 1 : 0);
		}
	}
};


struct Sequencer1Widget : ModuleWidget {
	Sequencer1Widget(Sequencer1* module) {
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

		addInput(createInputCentered<PJ301MPort>(Vec(x,y), module, Sequencer1::CLOCK_INPUT));
		y += dy;
		addInput(createInputCentered<PJ301MPort>(Vec(x,y), module, Sequencer1::RESET_INPUT));
		y += dy;
		addOutput(createOutputCentered<PJ3410Port>(Vec(x,y), module, Sequencer1::GATE_OUTPUT));
		y += dy;
		addOutput(createOutputCentered<PJ3410Port>(Vec(x,y), module, Sequencer1::CV_OUTPUT));
		y += dy;
		addParam(createParamCentered<RotarySwitch<RoundSmallBlackKnob>>(Vec(x,y), module, Sequencer1::SEQ_LENGTH_PARAM));
		y += dy;
		addParam(createParamCentered<RotarySwitch<RoundSmallBlackKnob>>(Vec(x,y), module, Sequencer1::CYCLES_PER_EVOLUTION_PARAM));
		y += dy;
		addParam(createParamCentered<RotarySwitch<RoundSmallBlackKnob>>(Vec(x,y), module, Sequencer1::EVOLUTION_LENGTH_PARAM));
		y += dy;
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x,y), module, Sequencer1::DURATION_EVOLUTION_CHANCE_PARAM));
		y += dy;
		addParam(createParamCentered<CKSS>(Vec(x,y), module, Sequencer1::FULL_LENGTH_EVOLUTION_PARAM));
		

		x += dx * 2;
		y = yStart;
		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){			

			addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x,y), module, Sequencer1::MAIN_SEQ_NOTE_CV_PARAM + ni));
			addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(Vec(x + dx*0.55f, y), module, Sequencer1::MAIN_SEQ_ACTIVE_LIGHT + ni * 3));
			addParam(createParamCentered<RotarySwitch<Trimpot>>(Vec(x + dx,y), module, Sequencer1::MAIN_SEQ_DURATION_PARAM + ni));

			y += dy;

			if(ni == 7){
				y = yStart;
				x += dx * 3;
			}
		}
	}

	void appendContextMenu(Menu* menu) override {
		Sequencer1* module = dynamic_cast<Sequencer1*>(this->module);

		menu->addChild(new MenuEntry); //Blank Row
		menu->addChild(createMenuLabel("Sequencer1"));
		
		menu->addChild(createSubmenuItem("Range", CVRange_Lables[module->range],
			[=](Menu* menu) {
				for(int i = 0; i < CVRANGE_MAX; i++){
					menu->addChild(createMenuItem(CVRange_Lables[i], CHECKMARK(module->range == i), [module,i]() { 
						module->range = (CVRange)i;
					}));
				}
			}
		));
	}
};


Model* modelSequencer1 = createModel<Sequencer1, Sequencer1Widget>("Sequencer1");