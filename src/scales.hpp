#define NUM_OF_SCALES 12
static std::vector<int> SCALES[NUM_OF_SCALES] = {
	std::vector<int>({1,3,5,6,8,10,12}), //Major
	std::vector<int>({1,3,4,6,8,10,11}), //Dorian
	std::vector<int>({1,2,4,6,8,9,11}), //Phrygian
	std::vector<int>({1,3,5,7,8,10,12}), //Lydian
	std::vector<int>({1,3,5,6,8,10,11}), //Mixolydian
	std::vector<int>({1,3,4,6,8,9,11}), //Minor
	std::vector<int>({1,2,4,6,7,9,11}), //Locrian
	std::vector<int>({1,3,4,6,8,9,12}), //Harmonic Minor
	std::vector<int>({1,3,4,6,8,10,12}), //Melodic Minor
	std::vector<int>({1,3,5,8,10}), //Major Pentatonic
	std::vector<int>({1,4,6,8,11}), //Minor Pentatonic
	std::vector<int>({1,4,6,7,8,11}), //Blues
};
