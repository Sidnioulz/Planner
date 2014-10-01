# Process this file with autoconf to produce a configure script.
# require autoconf 2.54
AC_PREREQ(2.54)

AC_INIT(AUTHORS)
AM_INIT_AUTOMAKE(planner, 0.14.6)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])
AC_CONFIG_HEADERS(config.h)

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS
AM_DISABLE_STATIC
AM_PROG_LIBTOOL

AM_PATH_GLIB_2_0

PLANNER_COMPILE_WARNINGS

IT_PROG_INTLTOOL([0.35.5])

AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
if test x$PKG_CONFIG = xno ; then
  AC_MSG_ERROR([*** pkg-config not found. See http://www.freedesktop.org/software/pkgconfig/])
fi

# Just run true if these don't exist
AC_PATH_PROG(UPDATE_MIME_DATABASE, update-mime-database, true)
AC_PATH_PROG(UPDATE_DESKTOP_DATABASE, update-desktop-database, true)

AC_ARG_ENABLE(update-mimedb,
   AC_HELP_STRING([--disable-update-mimedb],
                   [disable the update-mime-database after install [default=no]]),,
    enable_update_mimedb=yes)
AM_CONDITIONAL(ENABLE_UPDATE_MIMEDB, test x$enable_update_mimedb = xyes)


dnl
dnl GConf
dnl
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
if test x"$GCONFTOOL" = xno; then
  AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi
AM_GCONF_SOURCE_2

dnl  -------------
dnl | API docs    |--------------------------------------------
dnl  -------------
GTK_DOC_CHECK([1.0])

dnl NOTE: We need to use a separate automake conditional for this
dnl       to make this work with the tarballs.
AM_CONDITIONAL(ENABLE_GTK_DOC, test x$enable_gtk_doc = xyes)
dnl -----------------------------------------------------------


dnl ==========================================================================
dnl
dnl Library Dependencies, uses pkg-config to detect correct version, etc
dnl
dnl ==========================================================================

dnl If you add a version number here, you *must* add an AC_SUBST line for
dnl it too, or it will never make it into the spec file!

GLIB_REQUIRED=2.6.0
GTK_REQUIRED=2.24.0
LIBGNOMECANVAS_REQUIRED=2.10.0
LIBGLADE_REQUIRED=2.3.0
GNOME_VFS_REQUIRED=2.10.0
GCONF_REQUIRED=2.10.0
LIBXML_REQUIRED=2.6.27
LIBXSLT_REQUIRED=1.1.23
PYGTK_REQUIRED=2.6.0

dnl  -----------
dnl | Database/GDA check |----------------------------------------------
dnl  -----------
AC_ARG_WITH(database,
	AS_HELP_STRING([--with-database],
		[enable database support @<:@default=no@:>@ (auto/no/gda3)]),
	with_database="$withval", with_database=no)

GDA_VER=
use_gda=no

case "$with_database" in
auto|yes)
	PKG_CHECK_MODULES(GDA, libgda-3.0 >= 1.0, [GDA_VER=30])
	;;
*gda3*)
	PKG_CHECK_MODULES(GDA, libgda-3.0 >= 1.0, [GDA_VER=30])
	;;
*)
	use_gda=no
	;;
esac

if test x$GDA_VER != x; then
	use_gda=yes
fi

AC_SUBST(GDA_VER)
AM_CONDITIONAL(HAVE_GDA, test x$use_gda = xyes)
dnl -----------------------------------------------------------


dnl ================================
dnl | planner                      |---------------------------
dnl ================================
PKG_CHECK_MODULES(PLANNER,
[
	glib-2.0 >= $GLIB_REQUIRED
	gobject-2.0 gmodule-2.0
	gtk+-2.0 >= $GTK_REQUIRED
	libgnomecanvas-2.0 >= $LIBGNOMECANVAS_REQUIRED
	libglade-2.0 >= $LIBGLADE_REQUIRED
	gconf-2.0 >= $GCONF_REQUIRED
])

AC_DEFINE(WITH_GNOME, 1, [Define to 1 to indicate presence of GNOME libraries])

AC_SUBST(PLANNER_CFLAGS)
AC_SUBST(PLANNER_LIBS)
dnl -----------------------------------------------------------

dnl ================================
dnl | libplanner                   |---------------------------
dnl ================================
PKG_CHECK_MODULES(LIBPLANNER,
[
	glib-2.0 >= $GLIB_REQUIRED
	libxml-2.0 >= $LIBXML_REQUIRED
	gobject-2.0 gmodule-2.0
])
dnl -----------------------------------------------------------


