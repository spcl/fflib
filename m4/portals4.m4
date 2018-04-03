AC_DEFUN([FF_WITH_PORTALS4],
	[AC_MSG_NOTICE([*** checking for Portals 4 ***])
	AC_ARG_WITH(portals4, AC_HELP_STRING([--with-portals4], [Portals 4 installation path]))

	p4path_header=""
	p4found=yes

	old_LDFLAGS=${LDFLAGS}

	if test x"${with_portals4}" != x; then
		p4path_header="${with_portals4}/include/"
		LDFLAGS="${LDFLAGS} -L${with_portals4}/lib/"
	fi


	AC_CHECK_HEADER(${p4path_header}portals4.h, [], [AC_MSG_ERROR([Can't find Portals 4 header]); p4found=no])
	AC_CHECK_LIB([portals], [PtlMEAppend], [], [AC_MSG_ERROR([Can't find Portals 4 lib]); p4found=no])

	if test x"${p4found}" == xyes; then
		CFLAGS="${CFLAGS} -I${p4path_header}"
		CXXFLAGS="${CXXFLAGS} -I${p4path_header}"
	else
		LDFLAGS=${old_LDFLAGS}
	fi

	]
)
