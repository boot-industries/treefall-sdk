# Attaching ARA2 Processors to Media Items

Extensions can manage ARA2 processors on media items using two callbacks:

- `ARA_AttachProcessorToMediaItem(MediaItem* item, const char* processorId, const char* stateData)` associates an ARA2 processor identified by `processorId` with `item`. The optional `stateData` parameter lets extensions pass serialized processor state to the host.
- `ARA_DetachProcessorFromMediaItem(MediaItem* item)` removes any previously attached ARA2 processor from `item`.

These callbacks allow extensions to directly attach or detach ARA2 processing contexts to individual media items.
