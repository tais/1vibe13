	#include "types.h"
	#include "Music Control.h"
	#include "soundman.h"
	#include "random.h"
	#include "jascreens.h"
	#include "Overhead.h"
	#include "Timer Control.h"
	#include "strategicmap.h"
	#include "MediaLifecycleModel.h"

	#include "Overhead Types.h"
	#include <Game Clock.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

static UINT32 uiMusicHandle = NO_SAMPLE;
static BOOLEAN fMusicPlaying = FALSE;

static BOOLEAN fMusicFadingOut = FALSE;
static BOOLEAN fMusicFadingIn = FALSE;
static UINT32 uiMusicVolume = 50;

static BOOLEAN gfMusicEnded = FALSE;

static UINT8 gubMusicMode = 0;

static UINT8 gubOldMusicMode = 0;

static INT8 gbVictorySongCount = 0;
static INT8 gbDeathSongCount = 0;

static BOOLEAN gfUseCreatureMusic = FALSE;

static INT8 gbFadeSpeed = 1;

static BOOLEAN gfDontRestartSong = FALSE;

static MediaLifecycleModel::PlaybackEpoch gMusicPlayback;


// Original music and their hardcoded paths
enum MusicList
{
	MARIMBAD2_MUSIC,
	MENUMIX_MUSIC,
	NOTHING_A_MUSIC,
	NOTHING_B_MUSIC,
	NOTHING_C_MUSIC,
	NOTHING_D_MUSIC,
	TENSOR_A_MUSIC,
	TENSOR_B_MUSIC,
	TENSOR_C_MUSIC,
	TRIUMPH_MUSIC,
	DEATH_MUSIC,
	BATTLE_A_MUSIC,
	BATTLE_B_MUSIC, //same as tensor B
	CREEPY_MUSIC,
	CREATURE_BATTLE_MUSIC,
	MUSIC_DIR,
	NUM_MUSIC
};

const CHAR8 *szMusicList[NUM_MUSIC] =
{
	"MUSIC\\marimbad 2",
	"MUSIC\\menumix1",
	"MUSIC\\nothing A",
	"MUSIC\\nothing B",
	"MUSIC\\nothing C",
	"MUSIC\\nothing D",
	"MUSIC\\tensor A",
	"MUSIC\\tensor B",
	"MUSIC\\tensor C",
	"MUSIC\\triumph",
	"MUSIC\\death",
	"MUSIC\\battle A",
	"MUSIC\\battle B",
	"MUSIC\\creepy",
	"MUSIC\\creature battle",
	"MUSIC",
};


using MusicListEntries = std::array<std::vector<std::string>, MAX_MUSIC>;
static MusicListEntries gMusicLists;
static bool gMusicListsInitialized = false;
static BOOLEAN MusicStop(void);

static void AddMusicToList(
	MusicListEntries& lists, STR fileName, NewMusicList mode)
{
	UINT8 constexpr buf = 64;
	CHAR8 musicFile[buf];

	if (MediaLifecycleModel::IsValidIndex(
		lists.size(), static_cast<int>(mode)) && fileName &&
		SoundFileExists(fileName, musicFile, sizeof(musicFile)))
	{
		lists[mode].emplace_back(musicFile);
	}
}

