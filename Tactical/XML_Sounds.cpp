#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include <string.h>
#include "sgp.h"
#include "Sound Control.h"
#include "Debug Control.h"
#include "expat.h"
#include "XML.h"

extern char szSoundEffects[MAX_SAMPLES][255];

struct
{
    PARSE_STAGE     curElement;
    CHAR8           szCharData[MAX_CHAR_DATA_LENGTH+1];
    UINT32          maxArraySize;
    UINT32          curIndex;
    UINT32          currentDepth;
    UINT32          maxReadDepth;
}
typedef soundParseData;

static void XMLCALL
soundStartElementHandle(void *userData, const XML_Char *name, const XML_Char **atts)
{
    soundParseData * pData = (soundParseData *)userData;

    if(pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
    {
        if(strcmp(name, "SOUNDLIST") == 0 && pData->curElement == ELEMENT_NONE)
        {
            pData->curElement = ELEMENT_LIST;

            pData->maxReadDepth++; //we are not skipping this element
        }
        else if(strcmp(name, "SOUND") == 0 && pData->curElement == ELEMENT_LIST)
        {
            pData->curElement = ELEMENT;

            pData->maxReadDepth++; //we are not skipping this element
            pData->curIndex++;
        }

        pData->szCharData[0] = '\0';
    }

    pData->currentDepth++;

}

static void XMLCALL
soundCharacterDataHandle(void *userData, const XML_Char *str, int len)
{
    soundParseData * pData = (soundParseData *)userData;

    if((pData->currentDepth <= pData->maxReadDepth) && (strlen(pData->szCharData) < MAX_CHAR_DATA_LENGTH))
    {
        strncat(pData->szCharData,str,__min((unsigned int)len,MAX_CHAR_DATA_LENGTH-strlen(pData->szCharData)));
    }
}


static void XMLCALL
soundEndElementHandle(void *userData, const XML_Char *name)
{
    soundParseData * pData = (soundParseData *)userData;

    if(pData->currentDepth <= pData->maxReadDepth) //we're at the end of an element that we've been reading
    {
        if(strcmp(name, "SOUNDLIST") == 0)
        {
            pData->curElement = ELEMENT_NONE;
        }
        else if(strcmp(name, "SOUND") == 0)
        {
            pData->curElement = ELEMENT_LIST;

            if(pData->curIndex < pData->maxArraySize)
            {
                char temp;
                for(int i=0;i<min((int)strlen(pData->szCharData),254);i++)
                {
                    temp = pData->szCharData[i];
                    szSoundEffects[pData->curIndex][i] = temp; szSoundEffects[pData->curIndex][i+1] = '\0';
                }
            }
        }
        pData->maxReadDepth--;
    }
    pData->currentDepth--;
}

BOOLEAN ReadInSoundArray(STR fileName)
{
    soundParseData pData;
    DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("Loading %s",SOUNDSFILENAME ) );

    memset(&pData,0,sizeof(pData));
    pData.maxArraySize = MAX_SAMPLES;
    pData.curIndex = -1;
	const LegacyXmlCallbacks callbacks{
		&pData, soundStartElementHandle, soundEndElementHandle,
		soundCharacterDataHandle};
	const LegacyXmlResult result =
		ParseLegacyXmlFile(fileName, callbacks);
	if (!result)
	{
		if (result.status != LegacyXmlStatus::NotFound &&
			result.status != LegacyXmlStatus::ReadError)
		{
			const auto message = FormatLegacyXmlFailure(fileName, result);
			LiveMessage(message.data());
		}
		return FALSE;
	}
    return( TRUE );
}

BOOLEAN WriteSoundArray()
{
    HWFILE		hFile;
    DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("WriteSoundArray"));
    //Debug code; make sure that what we got from the file is the same as what's there
    // Open a new file
    hFile = FileOpen( "TABLEDATA\\Sounds out.xml", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS, FALSE );
    if ( !hFile )
        return( FALSE );
    UINT32 cnt;

    FilePrintf(hFile,"<SOUNDLIST>\r\n");
    for(cnt = 0;cnt < NUM_SAMPLES;cnt++)
    {
        FilePrintf(hFile,"\t<SOUND>");
        CHAR8 *szRemainder = szSoundEffects[cnt]; //the remaining string to be output (for making valid XML)
        while(szRemainder[0] != '\0')
        {
            UINT32 uiCharLoc = strcspn(szRemainder,"&<>\'\"\0");
            char invChar = szRemainder[uiCharLoc];

            if(uiCharLoc)
            {
                szRemainder[uiCharLoc] = '\0';
                FilePrintf(hFile,"%s",szRemainder);
                szRemainder[uiCharLoc] = invChar;
            }
            szRemainder += uiCharLoc;
            switch(invChar)
            {
                case '&':
                    FilePrintf(hFile,"&amp;");
                    szRemainder++;
                    break;

                case '<':
                    FilePrintf(hFile,"&lt;");
                    szRemainder++;
                    break;

                case '>':
                    FilePrintf(hFile,"&gt;");
                    szRemainder++;
                    break;

                case '\'':
                    FilePrintf(hFile,"&apos;");
                    szRemainder++;
                    break;

                case '\"':
                    FilePrintf(hFile,"&quot;");
                    szRemainder++;
                    break;
            }
        }
        FilePrintf(hFile,"</SOUND>\r\n");
    }
    FilePrintf(hFile,"</SOUNDLIST>\r\n");
    FileClose( hFile );
    return( TRUE );
}

