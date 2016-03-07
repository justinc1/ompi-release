#!/bin/bash

# cd build
# cd orte/tools/orted
# make V=1 orted
#
#depbase=`echo orted.o | sed 's|[^/]*$|.deps/&|;s|\.o$||'`;\
#	gcc -std=gnu99 -DHAVE_CONFIG_H -I. -I../../../../orte/tools/orted -I../../../opal/include -I../../../orte/include -I../../../ompi/include -I../../../oshmem/include -I../../../opal/mca/hwloc/hwloc191/hwloc/include/private/autogen -I../../../opal/mca/hwloc/hwloc191/hwloc/include/hwloc/autogen   -I../../../.. -I../../.. -I../../../../opal/include -I../../../../orte/include -I../../../../ompi/include -I../../../../oshmem/include   -I/home/justin_cinkelj/devel/mikelangelo/ompi-release/opal/mca/event/libevent2021/libevent -I/home/justin_cinkelj/devel/mikelangelo/ompi-release/opal/mca/event/libevent2021/libevent/include -I/home/justin_cinkelj/devel/mikelangelo/ompi-release/build-osv/opal/mca/event/libevent2021/libevent/include  -fPIC -DPIC -g -finline-functions -fno-strict-aliasing -pthread -MT orted.o -MD -MP -MF $depbase.Tpo -c -o orted.o ../../../../orte/tools/orted/orted.c &&\
#	mv -f $depbase.Tpo $depbase.Po
#/bin/bash ../../../libtool  --tag=CC   --mode=link gcc -std=gnu99  -fPIC -DPIC -g -finline-functions -fno-strict-aliasing -pthread   -o orted orted.o ../../../orte/libopen-rte.la ../../../opal/libopen-pal.la -lrt -lm -lutil  
#libtool: link: gcc -std=gnu99 -fPIC -DPIC -g -finline-functions -fno-strict-aliasing -pthread -o .libs/orted orted.o  ../../../orte/.libs/libopen-rte.so ../../../opal/.libs/libopen-pal.so -lrt -lm -lutil -pthread


#cd build-osv/orte/tools/orted/
pushd           orte/tools/orted/ || exit 1

# Capture and edit autotools compile/link cmd.
# Then re-run with -shared flag.
make clean
OUT=`make V=1`
CMD=`echo "$OUT" | grep '^libtool: link: ' | sed 's/^libtool: link: //'`
CMD=`echo "$OUT" | grep '^libtool: link: ' | sed -e 's/^libtool: link: //' -e 's|-o .libs/orted|-o .libs/orted.so|' `
$CMD -shared

popd

ls -la orte/tools/orted/.libs/orted.so
