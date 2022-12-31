# winterfs
Linux filesystem kernel module

Features:
-
- Supports up to 16TB volume size
- Supports up to 4TB file size
- 64-bit timestamps
- Fully utilizes kernel page cache & other memory management systems
- Designed for use with SSDs, no journaling or other features that reduce disk life/attempt to achieve performance gains that only make sense for HDDs
- Implemented as a kernel module, no FUSE overhead
- mkfs program for formatting volume included

Planned
-
- Greater redundancy & protection against data corruption
- Metadata encryption

Limitations
-
- Fixed block size & other parameters to make testing & implementation simpler

Reflection/commentary
-
- While there's a lot of documentation out there, much of it is fairly old and doesn't reflect kernel changes over the years, or is incomplete in some subtle way. Once I'm finished with this project I will write a short summary here of what I learned along the way so that others might be able to learn something from it without having to do as much digging themselves.
