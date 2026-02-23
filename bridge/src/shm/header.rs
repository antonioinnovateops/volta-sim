use super::layout::*;

/// Read-only view of the SHM header.
pub struct ShmHeader<'a> {
    data: &'a [u8],
}

impl<'a> ShmHeader<'a> {
    pub fn new(data: &'a [u8]) -> Self {
        Self { data }
    }

    pub fn magic(&self) -> u32 {
        u32::from_le_bytes(self.data[MAGIC_OFFSET..MAGIC_OFFSET + 4].try_into().unwrap())
    }

    pub fn version(&self) -> u32 {
        u32::from_le_bytes(
            self.data[VERSION_OFFSET..VERSION_OFFSET + 4]
                .try_into()
                .unwrap(),
        )
    }

    pub fn sequence(&self) -> u64 {
        u64::from_le_bytes(
            self.data[SEQUENCE_OFFSET..SEQUENCE_OFFSET + 8]
                .try_into()
                .unwrap(),
        )
    }

    pub fn is_initialized(&self) -> bool {
        let flags = u32::from_le_bytes(
            self.data[FLAGS_OFFSET..FLAGS_OFFSET + 4]
                .try_into()
                .unwrap(),
        );
        flags & 1 != 0
    }

    pub fn is_valid(&self) -> bool {
        self.magic() == MAGIC && self.version() == VERSION && self.is_initialized()
    }
}
