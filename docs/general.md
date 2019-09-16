## Chains flow

`enum CBRunChainOutputState { Running, Restarted, Stopped, Failed };`

#### `Running`:
The chain executed normally and finished one cycle normally.

#### `Restarted`:
The chain executed normally until a block requested a chain restart, this might be normal, depending on chain behavior, anyway it's not considered fatal or a error by the system.
* If this was a `Inline` sub chain (`Do, Dispatch`): 
  * The root chain will restart as well.

#### `Stopped`:

#### `Failed`:



