##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Mark Grondona <mgrondona@llnl.gov>
#
#  SYNOPSIS:
#    AC_SLURM
#
#  DESCRIPTION:
#    Checks for whether to include slurm module
#
##*****************************************************************************

AC_DEFUN([AC_SLURM],
[
  #
  # Check for whether to build slurm module
  # 
  AC_MSG_CHECKING([for whether to build slurm module])
  AC_ARG_WITH([slurm],
    AS_HELP_STRING([--with-slurm],[support running pdsh under SLURM allocation]),
      [ case "$withval" in
        yes) ac_with_slurm=yes ;;
        no)  ac_with_slurm=no ;;
        *)   AC_MSG_RESULT([doh!])
             AC_MSG_ERROR([bad value "$withval" for --with-slurm]) ;;
      esac
    ]
  )
  AC_MSG_RESULT([${ac_with_slurm=no}]) 

  if test "$ac_with_slurm" = "yes"; then
     AC_CHECK_LIB(slurm, slurm_load_jobs, [ac_have_libslurm=yes], [])
	
     if test "$ac_have_libslurm" != "yes"; then
        AC_MSG_NOTICE([Cannot support slurm without libslurm.])
     else
        ac_have_slurm=yes
        AC_ADD_STATIC_MODULE("slurm")
        SLURM_LIBS="-lslurm"
        AC_DEFINE([HAVE_SLURM], [1], [Define if you have slurm.])
     fi
  fi

  AC_SUBST(HAVE_SLURM)
  AC_SUBST(SLURM_LIBS)      
])
