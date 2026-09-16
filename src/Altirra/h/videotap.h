// Altirra - Atari video frame tap interface

#ifndef f_AT_VIDEOTAP_H
#define f_AT_VIDEOTAP_H

#include <vd2/system/vdtypes.h>

struct VDPixmap;

class IATGTIAVideoTap {
public:
	virtual void WriteFrame(const VDPixmap& px, uint64 timestampStart, uint64 timestampEnd, float par) = 0;
};

#endif
