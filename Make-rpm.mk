##
# Makefile Include for RPM Construction
#   by Chris Dunlap <cdunlap@llnl.gov>
##
# $Id$
##
# REQUIREMENTS:
# - requires project to be under CVS version control
# - requires PACKAGE and VERSION makefile macro definitions to be defined
# - supports optional RELEASE makefile macro definition
# - requires "META" file to reside in the top-level directory of the project
# - requires RPM spec file named "$(PACKAGE).spec.in" or "$(PACKAGE).spec"
#     to reside in the top-level directory of the project
##
# META FILE FORMAT:
# - eep opp ork ah ah
##
# CVS TAG FORMAT:
# - eep opp ork ah ah
##
# NOTES:
# - RPM will be built directly from the CVS repository based on the CVS tag
# - CVS tag will be based on the contents of the META file by default;
#     this allows the version information to be stored and tagged within CVS
# - CVS tag can be specified on the make cmdline to override the default
#     (eg, make rpm tag=foo-1-2-3)
# - CVS "HEAD" tag can be used to build the most recent version in CVS
#     w/o requiring the release to be CVS tagged (eg, make rpm tag=HEAD);
#     this is intended for pre-release testing purposes only
# - CVS "HEAD" releases will have a "+" appended to the version to denote
#     an augmented release; the contents of such a release can be resurrected
#     from CVS by using a CVS date spec "-D" based on the RPM's "Build Date"
#     (eg, rpm -qp --queryformat="%{BUILDTIME:date}\n" foo-1.2.3-1.i386.rpm)
# - RPM will be signed with a PGP/GPG key if one is specified in ~/.rpmmacros
##
# USAGE:
# - update and cvs commit the "META" file in the top-level directory
# - cvs tag/rtag the project with a tag of the form "foo-1-2-3" (foo v1.2.3)
# - make rpm
##

tar rpm:
	@if test -z "$(PACKAGE)"; then \
	  echo "ERROR: Undefined PACKAGE macro definition" 1>&2; exit 0; fi; \
	pkg=$(PACKAGE); \
	if test -z "$(VERSION)"; then \
	  echo "ERROR: Undefined VERSION macro definition" 1>&2; exit 0; fi; \
	ver=$(VERSION); \
	test -n "$(RELEASE)" && rel=$(RELEASE) || unset rel; \
	test -n "$(mkinstalldirs)" \
	  && mkdir="$(mkinstalldirs)" || mkdir="mkdir -p"; \
	name=$$pkg-$$ver$${rel+-$$rel}; \
	test -z "$$tag" && tag=`echo $$name | tr '.' '-'`; \
	test "$$tag" = "HEAD" -o "$$tag" = "BASE" && ver="$$ver+"; \
	name=$$pkg-$$ver$${rel+-$$rel}; \
	tmp=$${TMPDIR-/tmp}/tmp-$$pkg-$$$$; \
	rm -rf $$tmp; \
	if ! $$mkdir $$tmp >/dev/null; then \
	  echo "ERROR: Cannot create \"$$tmp\" dir." 1>&2; exit 1; fi; \
	$(MAKE) -s $@-internal mkdir="$$mkdir" tmp="$$tmp" \
	  tag="$$tag" name="$$name" pkg="$$pkg" ver="$$ver" rel="$${rel-1}" \
	    && rm -rf $$tmp

tar-internal:
	@echo "creating $$name.tgz (tag=$$tag)"; \
	rm -f $$name.tgz || exit 1; \
	test -f CVS/Root && cvs="cvs -d `cat CVS/Root`" || cvs="cvs"; \
	(cd $$tmp; $$cvs -Q export -r $$tag -d $$name $$pkg) || exit 1 \
	  && (cd $$tmp; tar cf - $$name) | gzip -c9 >$$tmp/$$name.tgz; \
	test -z "$(META)" && meta=$$tmp/$$name/META || meta=$(META); \
	if test ! -f "$$meta"; then \
	  echo "ERROR: Cannot find metadata." 1>&2; exit 1; fi; \
	n=`perl -ne 'if (s/^\s*NAME:\s*(\S*).*/\1/i) {print;exit}' $$meta`; \
	if test "$(PACKAGE)" != "$$n"; then \
	  echo "ERROR: PACKAGE does not match metadata." 1>&2; exit 1; fi; \
	v=`perl -ne 'if (s/^\s*VERSION:\s*(\S*).*/\1/i) {print;exit}' $$meta`; \
	if test "$(VERSION)" != "$$v"; then \
	  echo "ERROR: VERSION does not match metadata." 1>&2; exit 1; fi; \
	r=`perl -ne 'if (s/^\s*RELEASE:\s*(\S*).*/\1/i) {print;exit}' $$meta`; \
	if test "$(RELEASE)" != "$$r"; then \
	  echo "ERROR: RELEASE does not match metadata." 1>&2; exit 1; fi; \
	cp -p $$tmp/$$name.tgz $$name.tgz || exit 1

rpm-internal: tar-internal
	@echo "creating $$name*rpm (tag=$$tag)"; \
	for d in BUILD RPMS SOURCES SPECS SRPMS TMP; do \
	  if ! $$mkdir $$tmp/$$d >/dev/null; then \
	    echo "ERROR: Cannot create \"$$tmp/$$d\" dir." 1>&2; exit 1; fi; \
	done; \
	mv $$tmp/$$name.tgz $$tmp/SOURCES/; \
	test -f $$tmp/$$name/$$pkg.spec.in \
	  && spec=$$tmp/$$name/$$pkg.spec.in || spec=$$tmp/$$name/$$pkg.spec; \
	sed -e "s/^\([ 	]*Name:\).*/\1 $$pkg/i" \
	    -e "s/^\([ 	]*Version:\).*/\1 $$ver/i" \
	    -e "s/^\([ 	]*Release:\).*/\1 $$rel/i" \
	    -e "s/^\([ 	]*Source0?:\).*/\1 $$name.tgz/i" \
	    <$$spec >$$tmp/SPECS/$$pkg.spec; \
	if ! test -s $$tmp/SPECS/$$pkg.spec; then \
	  echo "ERROR: Cannot create $$pkg.spec (tag=$$tag)" 1>&2; exit 1; fi; \
	rpm --showrc | egrep "_(gpg|pgp)_name" >/dev/null && sign="--sign"; \
	if ! rpm -ba --define "_tmppath $$tmp/TMP" --define "_topdir $$tmp" \
	  $$sign --quiet $$tmp/SPECS/$$pkg.spec >$$tmp/rpm.log 2>&1; then \
	    cat $$tmp/rpm.log; exit 1; fi; \
	cp -p $$tmp/RPMS/*/$$pkg-*.rpm $$tmp/SRPMS/$$pkg-*.src.rpm .
