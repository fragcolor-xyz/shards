# Shards Flatpak Build Instructions

## Prerequisites

1. **Install Flatpak and flatpak-builder:**
   ```bash
   # Ubuntu/Debian
   sudo apt install flatpak flatpak-builder
   
   # Fedora
   sudo dnf install flatpak flatpak-builder
   ```

2. **Add Flathub repository:**
   ```bash
   flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
   ```

3. **Install required runtime and SDK:**
   ```bash
   flatpak install flathub org.freedesktop.Platform//24.08
   flatpak install flathub org.freedesktop.Sdk//24.08
   ```

4. **Ensure Docker is running:**
   ```bash
   docker --version
   # Should show Docker version
   ```

## Build Process

1. **Navigate to your Shards directory:**
   ```bash
   cd /Users/sugar/devel/shards
   ```

2. **Run the build script:**
   ```bash
   ./build-flatpak.sh
   ```

   This will:
   - Clean up any old build artifacts
   - Build Shards using your existing Docker setup
   - Extract the binary from the Docker container
   - Create a minimal Flatpak manifest
   - Generate desktop integration files
   - Build the Flatpak package
   - Install it locally for testing

## Testing

1. **Run Shards via Flatpak:**
   ```bash
   flatpak run com.fragcolor.Shards
   ```

2. **Or find it in your application menu** - it should appear as "Shards"

3. **Check if it has proper GPU access:**
   ```bash
   flatpak run --command=sh com.fragcolor.Shards
   # Inside the container:
   glxinfo | grep "OpenGL renderer"
   ```

## Distribution

1. **Export the Flatpak for distribution:**
   ```bash
   flatpak build-export repo build-dir
   flatpak build-bundle repo shards.flatpak com.fragcolor.Shards
   ```

2. **Users can install with:**
   ```bash
   flatpak install shards.flatpak
   ```

## Troubleshooting

### If Docker build fails:
- Ensure Docker daemon is running
- Check if you have enough disk space
- Verify Dockerfile.shards exists

### If Flatpak build fails:
- Check that runtime/SDK are installed: `flatpak list --runtime`
- Ensure you have write permissions in the directory
- Try with `--verbose` flag: `flatpak-builder --verbose --force-clean build-dir com.fragcolor.Shards.yml`

### If GPU access doesn't work:
- Check graphics drivers are properly installed
- Verify with: `flatpak run --command=glxinfo com.fragcolor.Shards`
- On NVIDIA, you may need: `flatpak install flathub org.freedesktop.Platform.GL.nvidia`

### If audio doesn't work:
- Ensure PulseAudio/PipeWire is running
- Check with: `flatpak run --command=pactl com.fragcolor.Shards info`

## Customization

### To add your own icon:
Replace the placeholder `shards-icon.png` with a 256x256 PNG icon before running the build script.

### To modify permissions:
Edit the `finish-args` section in the generated `com.fragcolor.Shards.yml` file.

### To add runtime dependencies:
Add them to the `shards-runtime-deps` module in the manifest.

## Clean Up

To remove everything:
```bash
flatpak uninstall com.fragcolor.Shards
flatpak remote-delete shards-repo
rm -rf build-dir repo shards-binary *.yml *.desktop *.png
```