void InitializeMusicLists()
{
	if (gMusicListsInitialized) return;
	MusicListEntries stagedLists;
	UINT8 constexpr buf = 64;
	CHAR8 fileName[buf];

	// Special casing original music so they can be easily replaced with new files
	AddMusicToList(stagedLists, szMusicList[MENUMIX_MUSIC], MUSICLIST_MAIN_MENU);
	AddMusicToList(stagedLists, szMusicList[MARIMBAD2_MUSIC], MUSICLIST_LAPTOP);

	AddMusicToList(stagedLists, szMusicList[NOTHING_A_MUSIC], MUSICLIST_TACTICAL_NOTHING);
	AddMusicToList(stagedLists, szMusicList[NOTHING_B_MUSIC], MUSICLIST_TACTICAL_NOTHING);
	AddMusicToList(stagedLists, szMusicList[NOTHING_C_MUSIC], MUSICLIST_TACTICAL_NOTHING);
	AddMusicToList(stagedLists, szMusicList[NOTHING_D_MUSIC], MUSICLIST_TACTICAL_NOTHING);

	AddMusicToList(stagedLists, szMusicList[TENSOR_A_MUSIC], MUSICLIST_TACTICAL_ENEMYPRESENT);
	AddMusicToList(stagedLists, szMusicList[TENSOR_B_MUSIC], MUSICLIST_TACTICAL_ENEMYPRESENT);
	AddMusicToList(stagedLists, szMusicList[TENSOR_C_MUSIC], MUSICLIST_TACTICAL_ENEMYPRESENT);

	AddMusicToList(stagedLists, szMusicList[BATTLE_A_MUSIC], MUSICLIST_TACTICAL_BATTLE);
	if (SoundFileExists(szMusicList[BATTLE_B_MUSIC], fileName, sizeof(fileName)))
	{
		AddMusicToList(stagedLists, szMusicList[BATTLE_B_MUSIC], MUSICLIST_TACTICAL_BATTLE);
	}
	else
	{
		AddMusicToList(stagedLists, szMusicList[TENSOR_B_MUSIC], MUSICLIST_TACTICAL_BATTLE);
	}

	AddMusicToList(stagedLists, szMusicList[TRIUMPH_MUSIC], MUSICLIST_TACTICAL_VICTORY);
	AddMusicToList(stagedLists, szMusicList[DEATH_MUSIC], MUSICLIST_TACTICAL_DEATH);
	AddMusicToList(stagedLists, szMusicList[CREEPY_MUSIC], MUSICLIST_TACTICAL_CREEPY);
	AddMusicToList(stagedLists, szMusicList[CREATURE_BATTLE_MUSIC], MUSICLIST_TACTICAL_CREEPY_BATTLE);


	// Read music files into list
	for (size_t j = MUSICLIST_MAIN_MENU; j < MAX_MUSIC; j++)
	{
		STR baseFilename;
		switch (j)
		{
		case MUSICLIST_MAIN_MENU:
			baseFilename = "MUSIC\\Mainmenu_";
			break;
		case MUSICLIST_LAPTOP:
			baseFilename = "MUSIC\\Laptop_";
			break;
		case MUSICLIST_TACTICAL_NOTHING:
			baseFilename = "MUSIC\\Tactical_";
			break;
		case MUSICLIST_TACTICAL_ENEMYPRESENT:
			baseFilename = "MUSIC\\Enemy_";
			break;
		case MUSICLIST_TACTICAL_BATTLE:
			baseFilename = "MUSIC\\Battle_";
			break;
		case MUSICLIST_TACTICAL_ENEMYPRESENT_NIGHT:
			baseFilename = "MUSIC\\EnemyNight_";
			break;
		case MUSICLIST_TACTICAL_BATTLE_NIGHT:
			baseFilename = "MUSIC\\BattleNight_";
			break;
		case MUSICLIST_TACTICAL_VICTORY:
			baseFilename = "MUSIC\\Victory_";
			break;
		case MUSICLIST_TACTICAL_DEATH:
			baseFilename = "MUSIC\\Death_";
			break;
		case MUSICLIST_TACTICAL_CREEPY:
			baseFilename = "MUSIC\\Creepy_";
			break;
		case MUSICLIST_TACTICAL_CREEPY_BATTLE:
			baseFilename = "MUSIC\\CreepyBattle_";
			break;
		default:
			break;
		}
		if (!baseFilename) continue;

		for (size_t i = 0; i < 100; i++)
		{
			snprintf(fileName, sizeof(fileName), "%s%02zu", baseFilename, i);
			AddMusicToList(stagedLists, fileName, static_cast<NewMusicList>(j));
		}
	}
	gMusicLists.swap(stagedLists);
	gMusicListsInitialized = true;
}

