set fallback

rust_toolchain := `cat rust.version`

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

# configure cmake in build/Debug
configure:
  cmake -GNinja -B build/Debug -DCMAKE_BUILD_TYPE=Debug

# build shards (configures first if needed)
build: configure
  cmake --build build/Debug --target shards

[no-cd]
cargo-check:
  RUSTUP_TOOLCHAIN={{ rust_toolchain }} cargo check

tests:
  build/Debug/shards shards/tests/general.shs
  build/Debug/shards shards/tests/hello.shs

format:
  sh format.sh