#include "plugin.hpp"
#include "util.hpp"
#include "cvRange.hpp"

#define MAX_SEQ_LENGTH 16

#define MIN_NOTE_DUR -3
#define MAX_NOTE_DUR 4
#define DEFAULT_NOTE_DUR 2

struct Sequencer1 : Module {
	enum ParamId {
		ENUMS(MAIN_SEQ_NOTE_CV_PARAM, MAX_SEQ_LENGTH),
		ENUMS(MAIN_SEQ_DURATION_PARAM, MAX_SEQ_LENGTH),
		SEQ_LENGTH_PARAM,
		EVOLUTION_LENGTH_PARAM,
		CYCLES_PER_EVOLUTION_PARAM,
		DURATION_EVOLUTION_CHANCE_PARAM,
		FULL_LENGTH_EVOLUTION_PARAM,
		LENGTH_MODE_PARAM,
		DEEVOLUTION_MODE_PARAM,
		RATCHET_CHANCE_PARAM,
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
	int currentEvolvedStep;
	int currentDur;
	bool muted;
	bool ratcheting;
	int cyclesToEvolve;
	int evolutionCount;
	bool evolvingUp;
	int evolutionMapping [MAX_SEQ_LENGTH];
	float evolutionRatcheting [MAX_SEQ_LENGTH];
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

		configParam(SEQ_LENGTH_PARAM, 1, MAX_SEQ_LENGTH * 4, 8, "Sequence Length");
		configParam(EVOLUTION_LENGTH_PARAM, 0, MAX_SEQ_LENGTH, 0, "Evolution Length");
		configParam(CYCLES_PER_EVOLUTION_PARAM, 1, MAX_SEQ_LENGTH, 1, "Cycles Per Evlolution");
		configParam(DURATION_EVOLUTION_CHANCE_PARAM, 0, 1, 0, "Duration Evolution Chance");
		configSwitch(FULL_LENGTH_EVOLUTION_PARAM, 0, 1, 0, "Evolve uses Full Length", std::vector<std::string>{"No","Yes"});
		configSwitch(LENGTH_MODE_PARAM, 0, 1, 0, "Length Mode", std::vector<std::string>{"Beats","Steps"});
		configSwitch(DEEVOLUTION_MODE_PARAM, 0, 1, 0, "De-Evolution Mode", std::vector<std::string>{"Ping-Pong","Instant"});
		configParam(RATCHET_CHANCE_PARAM, 0, 1, 0, "Ratchet Chance on Evolution");
		

		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){

			std::string str_ni = std::to_string(ni+1);

			configParam<CVRangeParamQuantity<Sequencer1>>(MAIN_SEQ_NOTE_CV_PARAM + ni, 0.f, 1.f, 0.5f, "CV " + str_ni, "V");
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
		currentBeat = -1;
		currentEvolvedStep = -1;
		currentDur = 0;

		muted = false;
		ratcheting = false;

		cyclesToEvolve = 0;
		evolutionCount = 0;
		evolvingUp = true;
		evolveDur = false;
		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){
			evolutionMapping[ni] = -1;
			evolutionRatcheting[ni] = 0;
		}
	}

	void process(const ProcessArgs& args) override {
		//Reset Logic
		if(schmittTrigger(resetHigh,inputs[RESET_INPUT].getVoltage())){
			currentStep = -1;
			currentBeat = -1;
			currentEvolvedStep = -1;
			currentDur = 0;

			muted = false;
			ratcheting = false;

			cyclesToEvolve = 0;
			evolutionCount = 0;
			evolvingUp = true;
			evolveDur = false;
			for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){
				evolutionMapping[ni] = -1;
				evolutionRatcheting[ni] = 0;
			}
		}

		//Clock Logic
		if(schmittTrigger(clockHigh,inputs[CLOCK_INPUT].getVoltage())){
			int maxStep = params[SEQ_LENGTH_PARAM].getValue();

			bool maxStepInBeats = params[LENGTH_MODE_PARAM].getValue() == 0;

			bool resetCycle = false;
			bool doNextStep = false;

			currentBeat++;
			if(maxStepInBeats){
				if(currentBeat >= maxStep) resetCycle = true;	
			}

			if(currentDur > 1){
				currentDur--;
			}else{
				currentStep++;	
				doNextStep = true;
				if(!maxStepInBeats){
					if(currentStep >= maxStep/4) resetCycle = true;
				}
			}					

			if(resetCycle){
				int maxStepReached = currentStep;
				currentStep = 0;
				currentBeat = 0;

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
					
					if(evolvingUp && evolutionCount < maxEvolution){
						evolutionCount++;
						std::vector<int> indexes;
						for(int ni = 0; ni < maxStepReached; ni++){
							if(evolutionMapping[ni] == -1) indexes.push_back(ni);
						}
						if(indexes.size() > 0){
							int index = indexes[std::floor(rack::random::uniform() * indexes.size())];
							bool fullLength = params[FULL_LENGTH_EVOLUTION_PARAM].getValue() == 1;
							int maxRnd = fullLength ? MAX_SEQ_LENGTH : maxStepReached;
							evolutionMapping[index] = std::floor(rack::random::uniform() * maxRnd);
							evolutionRatcheting[index] = rack::random::uniform(); 
						}
					}else if(evolutionCount > 0){
						if(params[DEEVOLUTION_MODE_PARAM].getValue() == 1){
							//Instant De-evolve
							cyclesToEvolve = 0;
							evolutionCount = 0;
							evolvingUp = true;
							evolveDur = false;
							for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){
								evolutionMapping[ni] = -1;
								evolutionRatcheting[ni] = 0;
							}
						}else{
							//Slow De-evolve
							evolutionCount--;
							if(evolutionCount <= maxStepReached){
								std::vector<int> indexes;
								for(int ni = 0; ni < maxStepReached; ni++){
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
			}

			if(doNextStep){
				currentEvolvedStep = evolutionMapping[currentStep % MAX_SEQ_LENGTH];
				float ratchetRnd = evolutionRatcheting[currentStep % MAX_SEQ_LENGTH];
				if(currentEvolvedStep == -1){
					currentEvolvedStep = currentStep % MAX_SEQ_LENGTH;
					ratcheting = false;
				}else{
					ratcheting = ratchetRnd < params[RATCHET_CHANCE_PARAM].getValue();
				}

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
			outputs[GATE_OUTPUT].setVoltage((clockHigh || (!ratcheting && currentDur > 1)) ? 10 : 0);
		}

		//Update Lights
		for(int ni = 0; ni < MAX_SEQ_LENGTH; ni++){
			lights[MAIN_SEQ_ACTIVE_LIGHT + ni * 3 + 0].setBrightness((evolutionMapping[ni] != -1 && currentStep == ni) ? 1 : 0);
			lights[MAIN_SEQ_ACTIVE_LIGHT + ni * 3 + 1].setBrightness((evolutionMapping[ni] != -1)? 1 : 0);
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
		y += dy;
		addParam(createParamCentered<CKSS>(Vec(x,y), module, Sequencer1::LENGTH_MODE_PARAM));
		y += dy;
		addParam(createParamCentered<CKSS>(Vec(x,y), module, Sequencer1::DEEVOLUTION_MODE_PARAM));	
		y += dy;
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(x,y), module, Sequencer1::RATCHET_CHANCE_PARAM));
			
		

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
		
		addRangeSelectMenu<Sequencer1>(module,menu);
	}
};


Model* modelSequencer1 = createModel<Sequencer1, Sequencer1Widget>("Sequencer1");