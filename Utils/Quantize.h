#ifndef __QUANTIZE_H_
#define __QUANTIZE_H_

#include "types.h"

#include <cstdint>

typedef struct _NODE {
	BOOL bIsLeaf;				// TRUE if node has no children
	std::uint64_t nPixelCount;	// Number of pixels represented by this leaf
	std::uint64_t nRedSum;		// Sum of red components
	std::uint64_t nGreenSum;	// Sum of green components
	std::uint64_t nBlueSum;		// Sum of blue components
	struct _NODE* pChild[8];	// Pointers to child nodes
	struct _NODE* pNext;		// Pointer to next reducible node
} NODE;

class CQuantizer
{
protected:
	NODE* m_pTree;
	UINT m_nLeafCount;
	NODE* m_pReducibleNodes[9];
	UINT m_nMaxColors;
	UINT m_nColorBits;
	BOOL m_fValid;

public:
	CQuantizer (UINT nMaxColors, UINT nColorBits);
	virtual ~CQuantizer ();
	BOOL ProcessImage (const BYTE *pData, int iWidth, int iHeight );
	UINT GetColorCount ();
	void GetColorTable (RGBQUAD* prgb);

protected:
	BOOL AddColor (NODE** ppNode, BYTE r, BYTE g, BYTE b, UINT nColorBits,
		UINT nLevel, UINT* pLeafCount, NODE** pReducibleNodes);
	NODE* CreateNode (UINT nLevel, UINT nColorBits, UINT* pLeafCount,
		NODE** pReducibleNodes);
	BOOL ReduceTree (UINT nColorBits, UINT* pLeafCount,
		NODE** pReducibleNodes);
	void ResetTree();
	void DeleteTree (NODE** ppNode);
	void GetPaletteColors (NODE* pTree, RGBQUAD* prgb, UINT* pIndex);
};

#endif