void ShutdownMusicLists()
{
	MusicStop();
	for (std::vector<std::string>& list : gMusicLists) list.clear();
	gMusicListsInitialized = false;
	fMusicPlaying = FALSE;
	fMusicFadingOut = FALSE;
	fMusicFadingIn = FALSE;
	gfMusicEnded = FALSE;
	gfDontRestartSong = FALSE;
	uiMusicHandle = NO_SAMPLE;
}

std::size_t MusicListSize(NewMusicList mode)
{
	return MediaLifecycleModel::IsValidIndex(
		gMusicLists.size(), static_cast<int>(mode))
		? gMusicLists[mode].size() : 0;
}

STR MusicListEntry(NewMusicList mode, std::size_t songIndex)
{
	if (!MediaLifecycleModel::IsValidIndex(
		gMusicLists.size(), static_cast<int>(mode)) ||
		!MediaLifecycleModel::IsValidIndex(gMusicLists[mode].size(), songIndex))
	{
		return nullptr;
	}
	return gMusicLists[mode][songIndex].c_str();
}

static STR PickRandomSongFromList(NewMusicList mode)
{
	if (!MediaLifecycleModel::IsValidIndex(
		gMusicLists.size(), static_cast<int>(mode)) ||
		gMusicLists[mode].empty())
	{
		return NULL;
	}
	return gMusicLists[mode][Random(gMusicLists[mode].size())].c_str();
}


BOOLEAN StartMusicBasedOnMode(void);
void DoneFadeOutDueToEndMusic(void);
static void MusicStopCallback(void *pData);
static BOOLEAN MusicStop(void);
BOOLEAN MusicFadeOut(void);
BOOLEAN MusicFadeIn(void);


//********************************************************************************
// MusicPlay
//
//		Starts up one of the tunes in the music list.
//
//	Returns:	TRUE if the music was started, FALSE if an error occurred
//
//********************************************************************************
static BOOLEAN MusicPlay(STR zFileName)
{
	if (!zFileName || !*zFileName)
	{
		return FALSE;
	}

	SOUNDPARMS spParms{};

	if (fMusicPlaying || uiMusicHandle != NO_SAMPLE)
		MusicStop();

	const MediaLifecycleModel::PlaybackEpoch::Token playbackToken =
		gMusicPlayback.begin();
	spParms.uiPriority = PRIORITY_MAX;
	spParms.uiVolume = 0;
	spParms.uiLoop = 1;
	spParms.uiPan = 64;
	spParms.EOSCallback = MusicStopCallback;
	spParms.pCallbackData = reinterpret_cast<void*>(playbackToken);

	uiMusicHandle = SoundPlayStreamedFile(zFileName, &spParms);
	if (uiMusicHandle != SOUND_ERROR)
	{

		gfMusicEnded = FALSE;
		fMusicPlaying = TRUE;
		MusicFadeIn();
		return TRUE;
	}

	gMusicPlayback.cancel();
	uiMusicHandle = NO_SAMPLE;
	fMusicPlaying = FALSE;
	fMusicFadingIn = FALSE;
	fMusicFadingOut = FALSE;
	return FALSE;
}

BOOLEAN MusicPlay(NewMusicList mode, UINT8 songIndex)
{
	const STR song = MusicListEntry(mode, songIndex);
	if (!song)
	{
		return FALSE;
	}

	return MusicPlay(song);
}

//********************************************************************************
// MusicSetVolume
//
//		Sets the volume on the currently playing music.
//
//	Returns:	TRUE if the volume was set, FALSE if an error occurred
//
//********************************************************************************
BOOLEAN MusicSetVolume(UINT32 uiVolume)
{
	INT32 uiOldMusicVolume = uiMusicVolume;

	// WANNE: We want music in windowed mode
	//if( 1==iScreenMode ) /* on Windowed mode, skip the music? was coded for WINDOWED_MODE that way...*/
	//return FALSE;

	uiMusicVolume = __min(uiVolume, 127);

	if(uiMusicHandle != NO_SAMPLE)
	{
		// get volume and if 0 stop music!
		if (uiMusicVolume == 0)
		{
			gfDontRestartSong = TRUE;
			MusicStop();
			return TRUE;
		}

		SoundSetVolume(uiMusicHandle, uiMusicVolume);

		return TRUE;
	}

	// If here, check if we need to re-start music
	// Have we re-started?
	if (uiMusicVolume > 0 && uiOldMusicVolume == 0)
	{
		StartMusicBasedOnMode();
	}

	return FALSE;
}

