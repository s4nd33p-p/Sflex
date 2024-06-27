/*****************************************************************************\
 *  sflex.h - header for alex's sflex scheduler plugin.
\*****************************************************************************/

#ifndef _SLURM_SFLEX_H
#define _SLURM_SFLEX_H

/* sflex_agent - detached thread periodically when pending jobs can start */
extern void *sflex_agent(void *args);

/* Terminate sflex_agent */
extern void stop_sflex_agent(void);

/* Note that slurm.conf has changed */
extern void sflex_reconfig(void);

#endif	/* _SLURM_SFLEX_H */