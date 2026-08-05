	#include "Text.h"
	#include "FileMan.h"
	#include "GameSettings.h"
	#include "TextInfrastructureModel.h"
	// sevenfm
	#include <algorithm>
	#include <locale>
	#include <string>
	#include <vector>
	#include <vfs/Core/vfs_string.h>

auto FormatMoney(INT32 iNumber) -> std::wstring
{
    static std::wstringstream wss([] {
        std::wstringstream ss;
        try {
            ss.imbue(std::locale("en_US.UTF-8"));
        }
        catch (const std::exception&) {
            ss.imbue(std::locale::classic());
        }
        return ss;
        }());
    wss.str(L"");
    wss << iNumber;

    return L"$" + wss.str();
}

BOOLEAN LoadItemInfo(UINT16 ubIndex, CHAR16 *pNameString, CHAR16 *pInfoString )
{
	if (pNameString) pNameString[0] = L'\0';
	if (pInfoString) pInfoString[0] = L'\0';
	if (!TextInfrastructureModel::IsValidIndex(
			std::min<std::size_t>(gMAXITEMS_READ, MAXITEMS), ubIndex))
		return FALSE;

	if (pNameString)
		TextInfrastructureModel::CopyBounded(
			pNameString, 80, Item[ubIndex].szLongItemName);

	if (pInfoString)
		TextInfrastructureModel::CopyBounded(
			pInfoString, 400, Item[ubIndex].szItemDesc);

	return(TRUE);
}

BOOLEAN LoadBRName(UINT16 ubIndex, CHAR16 *pNameString )
{
	if (pNameString) pNameString[0] = L'\0';
	if (!TextInfrastructureModel::IsValidIndex(
			std::min<std::size_t>(gMAXITEMS_READ, MAXITEMS), ubIndex))
		return FALSE;
	if (pNameString)
		TextInfrastructureModel::CopyBounded(
			pNameString, 80, Item[ubIndex].szBRName);
	return TRUE;
}

BOOLEAN LoadBRDesc(UINT16 ubIndex, CHAR16 *pDescString )
{
	if (pDescString) pDescString[0] = L'\0';
	if (!TextInfrastructureModel::IsValidIndex(
			std::min<std::size_t>(gMAXITEMS_READ, MAXITEMS), ubIndex))
		return FALSE;
	if (pDescString)
		TextInfrastructureModel::CopyBounded(
			pDescString, 400, Item[ubIndex].szBRDesc);

	return TRUE;
}

BOOLEAN LoadShortNameItemInfo(UINT16 ubIndex, CHAR16 *pNameString )
{
	if (pNameString) pNameString[0] = L'\0';
	if (!TextInfrastructureModel::IsValidIndex(
			std::min<std::size_t>(gMAXITEMS_READ, MAXITEMS), ubIndex))
		return FALSE;
	if (pNameString)
		TextInfrastructureModel::CopyBounded(
			pNameString, 80, Item[ubIndex].szItemName);

	return(TRUE);
}


void LoadAllItemNames( void )
{
	const std::size_t itemCount =
		std::min<std::size_t>(gMAXITEMS_READ, MAXITEMS);
	for (std::size_t itemIndex = 0; itemIndex < itemCount; ++itemIndex)
	{
		const UINT16 itemId = static_cast<UINT16>(itemIndex);
		LoadItemInfo(itemId, ItemNames[itemIndex], NULL);

		// Load short item info
		LoadShortNameItemInfo(itemId, ShortItemNames[itemIndex]);
	}
}

void LoadAllExternalText( void )
{
	LoadAllItemNames();
}

STR16 GetWeightUnitString( void )
{
	if ( gGameSettings.fOptions[ TOPTION_USE_METRIC_SYSTEM ] ) // metric
	{
		return( pMessageStrings[ MSG_KILOGRAM_ABBREVIATION ] );
	}
	else
	{
		return( pMessageStrings[ MSG_POUND_ABBREVIATION ] );
	}
}

FLOAT GetWeightBasedOnMetricOption( UINT32 uiObjectWeight )
{
	FLOAT fWeight = 0.0f;

	//if the user is smart and wants things displayed in 'metric'
	if ( gGameSettings.fOptions[ TOPTION_USE_METRIC_SYSTEM ] ) // metric
	{
		fWeight = (FLOAT)uiObjectWeight;
	}

	//else the user is a caveman and display it in pounds
	else
	{
		fWeight = uiObjectWeight * 2.2f;
	}

	return( fWeight );
}

