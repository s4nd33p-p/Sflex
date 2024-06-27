/*****************************************************************************\
 *  sflex_wrapper.c - plugin for Slurm's internal scheduler.
\*****************************************************************************/

#include <stdio.h>

#include "slurm/slurm_errno.h"

#include "src/common/plugin.h"
#include "src/common/log.h"
// #include "src/common/node_select.h"
// #include "src/common/slurm_priority.h"
#include "src/slurmctld/job_scheduler.h"
#include "src/slurmctld/reservation.h"
#include "src/slurmctld/slurmctld.h"
#include "src/plugins/sched/sflex/sflex.h"

const char		plugin_name[]	= "sflex's Scheduler plugin";
const char		plugin_type[]	= "sched/sflex";
const uint32_t	plugin_version	= SLURM_VERSION_NUMBER;

static pthread_t sflex_thread = 0;
static pthread_mutex_t thread_flag_mutex = PTHREAD_MUTEX_INITIALIZER;

int init(void)
{
	sched_verbose("sflex scheduler plugin loaded");
	slurm_mutex_lock( &thread_flag_mutex );
	if ( sflex_thread ) {
		debug2( "sflex scheduler thread already running, "
			"not starting another" );
		slurm_mutex_unlock( &thread_flag_mutex );
		return SLURM_ERROR;
	}

	slurm_thread_create(&sflex_thread, sflex_agent, NULL);

	slurm_mutex_unlock( &thread_flag_mutex );

	return SLURM_SUCCESS;
}

void fini(void)
{
	slurm_mutex_lock( &thread_flag_mutex );
	if ( sflex_thread ) {
		verbose( "sflex scheduler plugin shutting down" );
		stop_sflex_agent();
		pthread_join(sflex_thread, NULL);
		sflex_thread = 0;
	}
	slurm_mutex_unlock( &thread_flag_mutex );
}

int sched_p_reconfig(void)
{
	sflex_reconfig();
	return SLURM_SUCCESS;
}