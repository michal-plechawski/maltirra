// Shared description of emulated controller pointers.
#ifndef f_AT_INPUTPOINTER_H
#define f_AT_INPUTPOINTER_H
#include <vd2/system/vectors.h>
#include "inputdefs.h"

struct ATInputPointerInfo {
	vdfloat2 mPos {};		// center of touch in [-1,+1]
	float mRadius {};		// radius of touch, or <0 if point because device doesn't do area touches
	bool mbPrimary {};		// true if this is the primary touch driven by main position inputs
	bool mbPressed {};		// true if an active touch, false for hover position
	ATInputPointerCoordinateSpace mCoordSpace {};
};

#endif
