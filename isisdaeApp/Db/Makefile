TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install) into <top>/db
# databases, templates, substitutions like this
DB += isisdae.db
DB += daecmds.db
DB += dae_diag.db
DB += dae3_parallel.db
DB += dae3_pull.db
DB += ADisisdae.template
DB += veto.db
DB += inst_alias_string_parameters.db
DB += inst_string_parameters.db
DB += inst_real_parameters.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

