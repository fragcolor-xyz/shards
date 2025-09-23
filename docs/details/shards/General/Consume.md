All values producers/consumers for the same channel must have the same Type.

Consume will wait until a value is available (and it removes values from the channel queue). If you want every listener to get a copy, use `Broadcast/Listen` instead of `Produce/Consume`.