dnl ================================
dnl | HTML output file-module      |---------------------------
dnl ================================
PKG_CHECK_MODULES(XSLT, libxslt >= $LIBXSLT_REQUIRED libexslt)
dnl -----------------------------------------------------------



dnl  ------------------
dnl | Scrollkeeper     |---------------------------------------
dnl  ------------------
AC_PATH_PROG(SCROLLKEEPER_CONFIG, scrollkeeper-config,no)
if test x$SCROLLKEEPER_CONFIG = xno; then
  AC_MSG_ERROR(Couldn't find scrollkeeper-config. Please install the scrollkeeper package)
fi


dnl  ------------------
dnl | Language Support |---------------------------------------
dnl  ------------------
GETTEXT_PACKAGE=planner
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The prefix for our gettext translation domains.])

if test "x$prefix" = "xNONE"; then
  GNOMELOCALEDIR=$ac_default_prefix/${DATADIRNAME}/locale
else
  GNOMELOCALEDIR=$prefix/${DATADIRNAME}/locale
fi
AC_DEFINE_UNQUOTED(GNOMELOCALEDIR, "$GNOMELOCALEDIR", [The locale dir to use])
dnl -----------------------------------------------------------


dnl  --------------
dnl | Python setup |--------------------------------------------
dnl  --------------
AC_ARG_ENABLE(python, [  --enable-python        build python bindings [default=auto]], enable_python="$enableval", enable_python=auto)
AC_ARG_ENABLE(python-plugin, [  --enable-python-plugin build python plugin [default=no]], enable_python_plugin="$enableval", enable_python_plugin=no)

if test "x$enable_python" != "xno"; then
    dnl check for python
    have_python=yes
    AM_PATH_PYTHON([2.2])
    AM_CHECK_PYTHON_HEADERS(,have_python=no)
    AC_SUBST(PYGTK_CFLAGS)

    dnl check for pygtk
    if test "x$have_python" = "xyes"; then
      PKG_CHECK_MODULES(PYGTK, pygtk-2.0 >= $PYGTK_REQUIRED, have_python=yes, have_python=no)
    fi
    if test "x$have_python" = "xyes"; then
      AC_PATH_PROG(PYGOBJECT_CODEGEN, pygobject-codegen-2.0, no)
      if test "x$PYGOBJECT_CODEGEN" = xno; then
        AC_MSG_ERROR(could not find pygobject-codegen-2.0 script)
        have_python=no
      fi
    fi
    AC_MSG_CHECKING(for pygtk defs)
    PYGTK_DEFSDIR=`$PKG_CONFIG --variable=defsdir pygtk-2.0`
    AC_SUBST(PYGTK_DEFSDIR)
    AC_MSG_RESULT($PYGTK_DEFSDIR)
else
    have_python=no
fi

AM_CONDITIONAL(HAVE_PYTHON, test x$have_python = xyes)
AM_CONDITIONAL(HAVE_PYTHON_PLUGIN, test x$enable_python_plugin = xyes)
dnl -----------------------------------------------------------

dnl  --------------
dnl | dotnet setup |--------------------------------------------
dnl  --------------
#AC_ARG_ENABLE(dotnet, [  --enable-dotnet        build dotnet bindings [default=auto]], enable_dotnet="$enableval", enable_dotnet=no)

#if test "x$enable_dotnet" != "xno"; then
#    PKG_CHECK_MODULES(,gtk-sharp,have_dotnet=yes,have_dotnet=no)

#    if test "x$have_dotnet" = "xyes"; then
#        AC_PATH_PROG(GAPI_FIXUP, gapi-fixup, no)
#        AC_PATH_PROG(GAPI_CODEGEN, gapi-codegen, no)
#        AC_PATH_PROG(MCS, mcs, no)
#        AC_PATH_PROG(MONO, mono, no)
#    fi
#else
#    have_dotnet=no
#fi

#if test "x$enable_dotnet" = "xyes"; then
#    AC_MSG_CHECKING(for mono environment)
#    if test "x$have_dotnet" = "xno"; then
#        AC_MSG_ERROR([not found])
#    fi
#fi
#AM_CONDITIONAL(HAVE_DOTNET, test x$have_dotnet = xyes)
dnl -----------------------------------------------------------


dnl  -----------------------------
dnl | Evolution Data Server check |----------------------------------------------
dnl  -----------------------------
AC_ARG_ENABLE(eds, [  --enable-eds  build evolution-data-server support [default=no]], enable_eds="$enableval", enable_eds=no)

