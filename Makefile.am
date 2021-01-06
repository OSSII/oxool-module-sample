ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src
EXTRA_DIST= sample.spec \
			autogen.sh \
			autoclean.sh \
			src \
			runTimeData \
			sample.xml.in \
			html \
			oxoffice_oxt
distdir = $(PACKAGE)-$(VERSION)

test-build: dist
	rpmbuild -ta @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.gz

create-tag:
	git tag -a v@PACKAGE_VERSION@-@OXOOLMODULE_VERSION_DIST@ -m "Tagging the v@PACKAGE_VERSION@-@OXOOLMODULE_VERSION_DIST@ release."
	git push origin v@PACKAGE_VERSION@-@OXOOLMODULE_VERSION_DIST@

release: dist create-tag
	rpmbuild -ta @PACKAGE_NAME@-@PACKAGE_VERSION@.tar.gz