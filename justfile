set fallback

# pull and update in one go
pull:
  git pull
  ./update.sh

build-docker-image:
  GIT_COMMIT=$(git rev-parse --short HEAD); docker buildx build \
   -f Dockerfile.shards \
   --build-arg GIT_COMMIT=$GIT_COMMIT \
   -t fragcolor/shards:latest \
   -t fragcolor/shards:$GIT_COMMIT \
   --push .