##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Al Chu <chu11@llnl.gov>
#
#  SYNOPSIS:
#    AC_STATIC_MODULE
#
#  DESCRIPTION:
#    Output #include files for static modules.
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************

AC_DEFUN([AC_STATIC_MODULES_INIT],
[
 
  rm -f modules/_static_decls
  rm -f modules/_static_pointers
  rm -f modules/_static_names

  # sshcmd and xrcmd always built
  echo "extern struct pdsh_module sshcmd_module;" >> modules/_static_decls        
  echo "&sshcmd_module," >> modules/_static_pointers
  echo "\"sshcmd\"," >> modules/_static_names

  echo "extern struct pdsh_module xrcmd_module;" >> modules/_static_decls        
  echo "&xrcmd_module," >> modules/_static_pointers
  echo "\"xrcmd\"," >> modules/_static_names
])

AC_DEFUN([AC_ADD_STATIC_MODULE],
[
  if test "$ac_static_modules" = "yes" ; then
     echo "extern struct pdsh_module $1_module;" >> modules/_static_decls        
     echo "&$1_module," >> modules/_static_pointers
     echo "\"$1\"," >> modules/_static_names
  fi
])
