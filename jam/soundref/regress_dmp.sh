#!/bin/sh
# regress golden audio dump: sh regress_dmp.sh <felucca-base dir> <out.bin>   (a patched copy of tests/regress.c; one job at a time, each render its own segment)
set -e
D=$1; O=$2; SR=$(cd "$(dirname "$0")" && pwd)
cd "$D"
sed -e 's|^#include "hostsim.c"|#include "'"$SR"'/dmp.h"\nstatic int32_t dmp_job;\n#include "hostsim.c"|' \
    -e 's|        uint32_t u = (uint32_t)x;|        uint32_t u = (uint32_t)x; dmp_rec("regress", dmp_job, x);|' \
    -e 's|run_job_body(&jobs\[i + k\]);|dmp_job = (int32_t)(i + k) * 20; fprintf(stderr, "JOB %d %s;", dmp_job, jobs[i + k].name); run_job_body(\&jobs[i + k]);|' tests/regress.c > tests/regress_dmp.c
sed -i 's|run_jobs(J, nj, jobs_at_once);|run_jobs(J, nj, 1);|' tests/regress_dmp.c
cc -O2 -w $CFX -DHOST_MIX_FOLLOWS_MASTER=1 -DDUELL_TEST_MODE=1 -Ibuild/gen_test -Ibuild/gen -Ifirmware/src -Itests -DNPART=4 -o /tmp/regress_dmp_$$ tests/regress_dmp.c -lm
rm -f "$O"; DMP_FILE="$O" DMP_SYNC=1 /tmp/regress_dmp_$$ tests/golden.txt tests/cpu_baseline.txt 2>&1 >/dev/null | tr ";" "grep -a "^JOB" > "$O.names"12" | grep -a "^JOB" > "$O.names" || true
rm -f tests/regress_dmp.c /tmp/regress_dmp_$$
ls -l "$O"
