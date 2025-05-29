# Setup buildx if not already done
docker buildx create --name multiarch --use

# Build and push headless
docker buildx build --platform linux/amd64,linux/arm64 \
  -f Dockerfile.headless -t fragcolor/shards-headless:latest --push .

# Build and push full
docker buildx build --platform linux/amd64,linux/arm64 \
  -f Dockerfile.shards -t fragcolor/shards:latest --push .
