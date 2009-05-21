##*****************************************************************************
## $Id$
##*****************************************************************************
#  AUTHOR:
#    Mark Grondona <mgrondona@llnl.gov>
#
#  SYNOPSIS:
#    AC_RCMD_RANK_LIST
#
#  DESCRIPTION:
#    Create user 
#
#  WARNINGS:
#    This macro must be placed after AC_PROG_CC or equivalent.
##*****************************************************************************


AC_DEFUN([AC_RCMD_RANK_LIST], [
		AC_ARG_WITH([rcmd-rank-list],
			AS_HELP_STRING([--with-rcmd-rank-list],
				[Specify priority ordered list of rcmd modules. Default is mrsh,rsh,ssh,krb4,qsh,mqsh,exec,xcpu]),
			[ for t in `echo $withval | tr "," " "`; do
			       if echo mrsh,rsh,ssh,krb4,qsh,mqsh,exec,xcpu | grep -q $t; then
				     if test -z "$ac_cv_rcmd_rank_list" ; then
					    ac_cv_rcmd_rank_list=\"$t\"
				      else
				        ac_cv_rcmd_rank_list="$ac_cv_rcmd_rank_list, \"$t\""
				      fi
				   else
				     AC_MSG_ERROR([Invalid rcmd type $t!])
				   fi
			   done 
			], [])

		AC_MSG_CHECKING([rcmd rank list])
		if test -z "$ac_cv_rcmd_rank_list"; then
		   ac_cv_rcmd_rank_list='"mrsh", "rsh", "ssh", "krb4", "qsh", "mqsh", "exec", "xcpu"'
        fi
        AC_MSG_RESULT([$ac_cv_rcmd_rank_list])

	    AC_DEFINE_UNQUOTED(RCMD_RANK_LIST, $ac_cv_rcmd_rank_list,
				[Define to quoted, comma-separated, priority-ordered list of rcmd types])
])
