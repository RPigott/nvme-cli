// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022 Solidigm.
 *
 * Author: leonardo.da.cunha@solidigm.com
 */

#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "nvme.h"
#include "libnvme.h"
#include "plugin.h"
#include "nvme-print.h"
#include "solidigm-telemetry.h"
#include "solidigm-telemetry/telemetry-log.h"
#include "solidigm-telemetry/cod.h"
#include "solidigm-telemetry/reason-id.h"

struct config {
	__u32 host_gen;
	bool ctrl_init;
	int	data_area;
};

int solidigm_get_telemetry_log(int argc, char **argv, struct command *cmd, struct plugin *plugin)
{
	const char *desc = "Parse Solidigm Telemetry log";
	const char *hgen = "Controls when to generate new host initiated report. Default value '1' generates new host initiated report, value '0' causes retrieval of existing log.";
	const char *cgen = "Gather report generated by the controller.";
	const char *dgen = "Pick which telemetry data area to report. Default is 3 to fetch areas 1-3. Valid options are 1, 2, 3, 4.";
	struct nvme_dev *dev;

	struct telemetry_log tl = {
		.root = json_create_object(),
	};

	struct config cfg = {
		.host_gen	= 1,
		.ctrl_init	= false,
		.data_area	= 3,
	};

	OPT_ARGS(opts) = {
		OPT_UINT("host-generate",   'g', &cfg.host_gen,  hgen),
		OPT_FLAG("controller-init", 'c', &cfg.ctrl_init, cgen),
		OPT_UINT("data-area",       'd', &cfg.data_area, dgen),
		OPT_END()
	};

	int err = parse_and_open(&dev, argc, argv, desc, opts);
	if (err)
		goto ret;

	if (cfg.host_gen > 1) {
		fprintf(stderr, "Invalid host-generate value '%d'\n", cfg.host_gen);
		err = EINVAL;
		goto close_fd;
	}
	
	if (cfg.ctrl_init)
		err = nvme_get_ctrl_telemetry(dev_fd(dev), true, &tl.log, cfg.data_area, &tl.log_size);
	else if (cfg.host_gen)
		err = nvme_get_new_host_telemetry(dev_fd(dev), &tl.log, cfg.data_area, &tl.log_size);
	else
		err = nvme_get_host_telemetry(dev_fd(dev), &tl.log, cfg.data_area, &tl.log_size);

	if (err < 0) {
		fprintf(stderr, "get-telemetry-log: %s\n",
			nvme_strerror(errno));
		goto close_fd;
	} else if (err > 0) {
		nvme_show_status(err);
		fprintf(stderr, "Failed to acquire telemetry log %d!\n", err);
		goto close_fd;
	}

	solidigm_telemetry_log_reason_id_parse(&tl);
	solidigm_telemetry_log_cod_parse(&tl);
	
	json_print_object(tl.root, NULL);
	json_free_object(tl.root);
	printf("\n");

	free(tl.log);

close_fd:
	dev_close(dev);
ret:
	return err;
}