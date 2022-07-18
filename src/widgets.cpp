#include "widgets.hpp"

std::vector<std::string> SUBDIVISION_LABELS = {"One","Two","Three (QQ-H)","Three (Q-H-Q)","Three (H-QQ)","Tripplets","Four"};

void configNoteBlock(Module * module, int paramIndex){
	module->configSwitch(paramIndex, 1, 7, 1, "Subdivisions", SUBDIVISION_LABELS);
	for(int i = 1; i <= 4; i++){
		module->configParam(paramIndex + i, NoteEntryWidget_MIN, NoteEntryWidget_MAX, 0.0f, "CV", "V");
	}
}
