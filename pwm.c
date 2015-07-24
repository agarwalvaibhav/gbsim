
/*
 * Greybus Simulator
 *
 * Copyright 2014 Google Inc.
 * Copyright 2014 Linaro Ltd.
 *
 * Provided under the three clause BSD license found in the LICENSE file.
 */

#include <fcntl.h>
#include <libsoc_pwm.h>
#include <linux/fs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "gbsim.h"

static int pwm_on[2];
static pwm *pwms[2];

int pwm_handler(uint16_t cport_id, uint16_t hd_cport_id, void *rbuf,
		size_t rsize, void *tbuf, size_t tsize)
{
	struct op_header *oph;
	struct op_msg *op_req = rbuf;
	struct op_msg *op_rsp;
	__u32 duty;
	__u32 period;
	size_t payload_size;
	uint16_t message_size;
	uint8_t module_id;
	uint8_t result = PROTOCOL_STATUS_SUCCESS;
	ssize_t nbytes;

	module_id = cport_to_module_id(cport_id);

	op_rsp = (struct op_msg *)tbuf;
	oph = (struct op_header *)&op_req->header;

	switch (oph->type) {
	case GB_PWM_TYPE_PROTOCOL_VERSION:
		payload_size = sizeof(struct gb_protocol_version_response);
		op_rsp->pv_rsp.major = GREYBUS_VERSION_MAJOR;
		op_rsp->pv_rsp.minor = GREYBUS_VERSION_MINOR;
		gbsim_debug("Module %hhu -> AP CPort %hu PWM protocol version response\n  ",
			    module_id, cport_id);
		break;
	case GB_PWM_TYPE_PWM_COUNT:
		payload_size = sizeof(struct gb_pwm_count_response);
		op_rsp->pwm_cnt_rsp.count = 1; /* Something arbitrary, but useful */
		gbsim_debug("Module %hhu -> AP CPort %hu PWM count response\n  ",
			    module_id, cport_id);
		break;
	case GB_PWM_TYPE_ACTIVATE:
		payload_size = 0;
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d activate request\n  ",
			    module_id, cport_id, op_req->pwm_act_req.which);
		break;
	case GB_PWM_TYPE_DEACTIVATE:
		payload_size = 0;
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d deactivate request\n  ",
			    module_id, cport_id, op_req->pwm_deact_req.which);
		break;
	case GB_PWM_TYPE_CONFIG:
		payload_size = 0;
		duty = le32toh(op_req->pwm_cfg_req.duty);
		period = le32toh(op_req->pwm_cfg_req.period);
		if (bbb_backend) {
			libsoc_pwm_set_duty_cycle(pwms[op_req->pwm_cfg_req.which], duty);
			libsoc_pwm_set_period(pwms[op_req->pwm_cfg_req.which], period);
		}
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d config (%dns/%dns) request\n  ",
			    module_id, cport_id, op_req->pwm_cfg_req.which, duty, period);
		break;
	case GB_PWM_TYPE_POLARITY:
		payload_size = 0;
		if (pwm_on[op_req->pwm_pol_req.which]) {
			result = PROTOCOL_STATUS_BUSY;
		} else if (bbb_backend) {
			libsoc_pwm_set_polarity(pwms[op_req->pwm_pol_req.which],
						    op_req->pwm_pol_req.polarity);
		}
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d polarity (%s) request\n  ",
			    module_id,
			    cport_id, op_req->pwm_cfg_req.which,
			    op_req->pwm_pol_req.polarity ? "inverse" : "normal");
		break;
	case GB_PWM_TYPE_ENABLE:
		payload_size = 0;
		pwm_on[op_req->pwm_enb_req.which] = 1;
		if (bbb_backend)
			libsoc_pwm_set_enabled(pwms[op_req->pwm_enb_req.which], ENABLED);
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d enable request\n  ",
			    module_id, cport_id, op_req->pwm_enb_req.which);
		break;
	case GB_PWM_TYPE_DISABLE:
		payload_size = 0;
		pwm_on[op_req->pwm_dis_req.which] = 0;
		if (bbb_backend)
			libsoc_pwm_set_enabled(pwms[op_req->pwm_dis_req.which], DISABLED);
		gbsim_debug("AP -> Module %hhu CPort %hu PWM %d disable request\n  ",
			    module_id, cport_id, op_req->pwm_dis_req.which);
		break;
	default:
		gbsim_error("pwm operation type %02x not supported\n", oph->type);
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

char *pwm_get_operation(uint8_t type)
{
	switch (type) {
	case GB_PWM_TYPE_INVALID:
		return "GB_PWM_TYPE_INVALID";
	case GB_PWM_TYPE_PROTOCOL_VERSION:
		return "GB_PWM_TYPE_PROTOCOL_VERSION";
	case GB_PWM_TYPE_PWM_COUNT:
		return "GB_PWM_TYPE_PWM_COUNT";
	case GB_PWM_TYPE_ACTIVATE:
		return "GB_PWM_TYPE_ACTIVATE";
	case GB_PWM_TYPE_DEACTIVATE:
		return "GB_PWM_TYPE_DEACTIVATE";
	case GB_PWM_TYPE_CONFIG:
		return "GB_PWM_TYPE_CONFIG";
	case GB_PWM_TYPE_POLARITY:
		return "GB_PWM_TYPE_POLARITY";
	case GB_PWM_TYPE_ENABLE:
		return "GB_PWM_TYPE_ENABLE";
	case GB_PWM_TYPE_DISABLE:
		return "GB_PWM_TYPE_DISABLE";
	default:
		return "(Unknown operation)";
	}
}

void pwm_init(void)
{
	if (bbb_backend) {
		/* Grab PWM0A and PWM0B found on P9-31 and P9-29 */
		pwms[0] = libsoc_pwm_request(0, 0, LS_GREEDY);
		pwms[1] = libsoc_pwm_request(0, 1, LS_GREEDY);
	}
}
