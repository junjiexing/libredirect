#pragma once

// {0CF72951-F078-4854-9AFD-0E870B0ABEBA}
DEFINE_GUID(FORCEPROXY_CALLOUT_GUID,
	0xcf72951, 0xf078, 0x4854, 0x9a, 0xfd, 0xe, 0x87, 0xb, 0xa, 0xbe, 0xba);

// {AE1E820A-C60A-42A8-B4A2-9ACFB050387F}
DEFINE_GUID(FORCEPROXY_SUBLAYER_GUID,
	0xae1e820a, 0xc60a, 0x42a8, 0xb4, 0xa2, 0x9a, 0xcf, 0xb0, 0x50, 0x38, 0x7f);

#define IOCTL_GET_CONN CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SET_CONN CTL_CODE(FILE_DEVICE_UNKNOWN,0x802,METHOD_BUFFERED,FILE_ANY_ACCESS)

#define FORMAT_ADDR(x) (x>>24)&0xFF, (x>>16)&0xFF, (x>>8)&0xFF, x&0xFF


struct connect_t
{
	int ip_version;
	union
	{
		struct
		{
			UINT32 local_address;
			UINT32 remote_address;
			UINT16 local_port;
			UINT16 remote_port;
		} v4;
		struct
		{

		}v6;
	};

	UINT64 process_id;

	struct
	{
		UINT64 classify_handle;
		UINT64 filter_id;
		FWPS_CLASSIFY_OUT classify_out;
	} _priv;
};