int StringToEnum(const STR value, const Str8EnumLookupType *table)
{
	if (NULL == value || 0 == *value || NULL == table)
		return 0;

	for (const Str8EnumLookupType *itr = table; itr->name != NULL; ++itr) {
		if (0 == _stricmp(value, itr->name)) 
			return itr->value;
	}
	CHAR8 *end = NULL;
	return (int)strtol(value, &end, 0);
}

int StringToEnum(const STR8 value, const Str16EnumLookupType *table)
{
	if (NULL == value || 0 == *value || NULL == table)
		return 0;

	int result = 0;
	const size_t convertedLength = mbstowcs(NULL, value, 0);
	if (convertedLength == (size_t)-1)
		return (int)strtol(value, NULL, 0);
	std::vector<CHAR16> wval(convertedLength + 1, L'\0');
	if (mbstowcs(wval.data(), value, wval.size()) == (size_t)-1)
		return (int)strtol(value, NULL, 0);
	for (const Str16EnumLookupType *itr = table; itr->name != NULL; ++itr) {
		if (0 == _wcsicmp(wval.data(), itr->name)) {
			result = itr->value;
		}
	}
	return (result) ? result : (int)strtol(value, NULL, 0);
}

int StringToEnum(const STR16 value, const Str8EnumLookupType *table)
{
	if (NULL == value || 0 == *value || NULL == table)
		return 0;

	int result = 0;
	const size_t convertedLength = wcstombs(NULL, value, 0);
	if (convertedLength == (size_t)-1)
		return (int)wcstol(value, NULL, 0);
	std::vector<CHAR8> mval(convertedLength + 1, '\0');
	if (wcstombs(mval.data(), value, mval.size()) == (size_t)-1)
		return (int)wcstol(value, NULL, 0);
	for (const Str8EnumLookupType *itr = table; itr->name != NULL; ++itr) {
		if (0 == _stricmp(mval.data(), itr->name)) {
			result = itr->value;
		}
	}
	return (result) ? result : (int)wcstol(value, NULL, 0);
}

int StringToEnum(const STR16 value, const Str16EnumLookupType *table) {
	if (NULL == value || 0 == *value || NULL == table)
		return 0;

	for (const Str16EnumLookupType *itr = table; itr->name != NULL; ++itr) {
		if (0 == _wcsicmp(value, itr->name)) 
			return itr->value;
	}
	return (int)wcstol(value, NULL, 0);
}



// routine for parsing white space separated lines.  Handled like command line parameters w.r.t quotes.
void ParseCommandLine (
                        const char *start,
                        char **argv,
                        char *args,
                        int *numargs,
                        int *numchars
                        )
{
   const char NULCHAR    = '\0';
   const char SPACECHAR  = ' ';
   const char TABCHAR    = '\t';
   const char RETURNCHAR = '\r';
   const char LINEFEEDCHAR = '\n';
   const char DQUOTECHAR = '\"';
   const char SLASHCHAR  = '\\';
   const char *p;
   int inquote;                    /* 1 = inside quotes */
   int copychar;                   /* 1 = copy char to *args */
   unsigned numslash;              /* num of backslashes seen */

   if (!numargs || !numchars) return;
   *numchars = 0;
   *numargs = 0;                   /* the program name at least */
   if (!start) {
      if (argv) *argv = NULL;
      return;
   }

   p = start;

   inquote = 0;

   /* loop on each argument */
   for(;;) 
   {
      if ( *p ) { while (*p == SPACECHAR || *p == TABCHAR || *p == RETURNCHAR || *p == LINEFEEDCHAR) ++p; }

      if (*p == NULCHAR) break; /* end of args */

      /* scan an argument */
      if (argv)
         *argv++ = args;     /* store ptr to arg */
      ++*numargs;

      /* loop through scanning one argument */
      for (;;) 
      {
         copychar = 1;
         /* Rules: 2N backslashes + " ==> N backslashes and begin/end quote
         2N+1 backslashes + " ==> N backslashes + literal "
         N backslashes ==> N backslashes */
         numslash = 0;
         while (*p == SLASHCHAR) 
         {
            /* count number of backslashes for use below */
            ++p;
            ++numslash;
         }
         if (*p == DQUOTECHAR) 
         {
            /* if 2N backslashes before, start/end quote, otherwise copy literally */
            if (numslash % 2 == 0) {
               if (inquote) {
                  if (p[1] == DQUOTECHAR)
                     p++;    /* Double quote inside quoted string */
                  else        /* skip first quote char and copy second */
                     copychar = 0;
               } else
                  copychar = 0;       /* don't copy quote */

               inquote = !inquote;
            }
            numslash /= 2;          /* divide numslash by two */
         }

         /* copy slashes */
         while (numslash--) {
            if (args)
               *args++ = SLASHCHAR;
            ++*numchars;
         }

         /* if at end of arg, break loop */
         if (*p == NULCHAR || (!inquote && (*p == SPACECHAR || *p == TABCHAR || *p == RETURNCHAR || *p == LINEFEEDCHAR)))
            break;

         /* copy character into argument */
         if (copychar) 
         {
            if (args)
               *args++ = *p;
            ++*numchars;
         }
         ++p;
      }

      /* null-terminate the argument */
      if (args)
         *args++ = NULCHAR;          /* terminate string */
      ++*numchars;
   }
   /* We put one last argument in -- a null ptr */
   if (argv)
      *argv++ = NULL;
   ++*numargs;
}


