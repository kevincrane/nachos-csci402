#ifndef IPTENTRY_H
#define IPTENTRY_H

#include "../machine/translate.h"

class IPTEntry : public TranslationEntry {

public:
	int processID;
	int timeStamp;
	int location;
	
	int pageLoc;
	int swapByteOffset;
	int inUse;
	
	IPTEntry() {
	  timeStamp = 0;
	}
	
};

#endif //IPTENTRY_H
