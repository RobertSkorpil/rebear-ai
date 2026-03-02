#!/bin/bash

# Base patterns to test
# This script cycles through flipping each bit (0-63) one at a time for each base pattern
# Also runs a no-flip test for each base pattern

# Patch locations
LOCATIONS=(0x200 0x208 0x210 0x218)

# Base patterns as arrays of hex bytes
declare -A BASE_PATTERNS
BASE_PATTERNS[pattern0]="0x2d 0x00 0x2d 0x00 0x4D 0x24 0x59 0x00"
BASE_PATTERNS[pattern1]="0x7f 0xa5 0x6d 0xa5 0x8d 0xc9 0x99 0xa5"
BASE_PATTERNS[pattern2]="0x59 0xb3 0x7f 0xb3 0x7f 0xd3 0x6b 0xb3"
BASE_PATTERNS[pattern3]="0x61 0x7c 0xb5 0x7c 0xc5 0xa0 0xa1 0x7c"
BASE_PATTERNS[pattern4]="0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00"

# Path to rebear-cli executable
REBEAR_CLI="build/cli/rebear-cli"

# Function to convert pattern string to array
pattern_to_array() {
    local pattern_str=$1
    local -a result
    read -ra result <<< "$pattern_str"
    echo "${result[@]}"
}

# Function to get pattern as hex string
get_pattern_hex() {
    local -a pattern=($1)
    printf "%02x%02x%02x%02x%02x%02x%02x%02x" \
        ${pattern[0]} ${pattern[1]} ${pattern[2]} ${pattern[3]} \
        ${pattern[4]} ${pattern[5]} ${pattern[6]} ${pattern[7]}
}

# Function to flip a bit in the pattern
flip_bit() {
    local bit_index=$1
    local pattern_str=$2
    local -a pattern
    read -ra pattern <<< "$pattern_str"
    
    # Determine which byte and which bit within that byte
    local byte_index=$((bit_index / 8))
    local bit_in_byte=$((bit_index % 8))
    
    # Flip the bit
    local original_byte=${pattern[$byte_index]}
    local flipped_byte=$((original_byte ^ (1 << bit_in_byte)))
    pattern[$byte_index]=$flipped_byte
    
    # Return the pattern as hex string (no spaces, no 0x prefix)
    printf "%02x%02x%02x%02x%02x%02x%02x%02x" \
        ${pattern[0]} ${pattern[1]} ${pattern[2]} ${pattern[3]} \
        ${pattern[4]} ${pattern[5]} ${pattern[6]} ${pattern[7]}
}

# Function to run a test cycle
run_test_cycle() {
    local cycle_name=$1
    local pattern=$2
    
    echo "=== $cycle_name ==="
    echo "Pattern: $pattern"
    
    # Clear all patches first
    $REBEAR_CLI patch clear --all
    
    # Apply the same pattern to all four locations
    # Use patch IDs 0-3 for the four locations
    patch_id=0
    for loc in "${LOCATIONS[@]}"; do
        echo "Patching location $loc (ID $patch_id) with: $pattern"
        $REBEAR_CLI patch set --id $patch_id --address $loc --data $pattern
        patch_id=$((patch_id + 1))
    done
    
    # Clear transaction buffer to remove leftover data
    echo "Clearing transaction buffer"
    $REBEAR_CLI clear
    
    # Start monitoring in background, output to file named by cycle
    OUTPUT_FILE="${cycle_name}_${pattern}.log"
    echo "Starting monitor, output to: $OUTPUT_FILE"
    $REBEAR_CLI monitor --duration 1 > "$OUTPUT_FILE" 2>&1 &
    MONITOR_PID=$!
    
    # Click button to stop current playback
    echo "Clicking button (stop)"
    $REBEAR_CLI button click
    
    # Wait 500ms
    sleep 0.5
    
    # Click button again to start next story
    echo "Clicking button (start)"
    $REBEAR_CLI button click
    
    # Wait for monitor to complete
    wait $MONITOR_PID
    
    echo "Cycle complete"
    echo ""
}

# Main execution
echo "Starting test suite with 5 base patterns"
echo "Each pattern will be tested with:"
echo "  - 1 no-flip cycle (base pattern)"
echo "  - 64 bit-flip cycles (one bit flipped at a time)"
echo "Total: 5 x 65 = 325 cycles"
echo ""

# Counter for progress
total_cycles=0

# Loop through each base pattern
for pattern_name in pattern0 pattern1 pattern2 pattern3 pattern4; do
    pattern_str="${BASE_PATTERNS[$pattern_name]}"
    echo "========================================"
    echo "Testing $pattern_name: $pattern_str"
    echo "========================================"
    echo ""
    
    # Run no-flip test first
    echo "--- No-flip test for $pattern_name ---"
    PATTERN=$(get_pattern_hex "$pattern_str")
    run_test_cycle "${pattern_name}_noflip" "$PATTERN"
    total_cycles=$((total_cycles + 1))
    echo "Progress: $total_cycles/325 cycles completed"
    echo ""
    
    # Run bit-flip tests
    for bit in {0..63}; do
        PATTERN=$(flip_bit $bit "$pattern_str")
        run_test_cycle "${pattern_name}_bit${bit}" "$PATTERN"
        total_cycles=$((total_cycles + 1))
        
        # Show progress every 10 cycles
        if [ $((bit % 10)) -eq 9 ]; then
            echo "Progress: $total_cycles/325 cycles completed"
            echo ""
        fi
    done
    
    echo "$pattern_name complete (65 cycles)"
    echo ""
done

echo "========================================"
echo "All tests completed!"
echo "Total cycles: $total_cycles"
echo "========================================"
echo "Clearing all patches..."
$REBEAR_CLI patch clear --all
