#pragma once

#include <WinSock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#include <fwpmu.h>
#include <fwpstypes.h>


#ifdef LIBREDIRECT_EXPORTS
#define LIBREDIRECT_API __declspec(dllexport)
#else
#define LIBREDIRECT_API __declspec(dllimport)
#endif

#include "libredirect_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define LIBREDIRECT_INIT_IPV4	1
#define LIBREDIRECT_INIT_IPV6	2


	LIBREDIRECT_API DWORD libredirect_init(DWORD init_type, HANDLE* engine_handle);
	LIBREDIRECT_API void libredirect_uninit(HANDLE engine_handle);

	LIBREDIRECT_API HANDLE libredirect_open();
	LIBREDIRECT_API void libredirect_close(HANDLE handle);

	LIBREDIRECT_API int libredirect_read_connect(HANDLE handle, connect_t* conn);
	LIBREDIRECT_API int libredirect_write_connect(HANDLE handle, const connect_t* conn);

#ifdef __cplusplus
}
#endif
