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

check-ci:
  claude -p "Claude, please check the CI for current branch PR, github actions. Somehow grep if there is any error and report pls"