if test "x$enable_eds" = "xyes" ; then
    EDS_REQUIRED=1.1.0 
    EDS_PACKAGE=1.2
    PKG_CHECK_MODULES(EDS, 
    [
		libecal-$EDS_PACKAGE >= $EDS_REQUIRED
    		libebook-$EDS_PACKAGE >= $EDS_REQUIRED
    ])
    use_eds=yes
else
    use_eds=no
fi
AM_CONDITIONAL(HAVE_EDS_PLUGIN, test x$use_eds = xyes)
dnl -----------------------------------------------------------

dnl  -----------------------------------
dnl | Simple Priority Scheduling check |-----------------------------------------
dnl  -----------------------------------
AC_ARG_ENABLE(simple_priority_scheduling, [  --enable-simple-priority-scheduling       enable a simple priority scheduling in tasks management[default=no]], enable_simple_priority_scheduling="$enableval", enable_simple_priority_scheduling=no)
if test "x$enable_simple_priority_scheduling" = "xyes" ; then
    use_simple_priority_scheduling=yes
    AC_DEFINE(WITH_SIMPLE_PRIORITY_SCHEDULING, 1, [Define to 1 to enable enable a simple priority scheduling in tasks management feature])
else
    use_simple_priority_scheduling=no
fi
AM_CONDITIONAL(HAVE_SIMPLE_PRIORITY_SCHEDULING, test x$use_simple_priority_scheduling = xyes)
dnl -----------------------------------------------------------

dnl  -------------------------------------
dnl | Evolution Data Server Backend check |--------------------------------------
dnl  -------------------------------------
AC_ARG_ENABLE(eds_backend, [  --enable-eds-backend       build evolution-data-server backend support [default=no]], enable_eds_backend="$enableval", enable_eds_backend=no)

if test "x$enable_eds_backend" = "xyes" ; then
    EDS_REQUIRED=1.1.0 
    EDS_PACKAGE=1.2
    EVOLUTION_REQUIRED=2.1.3
    PKG_CHECK_MODULES(EDS_BACKEND, 
    [
		libecal-$EDS_PACKAGE >= $EDS_REQUIRED
    		libedata-cal-$EDS_PACKAGE >= $EDS_REQUIRED
    		libedataserver-$EDS_PACKAGE >= $EDS_REQUIRED
    		evolution-plugin >= $EVOLUTION_REQUIRED
    ])
    extensiondir=`$PKG_CONFIG --variable=extensiondir evolution-data-server-$EDS_PACKAGE`
    AC_SUBST(extensiondir)
    plugindir=`$PKG_CONFIG --variable=plugindir evolution-plugin`
    AC_SUBST(plugindir)
    use_eds_backend=yes
else
    use_eds_backend=no
fi
AM_CONDITIONAL(HAVE_EDS_BACKEND, test x$use_eds_backend = xyes)
dnl -----------------------------------------------------------


dnl This will cause the automake generated makefiles to pass the
dnl correct flags to aclocal.
ACLOCAL_AMFLAGS="\${ACLOCAL_FLAGS}"
AC_SUBST(ACLOCAL_AMFLAGS)


AC_CONFIG_FILES([
Makefile
libplanner/Makefile
src/Makefile
docs/Makefile
docs/libplanner/Makefile
docs/user-guide/Makefile
docs/user-guide/C/Makefile
docs/user-guide/es/Makefile
docs/user-guide/eu/Makefile
docs/sql/Makefile
examples/Makefile
tests/Makefile
tests/files/Makefile
po/Makefile.in
data/Makefile
data/images/Makefile
data/glade/Makefile
data/ui/Makefile
data/dtd/Makefile
data/stylesheets/Makefile
data/mime/Makefile
data/sql/Makefile
python/Makefile
eds-backend/Makefile
eds-backend/planner-source/Makefile
eds-backend/utils/Makefile
libplanner-1.pc
])

AC_OUTPUT([
data/planner.desktop.in
])

echo
echo "Planner prefix               : $prefix"
echo "Build API docs               : $enable_gtk_doc"
echo "Python bindings              : $have_python"
echo "Python plugin                : $enable_python_plugin"
#echo "Dotnet bindings              : $have_dotnet"
echo "Simple priority scheduling   : $use_simple_priority_scheduling"
echo "Database/GDA support         : $use_gda, $GDA_VER"
echo "Evolution Data Server import : $use_eds"
echo "Evolution Data Server backend: $use_eds_backend"
echo

if test "x$use_simple_priority_scheduling" = "xyes" ; then
	echo "*** Note: The use simple priority tasks scheduling is experimental. Don't use it in production systems ***"
fi

if test "x$enable_eds_backend" = "xyes" ; then
	echo "*** Note: The Evolution backend is experimental. Don't use it in production systems ***"
fi

