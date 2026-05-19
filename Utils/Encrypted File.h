#ifndef __ENCRYPTED_H_
#define __ENCRYPTED_H_

#include "types.h"

BOOLEAN LoadEncryptedDataFromFile(STR pFileName, CHAR16 *pDestString, UINT32 uiSeekFrom, UINT32 uiSeekAmount);
BOOLEAN LoadEncryptedDataFromFileRandomLine(STR pFileName, CHAR16 *pDestString, UINT32 uiSeekAmount);
void DecodeString(CHAR16 *pDestString, UINT32 uiSeekAmount);

#endif