//********************************************************************************
// MusicGetVolume
//
//		Gets the volume on the currently playing music.
//
//	Returns:	TRUE if the volume was set, FALSE if an error occurred
//
//********************************************************************************
UINT32 MusicGetVolume(void)
{
	return uiMusicVolume;
}

//********************************************************************************
// MusicStop
//
//		Stops the currently playing music.
//
//	Returns:	TRUE if the music was stopped, FALSE if an error occurred
//
//********************************************************************************
static BOOLEAN MusicStop(void)
{
	// WANNE: We want music in windowed mode
	//if( 1==iScreenMode ) /* on Windowed mode, skip the music? was coded for WINDOWED_MODE that way...*/
	//	return(FALSE);

	const UINT32 handle = uiMusicHandle;
	gMusicPlayback.cancel();
	fMusicPlaying = FALSE;
	fMusicFadingOut = FALSE;
	fMusicFadingIn = FALSE;
	gfMusicEnded = FALSE;
	uiMusicHandle = NO_SAMPLE;
	if(handle != NO_SAMPLE)
	{
		//DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Music Stop %d %d", uiMusicHandle, gubMusicMode ) );

		SoundStop(handle);
		return TRUE;
	}

	//DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Music Stop %d %d", uiMusicHandle, gubMusicMode ) );
	return FALSE;
}

//********************************************************************************
// MusicFadeOut
//
//		Fades out the current song.
//
//	Returns:	TRUE if the music has begun fading, FALSE if an error occurred
//
//********************************************************************************
BOOLEAN MusicFadeOut(void)
{
	if(uiMusicHandle != NO_SAMPLE)
	{
		fMusicFadingOut = TRUE;
		return TRUE;
	}
	return FALSE;
}

//********************************************************************************
// MusicFadeIn
//
//		Fades in the current song.
//
//	Returns:	TRUE if the music has begun fading in, FALSE if an error occurred
//
//********************************************************************************
BOOLEAN MusicFadeIn(void)
{
	if(uiMusicHandle != NO_SAMPLE)
	{
		fMusicFadingIn = TRUE;
		return TRUE;
	}
	return FALSE;
}

