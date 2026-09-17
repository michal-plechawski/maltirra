// Altirra core OS helpers for macOS

#include <algorithm>
#include <vector>

#include <grp.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "oshelper.h"

bool ATIsUserAdministrator() {
	const group *adminGroup = getgrnam("admin");
	if (!adminGroup)
		return false;

	const gid_t adminGroupId = adminGroup->gr_gid;
	if (getgid() == adminGroupId || getegid() == adminGroupId)
		return true;

	const int groupCount = getgroups(0, nullptr);
	if (groupCount <= 0)
		return false;

	std::vector<gid_t> groups(static_cast<size_t>(groupCount));
	const int groupsRead = getgroups(groupCount, groups.data());
	if (groupsRead <= 0)
		return false;

	return std::find(groups.begin(), groups.begin() + groupsRead, adminGroupId) != groups.begin() + groupsRead;
}

void ATGenerateGuid(uint8 guid[16]) {
	uuid_generate_random(guid);
}
