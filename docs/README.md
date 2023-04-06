# Usage

## Plugin dependencies

* [MkDocs Awesome Pages Plugin}](https://github.com/lukasgeiter/mkdocs-awesome-pages-plugin)

## Serve - development

1. have docker
2. `docker run --rm -it -p 8000:8000 -v ${PWD}:/docs squidfunk/mkdocs-material`

## Build notes

### Build dependencies

```bash
pip install mkdocs-material
pip install mkdocs-awesome-pages-plugin
pip install mkdocs-macros-plugin
```

### Generate the docs

```bash
shards generate.edn
mkdocs build --site-dir public
```
Generated docs will be found in the [public](public) directory.

## Create more documentation

Search for `SHOptionalString` to find examples in the code on how to describe a shard.

You'll need to build a `shards` debug release after adding description information to a shard, and to generate output from running a shard. See [docs/contribute/code/build-shards.md](docs/contribute/code/build-shards.md) to find more details on building `shards` debug release.

### Describe a shard

Example describing a shard:
```c++
static SHOptionalString help() { return SHCCSTR("Computes the absolute value of a big integer."); }
```

Example describing inputs and outputs of a shard:
```c++
static SHOptionalString inputHelp() { return SHCCSTR("The input can be of any type."); }

static SHOptionalString outputHelp() { return SHCCSTR(R"(The output can be of any type.")"); }
```

Recompile the code after you're done.

### Add shard samples

Create `.edn` files, each with a Shards code sample inside.

Follow how other samples are formated in the [samples/shards](samples/shards) directory to create new code samples. Also look into [../src/tests](../src/tests) to see if there are examples that could help you start.

### Debug shard sample

Debug the shard sample using the commands as in the example below before versioning your changes.

Example on how to run a shard to produce debug logs:
```bash
export SHARD_PATH=`pwd`/docs/samples/shards/FS/Join/Join

touch ${SHARD_PATH}.log

build/Debug/shards docs/samples/run-sample.edn --looped false --file ${SHARD_PATH}.edn  > >(tee "${SHARD_PATH}.log");
```

If the sample code throws an error, you should see an error message:
```
Error: Main tick failed
```
