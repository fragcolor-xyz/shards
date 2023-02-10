- [ ]  clean up existing panel code
  - [ ] remove unused variables/fields
  - [ ] make sure the code is as much as possible shard-agnostic
  - [ ] i.e. use the abstract Panel type everywhere
  - [ ] have the Spatial.Panel be a special case which goal is just to set a transform, host a egui context and pass it to its children shards
	- [ ] unsure why it can't be done in its own activate function?

- [ ] once this is done
- [ ] check with interop are needed to have the same mechanism for a shard implemented on rust side.