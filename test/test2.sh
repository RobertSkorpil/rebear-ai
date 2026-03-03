#!/bin/bash

# Test script: Fill four locations with the same base pattern,
# then at address 0x230, test each base pattern with bit flips (0-63)

# Fixed patch locations and their pattern (same pattern for all four)
FIXED_LOCATIONS=(0x200 0x208 0x210 0x218)
FIXED_PATTERN="2d002d004d245900"

# Target address for bit-flipping
FLIP_ADDRESS=0x230

# Base patterns at the flip address
declare -A FLIP_BASES
FLIP_BASES[zeros]="0x00 0x00 0x00 0x00 0x00 0x00 0x00 0x00"
FLIP_BASES[pattern1]="0xfc 0x0d 0x3c 0x5b 0x87 0x20 0x9f 0xf5"

# Path to rebear-cli executable
REBEAR_CLI="build/cli/rebear-cli"

# Function to get pattern as hex string
get_pattern_hex() {
    local -a pattern=($1)
    printf "%02x%02x%02x%02x%02x%02x%02x%02x" \
        ${pattern[0]} ${pattern[1]} ${pattern[2]} ${pattern[3]} \
        ${pattern[4]} ${pattern[5]} ${pattern[6]} ${pattern[7]}
}

# Function to flip a bit in the pattern and return hex string
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
    local flip_pattern=$2
    
    echo "=== $cycle_name ==="
    echo "Flip pattern at $FLIP_ADDRESS: $flip_pattern"
    
    # Clear all patches first
    $REBEAR_CLI patch clear --all
    
    # Apply the fixed pattern to the four locations (patch IDs 0-3)
    patch_id=0
    for loc in "${FIXED_LOCATIONS[@]}"; do
        echo "Patching location $loc (ID $patch_id) with: $FIXED_PATTERN"
        $REBEAR_CLI patch set --id $patch_id --address $loc --data $FIXED_PATTERN
        patch_id=$((patch_id + 1))
    done
    
    # Apply the flip pattern at the target address (patch ID 4)
    echo "Patching location $FLIP_ADDRESS (ID $patch_id) with: $flip_pattern"
    $REBEAR_CLI patch set --id $patch_id --address $FLIP_ADDRESS --data $flip_pattern
    
    # Clear transaction buffer to remove leftover data
    echo "Clearing transaction buffer"
    $REBEAR_CLI clear
    
    # Start monitoring in background, output to file named by cycle
    OUTPUT_FILE="${cycle_name}_${flip_pattern}.log"
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
TOTAL_PATTERNS=${#FLIP_BASES[@]}
TOTAL_CYCLES=$((TOTAL_PATTERNS * 65))

echo "Starting test suite"
echo "Fixed pattern at 0x200/0x208/0x210/0x218: $FIXED_PATTERN"
echo "Flipping bits at address $FLIP_ADDRESS"
echo "Base patterns: $TOTAL_PATTERNS"
echo "  - 1 no-flip cycle + 64 bit-flip cycles per pattern"
echo "Total: $TOTAL_CYCLES cycles"
echo ""

# Counter for progress
total_cycles=0

for base_name in zeros pattern1; do
    FLIP_BASE="${FLIP_BASES[$base_name]}"
    BASE_HEX=$(get_pattern_hex "$FLIP_BASE")
    
    echo "========================================"
    echo "Bit-flip test at $FLIP_ADDRESS — base: $base_name ($BASE_HEX)"
    echo "========================================"
    echo ""
    
    # Run no-flip test first
    echo "--- No-flip test ($base_name) ---"
    run_test_cycle "${base_name}_noflip" "$BASE_HEX"
    total_cycles=$((total_cycles + 1))
    echo "Progress: $total_cycles/$TOTAL_CYCLES cycles completed"
    echo ""
    
    # Run bit-flip tests
    for bit in {0..63}; do
        PATTERN=$(flip_bit $bit "$FLIP_BASE")
        run_test_cycle "${base_name}_bit${bit}" "$PATTERN"
        total_cycles=$((total_cycles + 1))
        
        # Show progress every 10 cycles
        if [ $((bit % 10)) -eq 9 ]; then
            echo "Progress: $total_cycles/$TOTAL_CYCLES cycles completed"
            echo ""
        fi
    done
    
    echo "$base_name complete (65 cycles)"
    echo ""
done

echo "========================================"
echo "All tests completed!"
echo "Total cycles: $total_cycles"
echo "========================================"
echo "Clearing all patches..."
$REBEAR_CLI patch clear --all
