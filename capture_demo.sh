#!/bin/bash

# Script to capture audio visualizer demo video on Wayland
# Requires: ffmpeg, wf-recorder (for Wayland), pulseaudio-utils

# Configuration
OUTPUT_FILE="visualization_demo.webm"
DURATION=10  # seconds
FPS=60       # capture at 60fps for smooth playback
VISUALIZER="./build/visualizer_optimized"  # Path to visualizer

# Check if visualizer exists
if [ ! -f "$VISUALIZER" ]; then
    echo "Error: Visualizer not found at $VISUALIZER"
    echo "Building visualizer..."
    cmake --build build --target visualizer_optimized || exit 1
fi

echo "Audio Visualizer Demo Capture Script (Wayland)"
echo "=============================================="

# Quick capture mode (if called with --quick)
if [ "$1" = "--quick" ]; then
    echo "Quick capture mode: Starting visualizer and recording..."
    
    # Start visualizer
    $VISUALIZER &
    VIS_PID=$!
    sleep 2
    
    echo "Click on the visualizer window:"
    GEOMETRY=$(slurp)
    
    if [ -z "$GEOMETRY" ]; then
        echo "No window selected"
        kill $VIS_PID 2>/dev/null
        exit 1
    fi
    
    # Record
    echo "Recording for $DURATION seconds..."
    wf-recorder -a -f "$OUTPUT_FILE" -c libvpx-vp9 -p "crf=30" -r $FPS -g "$GEOMETRY" &
    RECORDER_PID=$!
    
    sleep $DURATION
    kill -SIGINT $RECORDER_PID
    wait $RECORDER_PID
    kill $VIS_PID 2>/dev/null
    
    echo "✓ Quick capture complete: $OUTPUT_FILE"
    exit 0
fi

# Method 1: Using wf-recorder (Wayland native)
capture_wfrecorder() {
    echo "Method 1: Capturing with wf-recorder..."
    
    # Start visualizer in background
    echo "Starting visualizer..."
    $VISUALIZER &
    VIS_PID=$!
    
    # Wait for window to appear
    sleep 2
    
    echo "Select the visualizer window by clicking on it..."
    GEOMETRY=$(slurp)
    
    if [ -z "$GEOMETRY" ]; then
        echo "No window selected"
        kill $VIS_PID 2>/dev/null
        return 1
    fi
    
    # Start playing some audio (optional)
    echo "Starting audio playback (optional)..."
    # You can uncomment this to play a test file:
    # mpv --loop your_music.mp3 &
    # AUDIO_PID=$!
    
    echo "Recording for $DURATION seconds..."
    
    # Record selected window with audio
    wf-recorder \
        -a -f "$OUTPUT_FILE" \
        -c libvpx-vp9 -p "crf=30" \
        -r $FPS \
        -g "$GEOMETRY" \
        & 
    
    RECORDER_PID=$!
    sleep $DURATION
    
    # Stop recording
    kill -SIGINT $RECORDER_PID
    wait $RECORDER_PID
    
    # Cleanup
    kill $VIS_PID 2>/dev/null
    # kill $AUDIO_PID 2>/dev/null
}

# Method 2: Using OBS Studio (most reliable for Wayland)
capture_obs() {
    echo "Method 2: Using OBS Studio (recommended for quality)"
    echo "1. Open OBS Studio"
    echo "2. Add Window Capture source → Select visualizer"
    echo "3. Add Audio Output Capture (PulseAudio)"
    echo "4. Settings → Output → Recording Format: WebM"
    echo "5. Start visualizer and record in OBS"
    echo ""
    echo "OBS provides the best quality and reliability on Wayland"
}

