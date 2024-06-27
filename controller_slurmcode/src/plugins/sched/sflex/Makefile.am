# Makefile for sflex scheduler plugin

AUTOMAKE_OPTIONS = foreign

AM_CXXFLAGS = -fexceptions

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -DSLURM_PLUGIN_DEBUG -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = sched_sflex.la

sched_sflex_la_SOURCES = \
			sflex_wrapper.c \
			sflex.c	\
			sflex.h
sched_sflex_la_LDFLAGS = $(PLUGIN_FLAGS)