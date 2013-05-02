#ifndef	CONFIG_CONFIG_TYPE_PROTO_H
#define	CONFIG_CONFIG_TYPE_PROTO_H

#include <config/config_type_enum.h>

enum ConfigProto {
	ConfigProtoTCP,
	ConfigProtoTCPPool,
	ConfigProtoNone,
};

typedef ConfigTypeEnum<ConfigProto> ConfigTypeProto;

extern ConfigTypeProto config_type_proto;

#endif /* !CONFIG_CONFIG_TYPE_PROTO_H */
