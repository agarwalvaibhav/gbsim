
/*
 * Greybus Simulator
 *
 * Copyright 2014, 2015 Google Inc.
 * Copyright 2014, 2015 Linaro Ltd.
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <fcntl.h>
#include <pthread.h>
#include <libsoc_gpio.h>
#include <linux/fs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "gbsim.h"

#define CONFIG_COUNT_MAX 20

int i2s_mgmt_handler(uint16_t cport_id, uint16_t hd_cport_id, void *rbuf,
		     size_t rsize, void *tbuf, size_t tsize)
{
	struct op_header *oph;
	struct op_msg *op_req = rbuf;
	struct op_msg *op_rsp;
	struct gb_i2s_mgmt_configuration *conf;
	size_t payload_size;
	uint16_t message_size;
	uint8_t module_id;
	uint8_t result = PROTOCOL_STATUS_SUCCESS;
	ssize_t nbytes;

	module_id = cport_to_module_id(cport_id);

	op_rsp = (struct op_msg *)tbuf;
	oph = (struct op_header *)&op_req->header;

	switch (oph->type) {
	case GB_I2S_MGMT_TYPE_PROTOCOL_VERSION:
		payload_size = sizeof(struct gb_protocol_version_response);
		op_rsp->pv_rsp.major = GREYBUS_VERSION_MAJOR;
		op_rsp->pv_rsp.minor = GREYBUS_VERSION_MINOR;
		gbsim_debug("Module %hhu -> AP CPort %hu I2S protocol version response\n  ",
				module_id, cport_id);
		break;
	case GB_I2S_MGMT_TYPE_GET_SUPPORTED_CONFIGURATIONS:
		payload_size = sizeof(struct gb_i2s_mgmt_get_supported_configurations_response) +
			sizeof(struct gb_i2s_mgmt_configuration) * CONFIG_COUNT_MAX;

		op_rsp->i2s_mgmt_get_sup_conf_rsp.config_count = 1;

		conf = &op_rsp->i2s_mgmt_get_sup_conf_rsp.config[0];
		conf->sample_frequency = htole32(48000);
		conf->num_channels = 2;
		conf->bytes_per_channel = 2;
		conf->byte_order = GB_I2S_MGMT_BYTE_ORDER_LE;
		conf->spatial_locations = htole32(
						GB_I2S_MGMT_SPATIAL_LOCATION_FL |
						GB_I2S_MGMT_SPATIAL_LOCATION_FR);
		conf->ll_protocol = htole32(GB_I2S_MGMT_PROTOCOL_I2S);
		conf->ll_mclk_role = GB_I2S_MGMT_ROLE_MASTER;
		conf->ll_bclk_role = GB_I2S_MGMT_ROLE_MASTER;
		conf->ll_wclk_role = GB_I2S_MGMT_ROLE_MASTER;
		conf->ll_wclk_polarity = GB_I2S_MGMT_POLARITY_NORMAL;
		conf->ll_wclk_change_edge = GB_I2S_MGMT_EDGE_FALLING;
		conf->ll_wclk_tx_edge = GB_I2S_MGMT_EDGE_RISING;
		conf->ll_wclk_rx_edge = GB_I2S_MGMT_EDGE_FALLING;
		conf->ll_data_offset = 1;

		gbsim_debug("Module %hhu -> AP CPort %hu I2S GET_CONFIGURATION response\n  ",
			    module_id, cport_id);
		break;
	case GB_I2S_MGMT_TYPE_SET_CONFIGURATION:
		payload_size = 0;
		gbsim_debug("Module %hhu -> AP CPort %hu I2S SET_CONFIGURATION response\n  ",
			    module_id, cport_id);
		break;
	case GB_I2S_MGMT_TYPE_SET_SAMPLES_PER_MESSAGE:
		payload_size = 0;
		gbsim_debug("Module %hhu -> AP CPort %hu I2S SET_SAMPLES_PER_MESSAGE response\n  ",
			    module_id, cport_id);
		break;
	case GB_I2S_MGMT_TYPE_SET_START_DELAY:
		payload_size = 0;
		gbsim_debug("Module %hhu -> AP CPort %hu I2S SET_START_DELAY response\n  ",
			    module_id, cport_id);
		break;
	case GB_I2S_MGMT_TYPE_ACTIVATE_CPORT:
		payload_size = 0;
		gbsim_debug("Module %hhu -> AP CPort %hu I2S ACTIVATE_CPORT response\n  ",
			    module_id, cport_id);
		break;
	case GB_I2S_MGMT_TYPE_DEACTIVATE_CPORT:
		payload_size = 0;
		gbsim_debug("Module %hhu -> AP CPort %hu I2S DEACTIVATE_CPORT response\n  ",
			    module_id, cport_id);
		break;
	default:
		gbsim_error("i2s mgmt operation type %02x not supported\n", oph->type);
		return -EINVAL;
	}

	/* Fill in the response header */
	message_size = sizeof(struct op_header) + payload_size;
	op_rsp->header.size = htole16(message_size);
	op_rsp->header.id = oph->id;
	op_rsp->header.type = OP_RESPONSE | oph->type;
	op_rsp->header.result = result;
	/* Store the cport id in the header pad bytes */
	op_rsp->header.pad[0] = hd_cport_id & 0xff;
	op_rsp->header.pad[1] = (hd_cport_id >> 8) & 0xff;

	if (verbose)
		gbsim_dump(op_rsp, message_size);

	nbytes = write(to_ap, op_rsp, message_size);
	if (nbytes < 0)
		return nbytes;

	return 0;
}


