#include <fstream>
#include <unistd.h>
#include <chrono>
#include <iostream>

using namespace std;

void getEncoderValues(int &leftEncoder, int &rightEncoder){
	fstream inFile;
	inFile.open("/sys/ssi/stream");

	int i = 0;
	int x;
	
	if (!inFile) {
		printf("Could not ready from encoders. Is robot_ssi module loaded and configured?\n");
		return;
	}
	else
	{

	while (inFile >> x){
		switch(i){
		case 0:
			leftEncoder = x;
			break;
		case 1:
			rightEncoder = x;
			break;
		default:
			printf("Too many items in ssi stream, is it configured to 64 bits?");
			break;
		}
		i++;
	}

	if(i != 2){
		printf("Not enough items in ssi stream, is it configured to 64 bits");
	}
	}

	inFile.close();
}

int main(){
	int left, right;
	while(true){
		chrono::steady_clock::time_point begin = chrono::steady_clock::now();
		getEncoderValues(left, right);
		chrono::steady_clock::time_point end = chrono::steady_clock::now();

		std::cout << "Time difference = " << chrono::duration_cast<chrono::microseconds>(end - begin).count() << "[µs]" << endl;
		//std::cout << "Time difference = " << chrono::duration_cast<chrono::nanoseconds> (end - begin).count() << "[ns]" << endl;
		printf("L: %d\tR: %d\r", left, right);
		usleep(1000000);
	}
	return 0;
}
