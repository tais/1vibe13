#include "ImportStrings.h"
#include "LocalizedStrings.h"

#include <vfs/Tools/vfs_tools.h>
#include <vfs/Core/vfs.h>

#include <iomanip>

void Loc::ImportStrings()
{
	Loc::AssociateWithFile(Loc::AIM_BIOGRAPHY,	L"Localization/AimBiographies.xml");
	Loc::AssociateWithFile(Loc::AIM_HISTORY,	L"Localization/AimHistory.xml");
	Loc::AssociateWithFile(Loc::AIM_POLICY,		L"Localization/AimPolicy.xml");
	Loc::AssociateWithFile(Loc::GAME_STRINGS,	L"Localization/GameStrings.xml");

	for(int i=0; i<200; ++i)
	{
		std::wstringstream wss;
		wss << std::setfill(L'0') << std::setw(3) << i;
		vfs::String s = wss.str() + L".EDT.xml";
		vfs::Path filename(L"Localization/Dialogue");
		filename += vfs::Path(s);
		if(getVFS()->fileExists(filename))
		{
			Loc::AssociateWithFile(Loc::DIALOGUE,filename,vfs::toString<wchar_t>(i));
		}
	}
}
