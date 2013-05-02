#include "config_type_proto.h"

static struct ConfigTypeProto::Mapping config_type_proto_map[] = {
	{ "TCP",	ConfigProtoTCP },
	{ "TCP_POOL",	ConfigProtoTCPPool },
	{ NULL,		ConfigProtoNone }
};

ConfigTypeProto
	config_type_proto("proto", config_type_proto_map);
