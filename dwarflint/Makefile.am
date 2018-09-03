## Process this file with automake to create Makefile.in
##
## Copyright (C) 1996-2011 Red Hat, Inc.
##
## This file is part of elfutils.
##
## This file is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
##
## elfutils is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##
include $(top_srcdir)/config/eu.am
DEFS += $(YYDEBUG) -DDEBUGPRED=@DEBUGPRED@ \
	-DSRCDIR=\"$(shell cd $(srcdir);pwd)\" -DOBJDIR=\"$(shell pwd)\"
AM_CPPFLAGS += -I$(srcdir)/../libelf -I$(srcdir)/../libebl \
	    -I$(srcdir)/../libdw -I$(srcdir)/../libdwfl \
	    -I$(srcdir)/../libasm

AM_LDFLAGS = -Wl,-rpath-link,../libelf:../libdw

no_mudflap.os = -fmudflap

bin_PROGRAMS = dwarfcheck

dwarfcheck_SOURCES = \
	dwarfcheck.cc \
	die_locus.cc die_locus.hh \
	files.cc files.hh \
	locus.cc locus.hh \
	option.cc option.hh option_i.hh \
	section_id.cc section_id.hh \
	pri.cc pri.hh

if BUILD_STATIC
libasm = ../libasm/libasm.a
libdw = ../libdw/libdw.a $(zip_LIBS) $(libelf) $(libebl) -ldl
libelf = ../libelf/libelf.a
else
libasm = ../libasm/libasm.so
libdw = ../libdw/libdw.so
libelf = ../libelf/libelf.so
endif
libebl = ../libebl/libebl.a
libeu = ../lib/libeu.a

# XXX later the C++ stuff will be in libdw.so directly
libdwpp = ../libdw/libdwpp.a $(libdw)

dwarfcheck_LDADD  = $(libebl) $(libelf) $(libdwpp) $(libeu) $(libmudflap) -ldl

installcheck-binPROGRAMS: $(bin_PROGRAMS)
	bad=0; pid=$$$$; list="$(bin_PROGRAMS)"; for p in $$list; do \
	  case ' $(AM_INSTALLCHECK_STD_OPTIONS_EXEMPT) ' in \
	   *" $$p "* | *" $(srcdir)/$$p "*) continue;; \
	  esac; \
	  f=`echo "$$p" | \
	     sed 's,^.*/,,;s/$(EXEEXT)$$//;$(transform);s/$$/$(EXEEXT)/'`; \
	  for opt in --help --version; do \
	    if LD_LIBRARY_PATH=$(DESTDIR)$(libdir) \
	       $(DESTDIR)$(bindir)/$$f $$opt > c$${pid}_.out 2> c$${pid}_.err \
	         && test -n "`cat c$${pid}_.out`" \
	         && test -z "`cat c$${pid}_.err`"; then :; \
	    else echo "$$f does not support $$opt" 1>&2; bad=1; fi; \
	  done; \
	done; rm -f c$${pid}_.???; exit $$bad

CLEANFILES += *.gconv

MAINTAINERCLEANFILES = 
