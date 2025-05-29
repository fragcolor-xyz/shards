# Get the git commit hash
GIT_COMMIT=$(git rev-parse --short HEAD)
echo "Building with git commit: $GIT_COMMIT"

# Setup buildx if not already done
docker buildx create --name multiarch --use

# Build and push headless
docker buildx build --platform linux/amd64,linux/arm64 \
  -f Dockerfile.headless \
  --build-arg GIT_COMMIT=$GIT_COMMIT \
  -t fragcolor/shards-headless:latest \
  -t fragcolor/shards-headless:$GIT_COMMIT \
  --push .

# Build and push full
docker buildx build --platform linux/amd64,linux/arm64 \
  -f Dockerfile.shards \
  --build-arg GIT_COMMIT=$GIT_COMMIT \
  -t fragcolor/shards:latest \
  -t fragcolor/shards:$GIT_COMMIT \
  --push .
