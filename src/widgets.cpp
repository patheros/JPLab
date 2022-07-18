#include "widgets.hpp"

std::vector<std::string> SUBDIVISION_LABELS = {"One","Two","Three (QQ-H)","Three (Q-H-Q)","Three (H-QQ)","Tripplets","Four"};

void configNoteBlock(Module * module, int paramIndex){
	module->configSwitch(paramIndex, 1, 7, 1, "Subdivisions", SUBDIVISION_LABELS);
	for(int i = 0; i < 4; i++){
		//Note CV
		module->configParam(paramIndex + i * 2 + 1, NoteEntryWidget_MIN, NoteEntryWidget_MAX, 0.0f, "CV", "V");
		//Note Extra
		auto q = module->configParam(paramIndex + i * 2 + 2, NE_NONE, NE_MUTE, NE_NONE, "");
		q->snapEnabled = true;
		if(i == 0) q->maxValue = NE_TIE; //Only allow 1st note in block to be randomized to Tie
	}
}
