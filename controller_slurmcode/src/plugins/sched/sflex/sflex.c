/*****************************************************************************\
 *  sflex.c - Implementation of sflex's  scheduler plugin
\*****************************************************************************/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "slurm/slurm.h"
#include "slurm/slurm_errno.h"

#include "src/common/list.h"
#include "src/common/macros.h"
#include "src/common/parse_time.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/node_conf.h"

#include "src/interfaces/burst_buffer.h"
#include "src/interfaces/preempt.h"
#include "src/interfaces/select.h"
#include "src/interfaces/gres.h"

#include "src/slurmctld/locks.h"
#include "src/slurmctld/reservation.h"
#include "src/slurmctld/slurmctld.h"
#include "src/plugins/sched/sflex/sflex.h"

#ifndef SFLEX_INTERVAL
#  define SFLEX_INTERVAL   2
#endif

#ifndef DEFAULT_SFLEX_PARAMS
#  define DEFAULT_SFLEX_PARAMS       "n:c:m:t"
#endif

/*********************** local variables *********************/
static bool stop_sflex = false;
static pthread_mutex_t term_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  term_cond = PTHREAD_COND_INITIALIZER;
static bool config_flag = false;
static int sflex_interval = SFLEX_INTERVAL;
char *sflex_params = DEFAULT_SFLEX_PARAMS;
static int max_sched_job_cnt = 1;
static int sched_timeout = 0;

/*********************** local functions *********************/
static void _begin_scheduling(void);
static void _load_config(void);
static void _my_sleep(int secs);

/* Terminate sflex_agent */
extern void stop_sflex_agent(void)
{
	slurm_mutex_lock(&term_lock);
	stop_sflex = true;
	slurm_cond_signal(&term_cond);
	slurm_mutex_unlock(&term_lock);
}

static void _my_sleep(int secs)
{
	struct timespec ts = {0, 0};
	struct timeval now;

	gettimeofday(&now, NULL);
	ts.tv_sec = now.tv_sec + secs;
	ts.tv_nsec = now.tv_usec * 1000;
	slurm_mutex_lock(&term_lock);
	if (!stop_sflex)
		slurm_cond_timedwait(&term_cond, &term_lock, &ts);
	slurm_mutex_unlock(&term_lock);
}

static void _load_config(void)
{
	char *sched_params, *select_type, *tmp_ptr;

	sched_timeout = slurm_conf.msg_timeout / 2;
	sched_timeout = MAX(sched_timeout, 1);
	sched_timeout = MIN(sched_timeout, 10);

	sched_params = slurm_conf.sched_params;

	if (sched_params && (tmp_ptr=strstr(sched_params, "interval=")))
		sflex_interval = atoi(tmp_ptr + 9);
	if (sflex_interval < 1) {
		error("Invalid SchedulerParameters interval: %d",
		      sflex_interval);
		sflex_interval = SFLEX_INTERVAL;
	}

	if (sched_params && (tmp_ptr=strstr(sched_params, "max_job_bf=")))
		max_sched_job_cnt = atoi(tmp_ptr + 11);
	if (sched_params && (tmp_ptr=strstr(sched_params, "bf_max_job_test=")))
		max_sched_job_cnt = atoi(tmp_ptr + 16);
	if (max_sched_job_cnt < 1) {
		error("Invalid SchedulerParameters bf_max_job_test: %d",
		      max_sched_job_cnt);
		max_sched_job_cnt = 50;
	}
	xfree(sched_params);

	select_type = slurm_get_select_type();
	if (!xstrcmp(select_type, "select/serial")) {
		max_sched_job_cnt = 0;
		stop_sflex_agent();
	}
	xfree(select_type);

	// sflex_params = slurm_conf.;
	// debug2("SFLEX: loaded sflex params: %s", sflex_params);
}

