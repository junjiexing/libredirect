#pragma once

#include <ntddk.h>

#include <wdf.h>

#include <mstcpip.h>

#define NDIS61 1				// Need to declare this to compile WFP stuff on Win7, I'm not sure why

#pragma warning(push)
#pragma warning(disable: 4201)	// Disable "Nameless struct/union" compiler warning for fwpsk.h only!
#include <fwpsk.h>				// Functions and enumerated types used to implement callouts in kernel mode
#pragma warning(pop)			// Re-enable "Nameless struct/union" compiler warning

#include <initguid.h>
#include <fwpmk.h>				// Functions used for managing IKE and AuthIP main mode (MM) policy and security associations
#include <fwpvi.h>				// Mappings of OS specific function versions (i.e. fn's that end in 0 or 1)
#include <guiddef.h>			// Used to define GUID's
#include <initguid.h>			// Used to define GUID's
#include "devguid.h"

#include "../libredirect/libredirect_common.h"

struct conn_item_t
{
	LIST_ENTRY list_entry;
    connect_t conn;
};

struct read_item_t
{
	LIST_ENTRY list_entry;
	WDFREQUEST request;
};

extern LIST_ENTRY read_req_list;
extern LIST_ENTRY connect_list;
extern WDFWAITLOCK connect_list_lck;


void callout_classify(
    const FWPS_INCOMING_VALUES* inFixedValues,
    const FWPS_INCOMING_METADATA_VALUES* inMetaValues,
    void* layerData, const void* classifyContext,
    const FWPS_FILTER* filter, UINT64 flowContext,
    FWPS_CLASSIFY_OUT* classifyOut);

NTSTATUS NTAPI callout_notify(FWPS_CALLOUT_NOTIFY_TYPE notifyType,
    const GUID* filterKey, FWPS_FILTER* filter);

void NTAPI callout_flow_delete(UINT16 layerId, UINT32 calloutId, UINT64 flowContext);
void do_redirect(connect_t& conn);

