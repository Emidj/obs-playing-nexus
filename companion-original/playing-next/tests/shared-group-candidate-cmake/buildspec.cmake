# Audit build only: use the existing SDK, never download/extract or rewrite
# the external project's .deps directory. Package paths are supplied explicitly.
include_guard(GLOBAL)
message(STATUS "Candidate audit: existing external SDK used read-only")
