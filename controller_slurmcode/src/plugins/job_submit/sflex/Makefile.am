# Makefile for job_submit/job_submit_sflex plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -DSLURM_PLUGIN_DEBUG -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = job_submit_sflex.la

# Job submit sflex_jobsubmit plugin.
job_submit_sflex_la_SOURCES = job_submit_sflex.c
job_submit_sflex_la_LDFLAGS = $(PLUGIN_FLAGS)