//********************************************************************************
// MusicPoll
//
//		Handles any maintenance the music system needs done. Should be polled from
//	the main loop, or somewhere with a high frequency of calls.
//
//	Returns:	TRUE always
//
//********************************************************************************
BOOLEAN MusicPoll(BOOLEAN /*fForce*/)
{
	//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll");

	// WANNE: We want music in windowed mode
	//if( 1==iScreenMode ) /* on Windowed mode, skip the music? was coded for WINDOWED_MODE that way...*/
	//	return(TRUE);

	INT32 iVol;

	//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll: SoundServiceStreams ");
	SoundServiceStreams();
	//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll: SoundServiceRandom ");
	SoundServiceRandom();

	//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll: Handle Sound every sound overhead time");
	// Handle Sound every sound overhead time....
	if (COUNTERDONE(MUSICOVERHEAD))
	{
		//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll: Reset counter");
		// Reset counter
		RESETCOUNTER(MUSICOVERHEAD);

		if (fMusicFadingIn)
		{
			//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll: music fading in");
			if(uiMusicHandle != NO_SAMPLE)
			{
				iVol = SoundGetVolume(uiMusicHandle);
				iVol = __min( (INT32)uiMusicVolume, iVol+gbFadeSpeed );
				SoundSetVolume(uiMusicHandle, iVol);
				if(iVol == (INT32)uiMusicVolume)
				{
					fMusicFadingIn = FALSE;
					gbFadeSpeed = 1;
				}
			}
		}
		else if (fMusicFadingOut)
		{
			//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll: music fading out");
			if(uiMusicHandle != NO_SAMPLE)
			{
				iVol = SoundGetVolume(uiMusicHandle);
				iVol = (iVol >=1)? iVol-gbFadeSpeed : 0;

				iVol = __max( (INT32)iVol, 0 );

				SoundSetVolume(uiMusicHandle, iVol);
				if(iVol == 0)
				{
					MusicStop();
					fMusicFadingOut = FALSE;
					gbFadeSpeed = 1;
				}
			}
		}

		if (gfMusicEnded)
		{
			//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll: music ended");
			// OK, based on our music mode, play another!
			//DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Music End Loop %d %d", uiMusicHandle, gubMusicMode ) );

			// If we were in victory mode, change!
			if (gbVictorySongCount == 1 || gbDeathSongCount == 1)
			{
				if (gbDeathSongCount == 1 && GetCurrentScreen() == GAME_SCREEN)
				{
					CheckAndHandleUnloadingOfCurrentWorld();
				}

				if (gbVictorySongCount == 1)
				{
					SetMusicMode(MUSIC_TACTICAL_NOTHING);
				}
			}
			else
			{
				if (!gfDontRestartSong)
				{
					//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll: don't restart song, StartMusicBasedOnMode");
					StartMusicBasedOnMode();
				}
			}

			gfMusicEnded = FALSE;
			gfDontRestartSong = FALSE;
		}
	}

	//DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"MusicPoll done");
	return TRUE;
}

static BOOLEAN SetMusicMode(UINT8 ubMusicMode, BOOLEAN fForce)
{
	static INT8 bPreviousMode = 0;
	if (ubMusicMode > MUSIC_TACTICAL_CIV_GROUP_BATTLE) return FALSE;

	// OK, check if we want to restore
	if (ubMusicMode == MUSIC_RESTORE)
	{
		if (bPreviousMode == MUSIC_TACTICAL_VICTORY || bPreviousMode == MUSIC_TACTICAL_DEATH)
		{
			bPreviousMode = MUSIC_TACTICAL_NOTHING;
		}
		ubMusicMode = bPreviousMode;
	}
	else
	{
		// Save previous mode...
		bPreviousMode = gubOldMusicMode;
	}

	// if different, start a new music song
	if (fForce || gubOldMusicMode != ubMusicMode)
	{
		// Set mode....
		gubMusicMode = ubMusicMode;

		//DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Music New Mode %d %d", uiMusicHandle, gubMusicMode	) );

		gbVictorySongCount = 0;
		gbDeathSongCount = 0;

		if(uiMusicHandle != NO_SAMPLE)
		{
			// Fade out old music
			MusicFadeOut();
		}
		else
		{
			// Change music!
			StartMusicBasedOnMode();
		}
	}

	gubOldMusicMode = gubMusicMode;

	return TRUE;
}

