# Function to display usage information
usage() {
    echo "Usage: $0 -p <path> -c <command>"
    echo "Commands:"
    echo "  listAll: List all directories with last sync time and status"
    echo "  listMonitored: List all actively monitored directories"
    echo "  listStopped: List all directories no longer being monitored"
    echo "  purge: Delete the target directory or logfile specified in path"
    exit 1
}

# Function to parse log file and extract directory information
parse_log_file() {
    local log_file="$1"
    local command="$2"
    
    if [ ! -f "$log_file" ]; then
        echo "Error: Log file $log_file does not exist"
        exit 1
    fi

    declare -A dir_info
    declare -A active_dirs
    declare -A inactive_dirs

    while IFS= read -r line; do
        # Check for Added directory lines
        if [[ "$line" =~ "Added directory:" ]]; then
            source_dir=$(echo "$line" | awk -F '->' '{print $1}' | awk '{print $NF}' | tr -d ' ')
            target_dir=$(echo "$line" | awk -F '->' '{print $2}' | tr -d ' ')
            timestamp=$(echo "$line" | awk '{print $1 " " $2}' | tr -d '[]')
            
            # Initialize directory info if not exists
            if [ -z "${dir_info[$source_dir]}" ]; then
                dir_info["$source_dir"]="$target_dir|$timestamp|ACTIVE"
                active_dirs["$source_dir"]=1
            fi
        fi
        
            # Check for Monitoring stopped lines
        if [[ "$line" =~ "Monitoring stopped for:" ]] || [[ "$line" =~ "Monitoring stopped for " ]]; then
                
                # Extract source directory
                source_dir=$(echo "$line" | grep -oP '(Monitoring stopped for:? )\K[^ ]+')
                source_dir=$(echo "$source_dir" | sed 's/[[:space:]:]*$//')

                if [ -n "$source_dir" ] && [[ "$source_dir" != ":" ]]; then
                    # Mark as inactive in both tracking methods
                    unset active_dirs["$source_dir"]
                    inactive_dirs["$source_dir"]=1  # This ensures it stays in the list
                    
                    # Update dir_info status if entry exists
                    if [ -n "${dir_info[$source_dir]}" ]; then
                        IFS='|' read -r target timestamp _ <<< "${dir_info[$source_dir]}"
                        dir_info["$source_dir"]="$target|$timestamp|INACTIVE"
                    fi
                fi
        fi
        
        # Check for Sync completed lines
        if [[ "$line" =~ "Sync completed" ]]; then
            source_dir=$(echo "$line" | awk '{print $4}')
            target_dir=$(echo "$line" | awk '{print $6}')
            timestamp=$(echo "$line" | awk '{print $1 " " $2}' | tr -d '[]')
            status="SUCCESS"
            
            if [[ "$line" =~ "Errors:" ]]; then
                errors=$(echo "$line" | awk -F 'Errors:' '{print $2}' | tr -d ' ')
                if [ "$errors" -gt 0 ]; then
                    status="ERROR"
                fi
            fi
            
            dir_info["$source_dir"]="$target_dir|$timestamp|$status"
        fi
        
        # Check for Sync operations with status
        if [[ "$line" =~ "\[(SUCCESS|ERROR|PARTIAL)\]" ]]; then
            source_dir=$(echo "$line" | awk '{print $3}' | tr -d '[]')
            target_dir=$(echo "$line" | awk '{print $5}' | tr -d '[]')
            status=$(echo "$line" | grep -oE '\[(SUCCESS|ERROR|PARTIAL)\]' | tr -d '[]')
            timestamp=$(echo "$line" | awk '{print $1 " " $2}' | tr -d '[]')
            
            if [ -n "$source_dir" ] && [ -n "$target_dir" ]; then
                dir_info["$source_dir"]="$target_dir|$timestamp|$status"
            fi
        fi
    done < "$log_file"

    # Process based on command
    case "$command" in
        listAll)
            echo "Listing all directories:"
            for source in "${!dir_info[@]}"; do
                IFS='|' read -r target timestamp status <<< "${dir_info[$source]}"
                echo "$source -> $target [Last Sync: $timestamp] [$status]"
            done
            ;;
        listMonitored)
            echo "Listing monitored directories:"
            for source in "${!dir_info[@]}"; do
                IFS='|' read -r target timestamp status <<< "${dir_info[$source]}"
                if [[ "$status" == "ACTIVE" ]]; then
                    echo "$source -> $target [Last Sync: $timestamp]"
                fi
            done
            ;;
        listStopped)
            echo "Listing stopped directories:"
            # Print all directories that are in inactive_dirs
            for source in "${!inactive_dirs[@]}"; do
                if [ -n "${dir_info[$source]}" ]; then
                    IFS='|' read -r target timestamp status <<< "${dir_info[$source]}"
                    echo "$source -> $target [Last Sync: $timestamp]"
                else
                    echo "$source -> [No target directory info]"
                fi
            done
            ;;
    esac
}

# Function to purge a directory or file
purge() {
    local path="$1"
    
    if [ -d "$path" ]; then
        echo "Purging backup directory:"
        echo "Deleting $path..."
        rm -rf "$path"
        echo "Purge complete."
    elif [ -f "$path" ]; then
        echo "Purging a log-file:"
        echo "Deleting $path..."
        rm -f "$path"
        echo "Purge complete."
    else
        echo "Error: Path $path does not exist or is not a directory/file"
        exit 1
    fi
}

# Main script execution
while getopts ":p:c:" opt; do
    case $opt in
        p) path="$OPTARG" ;;
        c) command="$OPTARG" ;;
        \?) echo "Invalid option: -$OPTARG" >&2; usage ;;
        :) echo "Option -$OPTARG requires an argument." >&2; usage ;;
    esac
done

# Validate required arguments
if [ -z "$path" ] || [ -z "$command" ]; then
    usage
fi

# Execute the appropriate command
case "$command" in
    listAll|listMonitored|listStopped)
        parse_log_file "$path" "$command"
        ;;
    purge)
        purge "$path"
        ;;
    *)
        echo "Error: Unknown command '$command'"
        usage
        ;;
esac

exit 0