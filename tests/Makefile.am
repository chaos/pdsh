
SUBDIRS = test-modules
CPPFLAGS = \
	-I $(top_srcdir)

TEST_EXTENSIONS = .sh
SH_LOG_DRIVER = \
    env AM_TAP_AWK='$(AWK)' $(SHELL) \
    $(top_srcdir)/config/tap-driver.sh

check_SCRIPTS = \
    t0001-basic.sh \
    t0002-internal.sh \
    t0003-wcoll.sh \
    t0004-module-loading.sh \
    t0005-rcmd_type-and-user.sh \
    t0006-pdcp.sh \
    t1001-genders.sh \
    t1002-dshgroup.sh \
    t1003-slurm.sh \
    t2000-exec.sh \
    t2001-ssh.sh \
    t2002-mrsh.sh \
    t5000-dshbak.sh \
    t6036-long-output-lines.sh

TESTS = \
    $(check_SCRIPTS)

EXTRA_DIST = \
    $(check_SCRIPTS) \
    test-lib.sh \
    aggregate-results.sh

clean-local:
	rm -fr trash-directory.* test-results .prove *.log *.output

