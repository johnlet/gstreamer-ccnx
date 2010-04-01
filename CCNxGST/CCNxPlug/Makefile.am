# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libccnx.la
# bin_PROGRAMS=names fetch

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libccnx_la_SOURCES = CCNxPlugin.c ccnsrc.c ccnsink.c utils.c

# names_SOURCES = names.c
# names_CPPFLAGS = -I/home/topgun/test/include
# names_LDADD = -L/home/topgun/test/lib -lccn -ldl -lcrypto
# fetch_SOURCES = fetch.c
# fetch_CPPFLAGS = -I/home/topgun/test/include
# fetch_LDADD = -L/home/topgun/test/lib -lccn -ldl -lcrypto

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libccnx_la_CFLAGS = $(GST_CFLAGS) @CCN_CPP_FLAGS@
libccnx_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(GSTCTRL_LIBS)
libccnx_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) @CCN_LDFLAGS@ -lccn -lcrypto
libccnx_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = CCNxPlugin.h ccnxsink.h ccnxsrc.h conf.h utils.h
