#!/bin/bash
# Memory monitoring script for CI pipeline
# Usage: monitor_memory.sh <PID> <output_csv> [--tree]
#
# Options:
#   --tree    Monitor entire process tree (for processes like snakemake that spawn children)

PID=$1
OUTPUT=$2
TRACK_TREE=$3

if [ -z "$PID" ] || [ -z "$OUTPUT" ]; then
    echo "Usage: $0 <PID> <output_csv> [--tree]"
    exit 1
fi

echo "time_sec,rss_mb,vms_mb" > "$OUTPUT"
START=$(date +%s)

# Function to get memory for single process
get_single_memory() {
    local pid=$1
    if [ -f /proc/$pid/status ]; then
        local rss=$(grep VmRSS /proc/$pid/status 2>/dev/null | awk '{print $2}')
        local vms=$(grep VmSize /proc/$pid/status 2>/dev/null | awk '{print $2}')
        echo "${rss:-0} ${vms:-0}"
    fi
}

# Function to get total memory for process tree
get_tree_memory() {
    local parent_pid=$1
    local total_rss=0
    local total_vms=0

    # Get all processes with parent PID (direct children and grandchildren)
    # Using pgrep with -P flag gets direct children, then recursively get their children
    local all_pids=$(pgrep -P $parent_pid 2>/dev/null)
    all_pids="$parent_pid $all_pids"

    for pid in $all_pids; do
        if [ -f /proc/$pid/status ]; then
            local rss=$(grep VmRSS /proc/$pid/status 2>/dev/null | awk '{print $2}')
            local vms=$(grep VmSize /proc/$pid/status 2>/dev/null | awk '{print $2}')
            total_rss=$((total_rss + ${rss:-0}))
            total_vms=$((total_vms + ${vms:-0}))
        fi
    done

    echo "$total_rss $total_vms"
}

# Main monitoring loop
while kill -0 $PID 2>/dev/null; do
    CURRENT=$(date +%s)
    ELAPSED=$((CURRENT - START))

    if [ "$TRACK_TREE" == "--tree" ]; then
        # Monitor entire process tree
        read RSS VMS <<< "$(get_tree_memory $PID)"
    else
        # Monitor single process
        read RSS VMS <<< "$(get_single_memory $PID)"
    fi

    echo "$ELAPSED,$((${RSS:-0}/1024)),$((${VMS:-0}/1024))" >> "$OUTPUT"
    sleep 0.5
done
