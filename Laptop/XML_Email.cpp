#include <Engine/Adapters/Legacy/LegacyXmlDocument.h>

#include "LocalizationInputAdapter.h"

#include "sgp.h"
#include "Debug Control.h"
#include "expat.h"
#include "XML.h"
#include "Interface.h"
#include "LuaInitNPCs.h"
#include "email.h"

#include <utility>

struct
{
    PARSE_STAGE	curElement;

    CHAR8		szCharData[MAIL_STRING_SIZE + 1];
    CHAR16      currentMessage[MAIL_STRING_SIZE];
    EMAIL_XML   currentEmail;
    std::vector<EMAIL_XML>* emails;
    UINT16      currentEmailIndex;
    UINT16      currentMessageIndex;
    BOOLEAN     localizedVersion;
    bool        valid;

    UINT32		maxArraySize;
    UINT32		curIndex;
    UINT32		currentDepth;
    UINT32		maxReadDepth;
} typedef EmailXMLParseData;

static void XMLCALL
EmailOtherStartElementHandle(void* userData, const XML_Char* name, const XML_Char** atts)
{
    EmailXMLParseData* pData = (EmailXMLParseData*)userData;

    if (pData->currentDepth <= pData->maxReadDepth) //are we reading this element?
    {
        if (strcmp(name, "EMAILS") == 0 && pData->curElement == ELEMENT_NONE)
        {
            pData->curElement = ELEMENT_LIST;

            pData->maxReadDepth++; //we are not skipping this element
        }
        else if (strcmp(name, "EMAIL") == 0 && pData->curElement == ELEMENT_LIST)
        {
            pData->curElement = ELEMENT;
            pData->currentEmail = EMAIL_XML{};
            pData->currentMessageIndex = 0;

            pData->maxReadDepth++; //we are not skipping this element
        }
        else if (pData->curElement == ELEMENT &&
            (strcmp(name, "Index") == 0 ||
                strcmp(name, "Sender") == 0 ||
                strcmp(name, "Subject") == 0 ||
                strcmp(name, "Message") == 0))
        {
            pData->curElement = ELEMENT_PROPERTY;

            pData->maxReadDepth++; //we are not skipping this element
        }

        pData->szCharData[0] = '\0';
    }

    pData->currentDepth++;

}

static void XMLCALL
EmailOtherCharacterDataHandle(void* userData, const XML_Char* str, int len)
{
    EmailXMLParseData* pData = (EmailXMLParseData*)userData;

    if (pData->currentDepth <= pData->maxReadDepth && pData->valid)
        pData->valid = LaptopLocalizationModel::AppendText(
            pData->szCharData, str, len);
}


static void XMLCALL
EmailOtherEndElementHandle(void* userData, const XML_Char* name)
{
    EmailXMLParseData* pData = (EmailXMLParseData*)userData;

    if (pData->currentDepth <= pData->maxReadDepth)
    {
        if (strcmp(name, "EMAILS") == 0)
        {
            pData->curElement = ELEMENT_NONE;
        }
        else if (strcmp(name, "EMAIL") == 0)
        {
            pData->curElement = ELEMENT_LIST;
            if (!pData->localizedVersion)
            {
                pData->emails->push_back(std::move(pData->currentEmail));
            }
            else
            {
                const auto index = pData->currentEmailIndex;
                if (index >= pData->emails->size() ||
                    pData->currentMessageIndex !=
                        (*pData->emails)[index].Messages.size())
                {
                    pData->valid = false;
                }
                pData->currentEmailIndex += 1;
            }
        }
        else if (strcmp(name, "Sender") == 0)
        {
            pData->curElement = ELEMENT;
            if (!pData->localizedVersion)
            {
                pData->valid = pData->valid &&
                    LaptopLocalizationModel::ParseInteger(
                        pData->szCharData, pData->currentEmail.Sender);
            }
        }
        else if (strcmp(name, "Subject") == 0)
        {
            pData->curElement = ELEMENT;
            if (!pData->localizedVersion)
            {
                pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
                    pData->szCharData, pData->currentEmail.Subject);
            }
            else
            {
                // Replace existing text with localized version
                const auto i = pData->currentEmailIndex;
                if (i >= pData->emails->size())
                    pData->valid = false;
                else
                    pData->valid = pData->valid &&
                        LaptopLocalization::ConvertUtf8(
                            pData->szCharData, (*pData->emails)[i].Subject);
            }
        }
        else if (strcmp(name, "Message") == 0)
        {
            pData->curElement = ELEMENT;
            pData->valid = pData->valid && LaptopLocalization::ConvertUtf8(
                pData->szCharData, pData->currentMessage);

            if (!pData->localizedVersion)
            {
                if (pData->valid)
                    pData->currentEmail.Messages.emplace_back(
                        pData->currentMessage);
            }
            else
            {
                // Replace existing text with localized version
                const auto i = pData->currentEmailIndex;
                const auto j = pData->currentMessageIndex;
                if (i >= pData->emails->size() ||
                    j >= (*pData->emails)[i].Messages.size())
                {
                    pData->valid = false;
                }
                else if (pData->valid)
                {
                    (*pData->emails)[i].Messages[j] = pData->currentMessage;
                }

                pData->currentMessageIndex++;
            }
        }

        else if (pData->curElement == ELEMENT_PROPERTY)
        {
            // Recognized-but-unhandled leaf close (e.g. <Index>) -- pop back to ELEMENT so the
            // rest of this <EMAIL> record is not silently skipped/zeroed.
            pData->curElement = ELEMENT;
        }
        pData->maxReadDepth--;
    }
    pData->currentDepth--;
}

BOOLEAN ReadInExternalizedEmails(STR fileName, BOOLEAN localizedVersion)
{
    EmailXMLParseData pData{};
    std::vector<EMAIL_XML> pendingEmails =
        localizedVersion ? gEmails : std::vector<EMAIL_XML>{};

    DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Loading Emails.xml");

    if (!localizedVersion)
        pendingEmails.reserve(XML_JA2UB_SPECK_DISMISSALREFUND);
    pData.emails = &pendingEmails;
    pData.localizedVersion = localizedVersion;
    pData.valid = true;

    const LegacyXmlCallbacks callbacks{
        &pData, EmailOtherStartElementHandle, EmailOtherEndElementHandle,
        EmailOtherCharacterDataHandle};
    const LegacyXmlResult result =
        ParseLegacyXmlFile(fileName, callbacks);
    if (!result)
    {
        if (result.status == LegacyXmlStatus::NotFound)
            return localizedVersion;
        if (result.status != LegacyXmlStatus::ReadError)
        {
            const auto message = FormatLegacyXmlFailure(fileName, result);
            LiveMessage(message.data());
        }
        return FALSE;
    }

    if (!pData.valid ||
        (localizedVersion &&
            pData.currentEmailIndex != pendingEmails.size()))
        return FALSE;

    gEmails.swap(pendingEmails);

    return(TRUE);
}
