SUBDIRS = lib 3rdlib network network/dst_http network/dst_client network/voss network/src_http network/src_client
installdir = /home/nemo
#curday = $(shell date '+%Y%m%d')
all:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	echo "Making all in $$list"; \
	(cd $$subdir && make); \
	done;

clean:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	echo "Making all in $$list"; \
	(cd $$subdir && make clean); \
	done;

install:
	rm -rf $(installdir)/*;
	mkdir $(installdir)/bin -p;
	mkdir $(installdir)/log -p;
	mkdir $(installdir)/conf -p;
	mkdir $(installdir)/data -p;
	mkdir $(installdir)/path/tmpdir -p;
	mkdir $(installdir)/path/outdir -p;
	mkdir $(installdir)/path/datadir -p;
	mkdir $(installdir)/path/workdir -p;
	mkdir $(installdir)/path/indir -p;
	mkdir $(installdir)/path/bkdir -p;
	mkdir $(installdir)/path/delfile -p;
	cd network; cp vfs_master $(installdir)/bin; cp vfs_master $(installdir)/bin/vfs_master_voss; cp vfs_master*.conf $(installdir)/conf;
	cd network/dst_http; cp *.so $(installdir)/bin
	cd network/dst_client; cp *.so $(installdir)/bin
	cd network/src_http; cp *.so $(installdir)/bin
	cd network/src_client; cp *.so $(installdir)/bin
	cd network/voss; cp *.so $(installdir)/bin
