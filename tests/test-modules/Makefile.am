AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS =      -I$(top_srcdir)

if WITH_GNU_LD
VERSION_SCRIPT = \
	version.map
OTHER_FLAGS = \
	-Wl,--version-script=$(VERSION_SCRIPT)
endif

BUILT_SOURCES = \
	$(VERSION_SCRIPT)

MODULE_FLAGS = \
	-module -avoid-version $(OTHER_FLAGS) -rpath /foo

check_LTLIBRARIES = \
	a.la \
	b.la \
	pcptest.la

a_la_SOURCES =        a.c 
a_la_LDFLAGS =        $(MODULE_FLAGS)
b_la_SOURCES =        b.c 
b_la_LDFLAGS =        $(MODULE_FLAGS)

pcptest_la_SOURCES =  pcptest.c
pcptest_la_LDFLAGS =  $(MODULE_FLAGS)

$(VERSION_SCRIPT) : 
	(echo  "{ global:";                \
	 echo "    pdsh_module_info;";     \
	 echo "    pdsh_module_priority;"; \
	 echo "  local: *;";               \
	 echo "};") > $(VERSION_SCRIPT)

DISTCLEANFILES = \
	$(VERSION_SCRIPT)
