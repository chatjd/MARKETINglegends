* vidulum.conf: contains configuration settings for vidulumd
* vidulumd.pid: stores the process id of vidulumd while running
* blocks/blk000??.dat: block data (custom, 128 MiB per file)
* blocks/rev000??.dat; block undo data (custom)
* blocks/index/*; block index (LevelDB)
* chainstate/*; block chain state da