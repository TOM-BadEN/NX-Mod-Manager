#!/bin/bash
set -e

# This script is used to request the latest crash report from the PSV homebrew FTP server
# Tested with: https://github.com/devnoname120/vitacompanion
# Author: https://github.com/xfangfang

if [ $# -lt 2 ]; then
  echo -e "This script is used to request the latest crash report from the PSV homebrew FTP server"
  echo -e "Usage: docker run --rm -v "'$(pwd)'":/src xfangfang/vita_parse <ftp_config> <elf_path>"
  echo -e "Example: \n\tdocker run --rm -v "'$(pwd)'":/src xfangfang/vita_parse ftp://192.168.1.140:1337 main.elf"
  exit 1
fi

ftp_server="$1"
elf_path="$2"
reports_dir="ux0:/data"

echo "===>    Using FTP Config: $ftp_server"
echo "===>    Requesting crash report list from $ftp_server/$reports_dir"
crash_report_path="$(curl -s "$ftp_server"/$reports_dir/ | awk '/\.psp2dmp/ {print $9}' | sort -r | head -n 1)"

echo "===>    Requesting latest report: $ftp_server/$reports_dir/$crash_report_path"
curl -o "$crash_report_path" -s "$ftp_server"/$reports_dir/"$crash_report_path"
echo "===>    Saving crash report to $crash_report_path"

python main.py "$crash_report_path" /src/"$elf_path"

rm -f "$crash_report_path"
