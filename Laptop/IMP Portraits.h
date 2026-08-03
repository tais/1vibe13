#ifndef __IMP_PORTRAIT_H
#define __IMP_PORTRAIT_H

void EnterIMPPortraits( void );
void RenderIMPPortraits( void );
void ExitIMPPortraits( void );
void HandleIMPPortraits( void );
BOOLEAN RenderPortrait( INT16 sX, INT16 sY );
BOOLEAN IsValidSelectedIMPPortrait(INT32 portraitIndex);
BOOLEAN IsSelectableIMPPortraitForGender(INT32 portraitIndex, BOOLEAN isMale);

extern INT32 iPortraitNumber;


#endif
