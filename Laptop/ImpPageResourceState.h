#ifndef LAPTOP_IMP_PAGE_RESOURCE_STATE_H
#define LAPTOP_IMP_PAGE_RESOURCE_STATE_H

#include "LaptopUiStateModel.h"

inline LaptopUiStateModel::ResourceTransactionState&
GetImpPageResourceTransactionState()
{
	static LaptopUiStateModel::ResourceTransactionState state;
	return state;
}

inline bool ImpPageResourcesReady()
{
	return GetImpPageResourceTransactionState().ready();
}

#endif
