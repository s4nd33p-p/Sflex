/*****************************************************************************\
 *  job_submit_sflex.c - Set sflex functions in job submit request specifications.
 *****************************************************************************
 *  Copyright (C) 2010 Lawrence Livermore National Security.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Morris Jette <jette1@llnl.gov>
 *  CODE-OCEC-09-009. All rights reserved.
 *
 *  This file is part of Slurm, a resource management program.
 *  For details, see <https://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  Slurm is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  Slurm is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Slurm; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "src/interfaces/gres.h"
#include "read_config.h"
#include "../../../scontrol/scontrol.h"
#include "slurm/slurm_errno.h"
#include "src/common/slurm_xlator.h"
#include "src/slurmctld/slurmctld.h"
#include "slurm/slurm.h"
#include "slurm/slurm_errno.h"
#include "src/common/slurm_protocol_api.h"
// #include "src/api/"

#define MAX_ACCTG_FREQUENCY 30

/*
 * These variables are required by the generic plugin interface.  If they
 * are not found in the plugin, the plugin loader will ignore it.
 *
 * plugin_name - a string giving a human-readable description of the
 * plugin.  There is no maximum length, but the symbol must refer to
 * a valid string.
 *
 * plugin_type - a string suggesting the type of the plugin or its
 * applicability to a particular form of data or method of data handling.
 * If the low-level plugin API is used, the contents of this string are
 * unimportant and may be anything.  Slurm uses the higher-level plugin
 * interface which requires this string to be of the form
 *
 *	<application>/<method>
 *
 * where <application> is a description of the intended application of
 * the plugin (e.g., "auth" for Slurm authentication) and <method> is a
 * description of how this plugin satisfies that application.  Slurm will
 * only load authentication plugins if the plugin_type string has a prefix
 * of "auth/".
 *
 * plugin_version - an unsigned 32-bit integer containing the Slurm version
 * (major.minor.micro combined into a single number).
 */
const char plugin_name[]       	= "sflex jobsubmit plugin";
const char plugin_type[]       	= "job_submit/sflex";
const uint32_t plugin_version   = SLURM_VERSION_NUMBER;

