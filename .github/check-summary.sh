#!/bin/sh
# Fails unless a course test script's log ends with "OK Tests: N out of N", and lists the failures.
log=$1
summary=$(grep '^OK Tests:' "$log")
[ -n "$summary" ] || { echo "No summary line in $log"; exit 1; }
ok=$(echo "$summary" | awk '{print $3}')
total=$(echo "$summary" | awk '{print $6}')
echo "### $summary" >> "${GITHUB_STEP_SUMMARY:-/dev/null}"
if [ "$ok" != "$total" ]; then
  echo "Failing tests:"
  grep -E '\sFAILED' "$log" | tee -a "${GITHUB_STEP_SUMMARY:-/dev/null}"
  exit 1
fi