static void _begin_scheduling(void)
{
	int j, rc = SLURM_SUCCESS, job_cnt = 0;
	sched_debug2("came inside begin scheduling");
	List job_queue;
	job_queue_rec_t *job_queue_rec;
	job_record_t *job_ptr;
	part_record_t *part_ptr;
	bitstr_t *alloc_bitmap = NULL, *avail_bitmap = NULL;
	bitstr_t *exc_core_bitmap = NULL;
	uint32_t max_nodes, min_nodes, req_nodes, time_limit;
	time_t now = time(NULL), sched_start, last_job_alloc;
	bool resv_overlap = false;
	node_record_t **global_node_ptr = node_record_table_ptr;	// static variable declared in node_conf.c
	List gres_list_ptr;
	ListIterator gres_iter;
	gres_state_t *current_gres_record;
	gres_node_state_t *current_mps_ptr;
	int issflexjob=0;

	sched_start = now;
	last_job_alloc = now - 1;
	alloc_bitmap = bit_alloc(node_record_count);
	job_queue = build_job_queue(true, false);
	sort_job_queue(job_queue);
	while ((job_queue_rec = (job_queue_rec_t *) list_pop(job_queue))) {
		job_ptr  = job_queue_rec->job_ptr;
		part_ptr = job_queue_rec->part_ptr;
		xfree(job_queue_rec);
		if (part_ptr != job_ptr->part_ptr)
			continue;	/* Only test one partition */

		if (job_cnt++ > max_sched_job_cnt) {
			debug2("scheduling loop exiting after %d jobs",
			       max_sched_job_cnt);
			break;
		}

		if(IS_JOB_PENDING(job_ptr)) {
			debug2("sched nodes %s",job_ptr->sched_nodes);
		}

		
		/* Determine minimum and maximum node counts */
		/* On BlueGene systems don't adjust the min/max node limits
		   here.  We are working on midplane values. */
		min_nodes = MAX(job_ptr->details->min_nodes,
				part_ptr->min_nodes);

		if (job_ptr->details->max_nodes == 0)
			max_nodes = part_ptr->max_nodes;
		else
			max_nodes = MIN(job_ptr->details->max_nodes,
					part_ptr->max_nodes);

		max_nodes = MIN(max_nodes, 500000);     /* prevent overflows */

		if (job_ptr->details->max_nodes)
			req_nodes = max_nodes;
		else
			req_nodes = min_nodes;

		if (min_nodes > max_nodes) {
			/* job's min_nodes exceeds partition's max_nodes */
			continue;
		}

		for ( int index=0; index<node_record_count; index++) {
		node_record_t *current_node_record = global_node_ptr[index];
		// debug2("node hostname is %s",current_node_record->node_hostname);
		// debug2(" gres this node has %s", current_node_record->gres);
		gres_list_ptr = current_node_record->gres_list;
		gres_iter = list_iterator_create(gres_list_ptr);
		
		while ((current_gres_record = ( gres_state_t *) list_next(gres_iter))) {
			debug2("gres name is %s",current_gres_record->gres_name);
			if (strncmp(current_gres_record -> gres_name,"mps",3)==0) {
				issflexjob=1;
				current_mps_ptr = current_gres_record -> gres_data;
				debug2("Allocated MPS fraction on %s is %"PRIu64"", current_node_record->node_hostname,
																	current_mps_ptr->gres_cnt_alloc);
				debug2("Availabe MPS fraction on %s is %"PRIu64"", current_node_record->node_hostname,
																   (current_mps_ptr->gres_cnt_found - current_mps_ptr->gres_cnt_alloc));
				
				// does not matter about the allocated or available fractions
				// lets launch the job on suitable node.
				// dump_job_desc(job_ptr->);
																	   
			}

		}
	}
	debug2("its not the fault here");

	if (issflexjob==1) {
		j = job_test_resv(job_ptr, &now, true, &avail_bitmap,
				  &exc_core_bitmap, &resv_overlap, false);
		if (j != SLURM_SUCCESS) {
			FREE_NULL_BITMAP(avail_bitmap);
			FREE_NULL_BITMAP(exc_core_bitmap);
			continue;
		}

		rc = select_g_job_test(job_ptr, avail_bitmap,
				       min_nodes, max_nodes, req_nodes,
				       SELECT_MODE_RUN_NOW,
				       NULL, NULL,
				       exc_core_bitmap);
	}

		if (rc == SLURM_SUCCESS) {
			last_job_update = now;
			if (job_ptr->time_limit == INFINITE)
				time_limit = 365 * 24 * 60 * 60;
			else if (job_ptr->time_limit != NO_VAL)
				time_limit = job_ptr->time_limit * 60;
			else if (job_ptr->part_ptr &&
				 (job_ptr->part_ptr->max_time != INFINITE))
				time_limit = job_ptr->part_ptr->max_time * 60;
			else
				time_limit = 365 * 24 * 60 * 60;
			if (bit_overlap_any(alloc_bitmap, avail_bitmap) &&
			    (job_ptr->start_time <= last_job_alloc)) {
				job_ptr->start_time = last_job_alloc;
			}
			bit_or(alloc_bitmap, avail_bitmap);
			last_job_alloc = job_ptr->start_time + time_limit;
		}
		FREE_NULL_BITMAP(avail_bitmap);
		FREE_NULL_BITMAP(exc_core_bitmap);

		if ((time(NULL) - sched_start) >= sched_timeout) {
			debug2("scheduling loop exiting after %d jobs",
			       max_sched_job_cnt);
			break;
		}
	}
	FREE_NULL_LIST(job_queue);
	FREE_NULL_BITMAP(alloc_bitmap);
}

/* Note that slurm.conf has changed */
extern void sflex_reconfig(void)
{
	config_flag = true;
}

/* sflex_agent - detached thread */
extern void *sflex_agent(void *args)
{
	time_t now;
	double wait_time;
	static time_t last_sched_time = 0;
	/* Read config, nodes and partitions; Write jobs */
	slurmctld_lock_t all_locks = {
		READ_LOCK, WRITE_LOCK, READ_LOCK, READ_LOCK, READ_LOCK };

	_load_config();
	last_sched_time = time(NULL);
	while (!stop_sflex) {
		_my_sleep(sflex_interval);
		if (stop_sflex)
			break;
		if (config_flag) {
			config_flag = false;
			_load_config();
		}
		now = time(NULL);
		wait_time = difftime(now, last_sched_time);
		if ((wait_time < sflex_interval))
			continue;

		lock_slurmctld(all_locks);
        _begin_scheduling();
		last_sched_time = time(NULL);
		(void) bb_g_job_try_stage_in();
		unlock_slurmctld(all_locks);
	}
	return NULL;
}