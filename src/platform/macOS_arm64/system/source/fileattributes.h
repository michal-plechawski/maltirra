// Preserve the DOS archive attribute on native macOS filesystems.
#ifndef f_VD_MACOS_FILEATTRIBUTES_H
#define f_VD_MACOS_FILEATTRIBUTES_H
#include <sys/xattr.h>
namespace VDMacFileAttributes {
	inline constexpr char kArchiveAttribute[]="org.altirra.archive";
	inline bool Archive(const char *path,bool regular) {
		unsigned char value=0;
		return getxattr(path,kArchiveAttribute,&value,1,0,0)==1 ? value!=0 : regular;
	}
	inline bool Archive(int descriptor,bool regular) {
		unsigned char value=0;
		return fgetxattr(descriptor,kArchiveAttribute,&value,1,0,0)==1 ? value!=0 : regular;
	}
	inline void MarkWritten(int descriptor) {
		// The default for a regular file is archived. Removing a previous clear
		// also works on volumes without xattrs, where the default still applies.
		(void)fremovexattr(descriptor,kArchiveAttribute,0);
	}
}
#endif
