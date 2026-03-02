#!/bin/bash

# Base pattern: 2d 00 2d 00 4D 24 59 00
# This script cycles through flipping each bit (0-63) one at a time

# Patch locations
LOCATIONS=(0x200 0x208 0x210 0x218)

# Base pattern as hex bytes
BASE_PATTERN=(0x2d 0x00 0x2d 0x00 0x4D 0x24 0x59 0x00)

# Path to rebear-cli executable
REBEAR_CLI="build/cli/rebear-cli"

# Function to flip a bit in the pattern
flip_bit() {
    local bit_index=$1
    local pattern=("${BASE_PATTERN[@]}")
    
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

# Main loop: cycle through all 64 bits
for bit in {0..63}; do
    echo "=== Cycle $bit: Flipping bit $bit ==="
    
    # Get the pattern with the current bit flipped (no spaces)
    PATTERN=$(flip_bit $bit)
    
    echo "Pattern: $PATTERN"
    
    # Clear all patches first
    $REBEAR_CLI patch clear --all
    
    # Apply the same pattern to all four locations
    # Use patch IDs 0-3 for the four locations
    patch_id=0
    for loc in "${LOCATIONS[@]}"; do
        echo "Patching location $loc (ID $patch_id) with: $PATTERN"
        $REBEAR_CLI patch set --id $patch_id --address $loc --data $PATTERN
        patch_id=$((patch_id + 1))
    done
    
    # Start monitoring in background, output to file named by bit pattern
    OUTPUT_FILE="bit_${bit}_${PATTERN}.log"
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
    
    echo "Cycle $bit complete"
    echo ""
done

echo "All 64 bit-flip cycles completed."
echo "Clearing all patches..."
$REBEAR_CLI patch clear --all
