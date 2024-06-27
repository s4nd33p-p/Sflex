# Makefile for select/cons_sflex plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -DSLURM_PLUGIN_DEBUG -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = select_cons_sflex.la

# Trackable RESources selection plugin.
select_cons_sflex_la_SOURCES =	select_cons_sflex.c select_cons_sflex.h
select_cons_sflex_la_LDFLAGS = $(PLUGIN_FLAGS)
select_cons_sflex_la_LIBADD = ../cons_common/libcons_common.la

force:
$(select_cons_sflex_la_LIBADD) : force
	@cd `dirname $@` && $(MAKE) `basename $@`
