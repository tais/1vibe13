#include "types.h"
#ifdef _WIN32
#include <windows.h>
#endif
#include "Quantize.h"
#include "types.h"
#include "himage.h"
#include "ImageUtilityModel.h"

#include <new>

CQuantizer::CQuantizer (UINT nMaxColors, UINT nColorBits)
{
	m_pTree = NULL;
	m_nLeafCount = 0;
	for (int i = 0; i <= 8; ++i)
		m_pReducibleNodes[i] = NULL;
	m_nMaxColors = nMaxColors;
	m_nColorBits = nColorBits;
	m_fValid = nMaxColors > 0 && nMaxColors <= 256 &&
		nColorBits > 0 && nColorBits <= 8;
}

CQuantizer::~CQuantizer ()
{
	ResetTree();
}

BOOL CQuantizer::ProcessImage (const BYTE *pData, int iWidth, int iHeight )
{
	ResetTree();

	std::size_t byteCount = 0;
	if (!m_fValid || !pData ||
		!UtilsImageUtilityModel::CheckedImageByteCount(
			iWidth, iHeight, 3, byteCount))
	{
		return FALSE;
	}

	const BYTE* pbBits;
	BYTE r, g, b;
	int i, j;


	pbBits = pData;
	for (i=0; i<iHeight; i++) {
		for (j=0; j<iWidth; j++) {
			b = *pbBits++;
			g = *pbBits++;
			r = *pbBits++;
			if (!AddColor (&m_pTree, r, g, b, m_nColorBits, 0,
				&m_nLeafCount, m_pReducibleNodes))
			{
				ResetTree();
				return FALSE;
			}
			while (m_nLeafCount > m_nMaxColors)
			{
				if (!ReduceTree (m_nColorBits, &m_nLeafCount,
					m_pReducibleNodes))
				{
					ResetTree();
					return FALSE;
				}
			}
		}
				//Padding
		//pbBits ++;
	}
	return TRUE;
}

BOOL CQuantizer::AddColor (NODE** ppNode, BYTE r, BYTE g, BYTE b,
	UINT nColorBits, UINT nLevel, UINT* pLeafCount, NODE** pReducibleNodes)
{
	static const BYTE mask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

	//
	// If the node doesn't exist, create it.
	//
	if (*ppNode == NULL)
		*ppNode = CreateNode (nLevel, nColorBits, pLeafCount,
			pReducibleNodes);
	if (*ppNode == NULL) return FALSE;

	//
	// Update color information if it's a leaf node.
	//
	if ((*ppNode)->bIsLeaf) {
		(*ppNode)->nPixelCount++;
		(*ppNode)->nRedSum += r;
		(*ppNode)->nGreenSum += g;
		(*ppNode)->nBlueSum += b;
	}

	//
	// Recurse a level deeper if the node is not a leaf.
	//
	else {
		int shift = 7 - nLevel;
		int nIndex = (((r & mask[nLevel]) >> shift) << 2) |
			(((g & mask[nLevel]) >> shift) << 1) |
			((b & mask[nLevel]) >> shift);
		if (!AddColor (&((*ppNode)->pChild[nIndex]), r, g, b, nColorBits,
			nLevel + 1, pLeafCount, pReducibleNodes))
		{
			return FALSE;
		}
	}
	return TRUE;
}

NODE* CQuantizer::CreateNode (UINT nLevel, UINT nColorBits, UINT* pLeafCount,
	NODE** pReducibleNodes)
{
	NODE* pNode = new (std::nothrow) NODE{};
	if (pNode == NULL)
		return NULL;

	pNode->bIsLeaf = (nLevel == nColorBits) ? TRUE : FALSE;
	if (pNode->bIsLeaf)
		(*pLeafCount)++;
	else {
		pNode->pNext = pReducibleNodes[nLevel];
		pReducibleNodes[nLevel] = pNode;
	}
	return pNode;
}

BOOL CQuantizer::ReduceTree (UINT nColorBits, UINT* pLeafCount,
	NODE** pReducibleNodes)
{
	//
	// Find the deepest level containing at least one reducible node.
	//
	int i = static_cast<int>(nColorBits) - 1;
	for (; i >= 0 && pReducibleNodes[i] == NULL; --i) {}
	if (i < 0) return FALSE;

	//
	// Reduce the node most recently added to the list at level i.
	//

	NODE* pNode = pReducibleNodes[i];
	pReducibleNodes[i] = pNode->pNext;

	std::uint64_t nRedSum = 0;
	std::uint64_t nGreenSum = 0;
	std::uint64_t nBlueSum = 0;
	UINT nChildren = 0;

	for (i=0; i<8; i++) {
		if (pNode->pChild[i] != NULL) {
			nRedSum += pNode->pChild[i]->nRedSum;
			nGreenSum += pNode->pChild[i]->nGreenSum;
			nBlueSum += pNode->pChild[i]->nBlueSum;
			pNode->nPixelCount += pNode->pChild[i]->nPixelCount;
			DeleteTree(&pNode->pChild[i]);
			nChildren++;
		}
	}

	pNode->bIsLeaf = TRUE;
	pNode->nRedSum = nRedSum;
	pNode->nGreenSum = nGreenSum;
	pNode->nBlueSum = nBlueSum;
	if (nChildren == 0 || *pLeafCount < nChildren - 1) return FALSE;
	*pLeafCount -= (nChildren - 1);
	return TRUE;
}

void CQuantizer::DeleteTree (NODE** ppNode)
{
	if (!ppNode || !*ppNode) return;
	for (int i=0; i<8; i++) {
		if ((*ppNode)->pChild[i] != NULL)
			DeleteTree (&((*ppNode)->pChild[i]));
	}
	delete *ppNode;
	*ppNode = NULL;
}

void CQuantizer::ResetTree()
{
	DeleteTree(&m_pTree);
	m_nLeafCount = 0;
	for (UINT level = 0; level <= 8; ++level)
		m_pReducibleNodes[level] = NULL;
}

void CQuantizer::GetPaletteColors (NODE* pTree, RGBQUAD* prgb, UINT* pIndex)
{
	if (!pTree || !prgb || !pIndex) return;
	if (pTree->bIsLeaf) {
		if (pTree->nPixelCount == 0 || *pIndex >= 256) return;
		prgb[*pIndex].rgbRed =
			(BYTE) ((pTree->nRedSum) / (pTree->nPixelCount));
		prgb[*pIndex].rgbGreen =
			(BYTE) ((pTree->nGreenSum) / (pTree->nPixelCount));
		prgb[*pIndex].rgbBlue =
			(BYTE) ((pTree->nBlueSum) / (pTree->nPixelCount));
		prgb[*pIndex].rgbReserved = 0;
		(*pIndex)++;
	}
	else {
		for (int i=0; i<8; i++) {
			if (pTree->pChild[i] != NULL)
				GetPaletteColors (pTree->pChild[i], prgb, pIndex);
		}
	}
}

UINT CQuantizer::GetColorCount ()
{
	return m_nLeafCount;
}

void CQuantizer::GetColorTable (RGBQUAD* prgb)
{
	if (!prgb || !m_pTree) return;
	UINT nIndex = 0;
	GetPaletteColors (m_pTree, prgb, &nIndex);
}