// routine for parsing white space separated lines.  Handled like command line parameters w.r.t quotes.
void ParseCommandLine (
                        const wchar_t *start,
                        wchar_t **argv,
                        wchar_t *args,
                        int *numargs,
                        int *numchars
                        )
{
   const wchar_t NULCHAR    = L'\0';
   const wchar_t SPACECHAR  = L' ';
   const wchar_t TABCHAR    = L'\t';
   const wchar_t RETURNCHAR = L'\r';
   const wchar_t LINEFEEDCHAR = L'\n';
   const wchar_t DQUOTECHAR = L'\"';
   const wchar_t SLASHCHAR  = L'\\';
   const wchar_t *p;
   int inquote;                    /* 1 = inside quotes */
   int copychar;                   /* 1 = copy char to *args */
   unsigned numslash;              /* num of backslashes seen */

   if (!numargs || !numchars) return;
   *numchars = 0;
   *numargs = 0;                   /* the program name at least */
   if (!start) {
      if (argv) *argv = NULL;
      return;
   }

   p = start;

   inquote = 0;

   /* loop on each argument */
   for(;;) 
   {
      if ( *p ) { while (*p == SPACECHAR || *p == TABCHAR || *p == RETURNCHAR || *p == LINEFEEDCHAR) ++p; }

      if (*p == NULCHAR) break; /* end of args */

      /* scan an argument */
      if (argv)
         *argv++ = args;     /* store ptr to arg */
      ++*numargs;

      /* loop through scanning one argument */
      for (;;) 
      {
         copychar = 1;
         /* Rules: 2N backslashes + " ==> N backslashes and begin/end quote
         2N+1 backslashes + " ==> N backslashes + literal "
         N backslashes ==> N backslashes */
         numslash = 0;
         while (*p == SLASHCHAR) 
         {
            /* count number of backslashes for use below */
            ++p;
            ++numslash;
         }
         if (*p == DQUOTECHAR) 
         {
            /* if 2N backslashes before, start/end quote, otherwise copy literally */
            if (numslash % 2 == 0) {
               if (inquote) {
                  if (p[1] == DQUOTECHAR)
                     p++;    /* Double quote inside quoted string */
                  else        /* skip first quote char and copy second */
                     copychar = 0;
               } else
                  copychar = 0;       /* don't copy quote */

               inquote = !inquote;
            }
            numslash /= 2;          /* divide numslash by two */
         }

         /* copy slashes */
         while (numslash--) {
            if (args)
               *args++ = SLASHCHAR;
            ++*numchars;
         }

         /* if at end of arg, break loop */
         if (*p == NULCHAR || (!inquote && (*p == SPACECHAR || *p == TABCHAR || *p == RETURNCHAR || *p == LINEFEEDCHAR)))
            break;

         /* copy character into argument */
         if (copychar) 
         {
            if (args)
               *args++ = *p;
            ++*numchars;
         }
         ++p;
      }

      /* null-terminate the argument */
      if (args)
         *args++ = NULCHAR;          /* terminate string */
      ++*numchars;
   }
   /* We put one last argument in -- a null ptr */
   if (argv)
      *argv++ = NULL;
   ++*numargs;
}

// convert UTF-8 string to wstring
std::wstring utf8_to_wstring(const std::string& str)
{
	return vfs::String::as_utf16(str);
}

// convert wstring to UTF-8 string
std::string wstring_to_utf8(const std::wstring& str)
{
	return vfs::String::as_utf8(str);
}