# Method 3: Using GPU-accelerated pipewire capture
capture_pipewire() {
    echo "Method 3: PipeWire screen capture with GPU encoding..."
    
    # Check if running under Wayland
    if [ "$XDG_SESSION_TYPE" != "wayland" ]; then
        echo "Error: Not running under Wayland"
        return 1
    fi
    
    # Start visualizer
    echo "Starting visualizer..."
    $VISUALIZER &
    VIS_PID=$!
    sleep 2
    
    echo "Select the visualizer window..."
    GEOMETRY=$(slurp)
    
    if [ -z "$GEOMETRY" ]; then
        echo "No window selected"
        kill $VIS_PID 2>/dev/null
        return 1
    fi
    
    # Use pw-record for audio and wf-recorder for video
    # Record video
    wf-recorder \
        -f "temp_video.mp4" \
        -c h264_nvenc -p "preset=slow" \
        -r $FPS \
        -g "$(slurp)" \
        &
    VIDEO_PID=$!
    
    # Record audio
    pw-record --target=0 temp_audio.wav &
    AUDIO_PID=$!
    
    echo "Recording for $DURATION seconds..."
    sleep $DURATION
    
    # Stop recording
    kill -SIGINT $VIDEO_PID $AUDIO_PID
    wait $VIDEO_PID $AUDIO_PID
    
    # Stop visualizer
    kill $VIS_PID 2>/dev/null
    
    # Merge audio and video
    echo "Merging audio and video..."
    ffmpeg -i temp_video.mp4 -i temp_audio.wav \
        -c:v libvpx-vp9 -crf 30 -b:v 0 \
        -c:a libopus -b:a 128k \
        "$OUTPUT_FILE"
    
    rm temp_video.mp4 temp_audio.wav
}

# Method 4: Simple screenshot sequence
capture_screenshots() {
    echo "Method 4: Capture screenshot sequence (no audio)..."
    
    # Start visualizer
    echo "Starting visualizer..."
    $VISUALIZER &
    VIS_PID=$!
    sleep 2
    
    echo "Select the visualizer window..."
    GEOMETRY=$(slurp)
    
    if [ -z "$GEOMETRY" ]; then
        echo "No window selected"
        kill $VIS_PID 2>/dev/null
        return 1
    fi
    
    mkdir -p frames
    
    # Capture frames
    for i in $(seq -f "%04g" 0 $((DURATION * FPS - 1))); do
        grim -g "$(slurp -d)" "frames/frame_${i}.png"
        sleep $(echo "scale=3; 1/$FPS" | bc)
    done
    
    # Stop visualizer
    kill $VIS_PID 2>/dev/null
    
    # Create video from frames
    ffmpeg -framerate $FPS -i frames/frame_%04d.png \
        -c:v libvpx-vp9 -crf 30 -b:v 0 \
        "$OUTPUT_FILE"
    
    rm -rf frames
}

# Check for required tools
check_requirements() {
    local missing=()
    
    command -v ffmpeg >/dev/null || missing+=("ffmpeg")
    command -v wf-recorder >/dev/null || missing+=("wf-recorder")
    command -v slurp >/dev/null || missing+=("slurp")
    
    if [ ${#missing[@]} -gt 0 ]; then
        echo "Missing required tools: ${missing[*]}"
        echo "Install with: sudo pacman -S ${missing[*]}"
        echo "(or your distro's package manager)"
        return 1
    fi
    return 0
}

# Main menu
echo "Detected: Wayland session with NVIDIA GPU"
echo ""
echo "Choose capture method:"
echo "1. wf-recorder (Wayland native) - recommended"
echo "2. OBS Studio (manual, best quality)"
echo "3. PipeWire + GPU encoding (experimental)"
echo "4. Screenshot sequence (fallback, no audio)"
echo -n "Choice [1-4]: "
read choice

check_requirements || exit 1

case $choice in
    1) capture_wfrecorder ;;
    2) capture_obs ;;
    3) capture_pipewire ;;
    4) capture_screenshots ;;
    *) echo "Invalid choice"; exit 1 ;;
esac

if [ -f "$OUTPUT_FILE" ]; then
    echo "✓ Capture complete: $OUTPUT_FILE"
    echo "  Size: $(du -h "$OUTPUT_FILE" | cut -f1)"
    
    # Generate animated WebP as alternative (smaller file size)
    echo "Generating animated WebP..."
    ffmpeg -i "$OUTPUT_FILE" -vcodec libwebp -filter:v fps=fps=30 -lossless 0 -compression_level 6 -q:v 70 -loop 0 -preset picture -an -vsync 0 visualization_demo.webp
    
    echo "✓ Files generated:"
    echo "  - $OUTPUT_FILE (with audio)"
    echo "  - visualization_demo.webp (animated, no audio, smaller)"
fi