extern int init(void)
{
    info("Loaded sflex_jobsubmit plugin");

    return SLURM_SUCCESS;
}
extern int fini(void)
{
    info("Unloaded sflex_jobsubmit plugin");
    return SLURM_SUCCESS;
}
char **build_scontrol_msg(char *nodename, uint64_t new_count) {
    // Allocate memory for an array of strings (argv)
    char **argv = malloc(4 * sizeof(char *));
    if (argv == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for each string in argv
    for (int i = 0; i < 4; i++) {
        argv[i] = malloc(100 * sizeof(char)); // Adjust size as needed
        if (argv[i] == NULL) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
    }

    // Build the scontrol message
    // snprintf(argv[0], 100, "scontrol");
    // snprintf(argv[0], 100, "update");
    snprintf(argv[0], 100, "%s", nodename);
    snprintf(argv[1], 100, "gpu:1(S:0-31),mps:%lu(S:0-31)", new_count); // Adjust format as needed

    return argv;
}

// char *build_scontrol_msg(char *nodename, uint64_t new_count) {
//     // Calculate the maximum length needed for the command
//     size_t cmd_len = strlen("sudo scontrol update NodeName=") + strlen(nodename) +
//                      strlen(" Gres=gpu:1(S:0-31),mps:") + 21; // 21 for max length of uint64_t in string representation

//     // Allocate memory for the command string
//     char *cmd = malloc(cmd_len + 1); // +1 for null terminator
//     if (cmd == NULL) {
//         perror("Memory allocation failed");
//         exit(EXIT_FAILURE);
//     }

//     // Build the scontrol message
//     snprintf(cmd, cmd_len, "scontrol update NodeName=%s Gres=gpu:1(S:0-31),mps:%lu(S:0-31)",
//              nodename, new_count); // Adjust format as needed

//     return cmd;
// }

extern int job_submit(job_desc_msg_t *job_desc, uint32_t submit_uid,
		      char **err_msg)
{
	// return SLURM_SUCCESS;
	node_record_t *current_node_rec;
	node_record_t **global_node_ptr = node_record_table_ptr;	// static variable declared in node_conf.c
	List gres_list_ptr;
	ListIterator gres_iter;
	gres_state_t *current_gres_record;
	gres_node_state_t *current_mps_ptr;
	slurm_conf_t *config_ptr;
	char *cmd;
	char **argv;
	// ListIterator gres_iter;
	// gres_state_t *curr_gres;
    // info("job submit function called in sflex");
	// gres_iter = list_iterator_create(job_desc->gres_list);
	// while (curr_gres = (gres_state_t) list_next(gres_iter)){
	// 	if (strncmp(curr_gres->gres_name,"mps",3)==0) {
	// 		debug2("%s",curr_gres->gres_name);
	// 	}
	// }
	if(job_desc->tres_per_node){
		info("Before Change: Job %s tres_per_node %s",job_desc->name, job_desc->tres_per_node);
		// strcpy(job_desc->tres_per_node, "gres:mps:100");
		
		
		// info("cmd : %s",cmd);
		// system(cmd);
		info("After Change: Job %s tres_per_node %s",job_desc->name, job_desc->tres_per_node);
		for (int index=0;index<node_record_count;index++) {
			current_node_rec = global_node_ptr[index];
			gres_list_ptr = current_node_rec ->gres_list;
			gres_iter = list_iterator_create(gres_list_ptr);
			while(current_gres_record = ( gres_state_t *) list_next(gres_iter)) {
				if (strncmp(current_gres_record -> gres_name,"mps",3)==0) {
					current_mps_ptr = current_gres_record -> gres_data;
					debug2 ( "before change gres is %"PRIu64"",current_mps_ptr->gres_cnt_config);
					current_mps_ptr->gres_cnt_avail = current_mps_ptr->gres_cnt_config - current_mps_ptr->gres_cnt_alloc;
					if (current_mps_ptr->gres_cnt_avail <=80 ) {
						
						argv = build_scontrol_msg("ub-10",700);
						_update_node_gres(argv[0],argv[1]);
						_update_node_gres(argv[0],argv[1]);
					}
					debug2 ("after change gres is %"PRIu64"",current_mps_ptr->gres_cnt_config);
				}
			}
		}
	}
	return SLURM_SUCCESS;
}

extern int job_modify(job_desc_msg_t *job_desc, job_record_t *job_ptr,
		      uint32_t submit_uid, char **err_msg)
{
	// uint16_t acctg_freq = 0;
	// if (job_desc->acctg_freq)
	// 	acctg_freq = atoi(job_desc->acctg_freq);
	// /* This example code will prevent users from setting an accounting
	//  * frequency of less than 30 seconds in order to ensure more precise
	//  *  accounting. Also remove any QOS value set by the user in order
	//  * to use the default value from the database. */
	// if (acctg_freq < MIN_ACCTG_FREQUENCY) {
	// 	info("Changing accounting frequency of modify job %u "
	// 	     "from %u to %u", job_ptr->job_id,
	// 	     job_desc->acctg_freq, MIN_ACCTG_FREQUENCY);
	// 	job_desc->acctg_freq = xstrdup_printf(
	// 		"%d", MIN_ACCTG_FREQUENCY);
	// }

	// if (job_desc->qos) {
	// 	info("Clearing QOS (%s) from modify of job %u",
	// 	     job_desc->qos, job_ptr->job_id);
	// 	xfree(job_desc->qos);
	// }
	char *cmd;
	char **argv;

		// }
	if(job_desc->tres_per_node){
		info("Before Change: Job %s tres_per_node %s",job_desc->name, job_desc->tres_per_node);
		strcpy(job_desc->tres_per_node, "gres:mps:100");
		argv = build_scontrol_msg("ub-10",700);
		scontrol_update_node(2, argv);
		info("After Change: Job %s tres_per_node %s",job_desc->name, job_desc->tres_per_node);
	}

	return SLURM_SUCCESS;
}
