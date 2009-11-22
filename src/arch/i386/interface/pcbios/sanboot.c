#include <gpxe/dhcp.h>
#include <gpxe/settings.h>

FILE_LICENCE ( GPL2_OR_LATER );

struct setting keep_san_setting __setting = {
	.name = "keep-san",
	.description = "Preserve SAN connection",
	.tag = DHCP_EB_KEEP_SAN,
	.type = &setting_type_int8,
};