int i2s_data_handler(uint16_t cport_id, uint16_t hd_cport_id, void *rbuf,
		     size_t rsize, void *tbuf, size_t tsize)
{
	struct op_header *oph;
	struct op_msg *op_req = rbuf;
	struct op_msg *op_rsp;
	size_t payload_size;
	uint16_t message_size;
	uint8_t module_id;
	uint8_t result = PROTOCOL_STATUS_SUCCESS;
	ssize_t nbytes;

	module_id = cport_to_module_id(cport_id);

	op_rsp = (struct op_msg *)tbuf;
	oph = (struct op_header *)&op_req->header;

	switch (oph->type) {
	case GB_I2S_DATA_TYPE_PROTOCOL_VERSION:
		payload_size = sizeof(struct gb_protocol_version_response);
		op_rsp->pv_rsp.major = GREYBUS_VERSION_MAJOR;
		op_rsp->pv_rsp.minor = GREYBUS_VERSION_MINOR;
		gbsim_debug("Module %hhu -> AP CPort %hu I2S protocol version response\n  ",
				module_id, cport_id);
		break;
	case GB_I2S_DATA_TYPE_SEND_DATA:
		payload_size = 0;
		gbsim_debug("Module %hhu -> AP CPort %hu I2S SEND_DATA response\n  ",
			    module_id, cport_id);
		break;
	default:
		gbsim_error("i2s data operation type %02x not supported\n", oph->type);
		return -EINVAL;
	}

	/* Fill in the response header */
	message_size = sizeof(struct op_header) + payload_size;
	op_rsp->header.size = htole16(message_size);
	op_rsp->header.id = oph->id;
	op_rsp->header.type = OP_RESPONSE | oph->type;
	op_rsp->header.result = result;
	/* Store the cport id in the header pad bytes */
	op_rsp->header.pad[0] = hd_cport_id & 0xff;
	op_rsp->header.pad[1] = (hd_cport_id >> 8) & 0xff;

	if (verbose)
		gbsim_dump(op_rsp, message_size);

	nbytes = write(to_ap, op_rsp, message_size);
	if (nbytes < 0)
		return nbytes;

	return 0;
}

char *i2s_mgmt_get_operation(uint8_t type)
{
	switch (type) {
	case GB_I2S_MGMT_TYPE_PROTOCOL_VERSION:
		return "GB_I2S_MGMT_TYPE_PROTOCOL_VERSION";
	case GB_I2S_MGMT_TYPE_GET_SUPPORTED_CONFIGURATIONS:
		return "GB_I2S_MGMT_TYPE_GET_SUPPORTED_CONFIGURATIONS";
	case GB_I2S_MGMT_TYPE_SET_CONFIGURATION:
		return "GB_I2S_MGMT_TYPE_SET_CONFIGURATION";
	case GB_I2S_MGMT_TYPE_SET_SAMPLES_PER_MESSAGE:
		return "GB_I2S_MGMT_TYPE_SET_SAMPLES_PER_MESSAGE";
	case GB_I2S_MGMT_TYPE_GET_PROCESSING_DELAY:
		return "GB_I2S_MGMT_TYPE_GET_PROCESSING_DELAY";
	case GB_I2S_MGMT_TYPE_SET_START_DELAY:
		return "GB_I2S_MGMT_TYPE_SET_START_DELAY";
	case GB_I2S_MGMT_TYPE_ACTIVATE_CPORT:
		return "GB_I2S_MGMT_TYPE_ACTIVATE_CPORT";
	case GB_I2S_MGMT_TYPE_DEACTIVATE_CPORT:
		return "GB_I2S_MGMT_TYPE_DEACTIVATE_CPORT";
	case GB_I2S_MGMT_TYPE_REPORT_EVENT:
		return "GB_I2S_MGMT_TYPE_REPORT_EVENT";
	default:
		return "(Unknown operation)";
	}
}

char *i2s_data_get_operation(uint8_t type)
{
	switch (type) {
	case GB_I2S_DATA_TYPE_PROTOCOL_VERSION:
		return "GB_I2S_DATA_TYPE_PROTOCOL_VERSION";
	case GB_I2S_DATA_TYPE_SEND_DATA:
		return "GB_I2S_DATA_TYPE_SEND_DATA";
	default:
		return "(Unknown operation)";
	}
}

void i2s_init(void)
{

}