BOOLEAN StartMusicBasedOnMode(void)
{
	switch(gubMusicMode)
	{
		case MUSIC_MAIN_MENU:
			// ATE: Don't fade in
			gbFadeSpeed = (INT8)uiMusicVolume;
			MusicPlay(PickRandomSongFromList(MUSICLIST_MAIN_MENU));
			break;

		case MUSIC_LAPTOP:
			gbFadeSpeed = (INT8)uiMusicVolume;
			MusicPlay(PickRandomSongFromList(MUSICLIST_LAPTOP));
			break;

		case MUSIC_TACTICAL_NOTHING:
			// ATE: Don't fade in
			gbFadeSpeed = (INT8)uiMusicVolume;
			if(gfUseCreatureMusic)
			{
				MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_CREEPY));
			}
			else
			{
				MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_NOTHING));
			}
			break;

		case MUSIC_TACTICAL_ENEMYPRESENT:
			// ATE: Don't fade in EnemyPresent...
			gbFadeSpeed = (INT8)uiMusicVolume;
			if(gfUseCreatureMusic)
			{
				MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_CREEPY));
			}
			else if (NightTime() && MusicListSize(MUSICLIST_TACTICAL_ENEMYPRESENT_NIGHT) > 0)
			{
				MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_ENEMYPRESENT_NIGHT));
			}
			else
			{
				MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_ENEMYPRESENT));
			}
			break;

		case MUSIC_TACTICAL_BATTLE:
			// ATE: Don't fade in
			gbFadeSpeed = (INT8)uiMusicVolume;
			if(gfUseCreatureMusic)
			{
				MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_CREEPY_BATTLE));
			}
			else if (NightTime() && MusicListSize(MUSICLIST_TACTICAL_BATTLE_NIGHT) > 0)
			{
				MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_BATTLE_NIGHT));
			}
			else
			{
				MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_BATTLE));
			}
			break;

		case MUSIC_TACTICAL_VICTORY:

			// ATE: Don't fade in EnemyPresent...
			gbFadeSpeed = (INT8)uiMusicVolume;
			MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_VICTORY));
			gbVictorySongCount++;

			if(gfUseCreatureMusic && !gbWorldSectorZ)
			{
				//We just killed all the creatures that just attacked the town.
				gfUseCreatureMusic = FALSE;
			}
			break;

		case MUSIC_TACTICAL_DEATH:

			// ATE: Don't fade in EnemyPresent...
			gbFadeSpeed = (INT8)uiMusicVolume;
			MusicPlay(PickRandomSongFromList(MUSICLIST_TACTICAL_DEATH));
			gbDeathSongCount++;
			break;

		default:
			MusicFadeOut();
			break;
	}

	return TRUE;
}


BOOLEAN SetMusicMode(UINT8 ubMusicMode)
{
	return SetMusicMode(ubMusicMode, FALSE);
}


static void MusicStopCallback(void *pData)
{
	//DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Music EndCallback %d %d", uiMusicHandle, gubMusicMode	) );

	const auto playbackToken = reinterpret_cast<
		MediaLifecycleModel::PlaybackEpoch::Token>(pData);
	if (!gMusicPlayback.accept(playbackToken)) return;

	gfMusicEnded = TRUE;
	fMusicPlaying = FALSE;
	fMusicFadingOut = FALSE;
	fMusicFadingIn = FALSE;
	uiMusicHandle = NO_SAMPLE;

	//DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Music EndCallback completed" );
}


void SetMusicFadeSpeed(INT8 bFadeSpeed)
{
	gbFadeSpeed = MediaLifecycleModel::ClampFadeSpeed(bFadeSpeed);
}

UINT8 GetMusicMode(void)
{
	return gubMusicMode;
}

BOOLEAN UsingCreatureMusic(void)
{
	return gfUseCreatureMusic;
}

void UseCreatureMusic(BOOLEAN fUseCreatureMusic)
{
	if (gfUseCreatureMusic != fUseCreatureMusic)
	{
		// this means a change
		gfUseCreatureMusic = fUseCreatureMusic;
		SetMusicMode(gubMusicMode, TRUE);	// same as before
	}
}

BOOLEAN IsMusicPlaying(void)
{
	return fMusicPlaying;
}

UINT32 GetMusicHandle(void)
{
	return uiMusicHandle;
}

// unused
//void FadeMusicForXSeconds( UINT32 uiDelay )
//{
//	INT16 sNumTimeSteps, sNumVolumeSteps;
//
//	// get # time steps in delay....
//	sNumTimeSteps = (INT16)( uiDelay / 10 );
//
//	// Devide this by music volume...
//	sNumVolumeSteps = (INT16)( uiMusicVolume / sNumTimeSteps );
//
//	// Set fade delay...
//	SetMusicFadeSpeed( (INT8)sNumVolumeSteps );
//}

// unused
//void	DoneFadeOutDueToEndMusic( void )
//{
//	// Quit game....
//	InternalLeaveTacticalScreen( MAINMENU_SCREEN );
//	//SetPendingNewScreen( MAINMENU_SCREEN );
//}


