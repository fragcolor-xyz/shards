#!/bin/bash
set -e

usage() {
    echo "Usage: $0 [--full] [--arch ARCH]"
    echo "  --full        Also build the full shards image (not just headless)"
    echo "  --arch ARCH   Architecture to build: amd64, arm64, or all (default: amd64)"
    exit 1
}

BUILD_FULL=false
BUILD_ARCH="amd64"
while [[ $# -gt 0 ]]; do
    case $1 in
        --full) BUILD_FULL=true; shift ;;
        --arch) BUILD_ARCH="$2"; shift 2 ;;
        -h|--help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

# Validate arch
case $BUILD_ARCH in
    amd64|arm64|all) ;;
    *) echo "Invalid architecture: $BUILD_ARCH"; usage ;;
esac

# Get the git commit hash
GIT_COMMIT=$(git rev-parse --short HEAD)
echo "Building with git commit: $GIT_COMMIT"
echo "Target architecture: $BUILD_ARCH"

IMAGE_NAME="fragcolor/shards-headless"

# Detect container runtime
if command -v docker &> /dev/null && docker buildx version &> /dev/null; then
    RUNTIME="docker"
    echo "Using Docker with buildx"
elif command -v podman &> /dev/null; then
    RUNTIME="podman"
    echo "Using Podman"
else
    echo "Error: Neither Docker nor Podman found"
    exit 1
fi

build_and_push_docker() {
    local dockerfile=$1
    local image=$2

    # Setup buildx if not already done
    docker buildx create --name multiarch --use 2>/dev/null || true

    if [ "$BUILD_ARCH" = "all" ]; then
        local platforms="linux/amd64,linux/arm64"
    else
        local platforms="linux/$BUILD_ARCH"
    fi

    docker buildx build --platform "$platforms" \
        -f "$dockerfile" \
        --build-arg GIT_COMMIT=$GIT_COMMIT \
        -t "$image:latest" \
        -t "$image:$GIT_COMMIT" \
        --push .
}

build_and_push_podman() {
    local dockerfile=$1
    local image=$2

    if [ "$BUILD_ARCH" = "all" ]; then
        local arches="amd64 arm64"
    else
        local arches="$BUILD_ARCH"
    fi

    # Build for each architecture
    local built_tags=""
    for arch in $arches; do
        echo "Building for linux/$arch..."
        podman build --platform "linux/$arch" \
            -f "$dockerfile" \
            --build-arg GIT_COMMIT=$GIT_COMMIT \
            -t "$image:$GIT_COMMIT-$arch" .
        built_tags="$built_tags $image:$GIT_COMMIT-$arch"
    done

    # Create and push manifest for :latest
    podman manifest rm "$image:latest" 2>/dev/null || true
    podman manifest create "$image:latest" $built_tags
    podman manifest push "$image:latest" "docker://$image:latest"

    # Create and push manifest for :$GIT_COMMIT
    podman manifest rm "$image:$GIT_COMMIT" 2>/dev/null || true
    podman manifest create "$image:$GIT_COMMIT" $built_tags
    podman manifest push "$image:$GIT_COMMIT" "docker://$image:$GIT_COMMIT"

    # Cleanup arch-specific tags
    podman rmi $built_tags 2>/dev/null || true
}

# Build and push headless
echo "Building headless image..."
if [ "$RUNTIME" = "docker" ]; then
    build_and_push_docker "Dockerfile.headless" "$IMAGE_NAME"
else
    build_and_push_podman "Dockerfile.headless" "$IMAGE_NAME"
fi

# Build and push full (optional)
if [ "$BUILD_FULL" = true ]; then
    IMAGE_NAME_FULL="fragcolor/shards"
    echo "Building full image..."
    if [ "$RUNTIME" = "docker" ]; then
        build_and_push_docker "Dockerfile.shards" "$IMAGE_NAME_FULL"
    else
        build_and_push_podman "Dockerfile.shards" "$IMAGE_NAME_FULL"
    fi
    echo "Done! Pushed fragcolor/shards-headless and fragcolor/shards with :latest and :$GIT_COMMIT tags"
else
    echo "Done! Pushed fragcolor/shards-headless:latest and fragcolor/shards-headless:$GIT_COMMIT"
fi
