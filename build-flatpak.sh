#!/bin/bash
# Steam-style Flatpak packaging for Shards
# Build externally, package the result

set -e

echo "=== Cleaning up old files ==="
rm -f shards-binary
rm -f com.fragcolor.Shards.yml
rm -f com.fragcolor.Shards.desktop
rm -f shards-icon.png
rm -rf build-dir
rm -rf repo
rm -f generate-flatpak.sh

echo "=== Building Shards with Docker ==="
docker build -f Dockerfile.shards -t shards-builder .

echo "=== Extracting binary from Docker ==="
# Create temporary container and extract the binary
CONTAINER_ID=$(docker create shards-builder)
docker cp $CONTAINER_ID:/usr/local/bin/shards ./shards-binary
docker rm $CONTAINER_ID

echo "=== Creating minimal Flatpak manifest ==="
cat > com.fragcolor.Shards.yml << 'EOF'
app-id: com.fragcolor.Shards
runtime: org.freedesktop.Platform
runtime-version: '24.08'
sdk: org.freedesktop.Sdk
command: shards
finish-args:
  # Full GPU access
  - --device=dri
  - --device=kvm
  - --share=ipc
  - --socket=x11
  - --socket=wayland
  # Audio access
  - --socket=pulseaudio
  # Network for your platform needs
  - --share=network
  # File system access
  - --filesystem=home
  - --filesystem=xdg-documents
  - --filesystem=xdg-download
  # System integration
  - --socket=session-bus
  - --socket=system-bus
  - --talk-name=org.freedesktop.Notifications
  - --talk-name=org.freedesktop.FileManager1
  # OpenGL/Vulkan
  - --env=MESA_LOADER_DRIVER_OVERRIDE=i965
  - --env=__GLX_VENDOR_LIBRARY_NAME=mesa

modules:
  # Runtime dependencies only - no building
  - name: shards-runtime-deps
    buildsystem: simple
    build-commands:
      # Install any additional runtime libraries if needed
      - echo "Installing runtime dependencies"
    sources: []

  # The pre-built Shards binary
  - name: shards
    buildsystem: simple
    build-commands:
      - install -Dm755 shards-binary /app/bin/shards
      # Create desktop file for OS integration
      - mkdir -p /app/share/applications
      - mkdir -p /app/share/icons/hicolor/256x256/apps
    install-commands:
      # Desktop integration
      - install -Dm644 com.fragcolor.Shards.desktop /app/share/applications/com.fragcolor.Shards.desktop
      - install -Dm644 shards-icon.png /app/share/icons/hicolor/256x256/apps/com.fragcolor.Shards.png
    sources:
      - type: file
        path: shards-binary
      - type: file
        path: com.fragcolor.Shards.desktop
      - type: file
        path: shards-icon.png
EOF

echo "=== Creating desktop file ==="
cat > com.fragcolor.Shards.desktop << 'EOF'
[Desktop Entry]
Type=Application
Name=Shards
Comment=Shards Programming Language and Runtime
Exec=shards
Icon=com.fragcolor.Shards
Categories=Development;IDE;
Keywords=programming;language;development;
StartupNotify=true
EOF

echo "=== Creating placeholder icon ==="
# Create a simple placeholder icon (you should replace with actual icon)
convert -size 256x256 xc:blue -pointsize 72 -fill white -gravity center -annotate +0+0 'S' shards-icon.png 2>/dev/null || {
    echo "ImageMagick not found, creating text placeholder"
    echo "Replace shards-icon.png with your actual icon"
    touch shards-icon.png
}

echo "=== Building Flatpak ==="
flatpak-builder --force-clean --repo=repo build-dir com.fragcolor.Shards.yml

echo "=== Installing locally ==="
flatpak --user remote-add --if-not-exists --no-gpg-verify shards-repo repo
flatpak --user install shards-repo com.fragcolor.Shards

echo "=== Done! ==="
echo "Run with: flatpak run com.fragcolor.Shards"
echo "Or it should appear in your application menu as 'Shards'"

rm -f shards-binary
rm -f com.fragcolor.Shards.yml
rm -f com.fragcolor.Shards.desktop
rm -f shards-icon.png
rm -rf build-dir
rm -rf repo
rm -f generate-flatpak.sh