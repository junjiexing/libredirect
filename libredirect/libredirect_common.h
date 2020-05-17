#pragma once

#pragma once

// {0CF72951-F078-4854-9AFD-0E870B0ABEBA}
DEFINE_GUID(LIBREDIRECT_CALLOUT_GUID_V4,
	0xcf72951, 0xf078, 0x4854, 0x9a, 0xfd, 0xe, 0x87, 0xb, 0xa, 0xbe, 0xba);

// {EAD094BF-4EC4-4CA6-A343-E1170FF90DB6}
DEFINE_GUID(LIBREDIRECT_CALLOUT_GUID_V6,
	0xead094bf, 0x4ec4, 0x4ca6, 0xa3, 0x43, 0xe1, 0x17, 0xf, 0xf9, 0xd, 0xb6);

// {AE1E820A-C60A-42A8-B4A2-9ACFB050387F}
DEFINE_GUID(LIBREDIRECT_SUBLAYER_GUID,
	0xae1e820a, 0xc60a, 0x42a8, 0xb4, 0xa2, 0x9a, 0xcf, 0xb0, 0x50, 0x38, 0x7f);


#define IOCTL_GET_CONN CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SET_CONN CTL_CODE(FILE_DEVICE_UNKNOWN,0x802,METHOD_BUFFERED,FILE_ANY_ACCESS)



struct connect_t
{
	int ip_version;
	union
	{
		struct
		{
			IN_ADDR local_address;
			IN_ADDR remote_address;
			USHORT local_port;
			USHORT remote_port;
		} v4;
		struct
		{
			IN6_ADDR local_address;
			IN6_ADDR remote_address;
			USHORT local_port;
			USHORT remote_port;
			SCOPE_ID remote_scope_id;
		}v6;
	};

	UINT64 process_id;

	DWORD local_redirect_pid;

	struct
	{
		UINT64 classify_handle;
		UINT64 filter_id;
		FWPS_CLASSIFY_OUT classify_out;
	} _priv;
};
