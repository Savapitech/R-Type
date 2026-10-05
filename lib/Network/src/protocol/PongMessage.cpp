#include "PongMessage.hpp"

bool PongMessage::serialize(PacketWriter &writer) const
{
    return writer.writeUInt32(timestamp);
}

bool PongMessage::deserialize(PacketReader &reader)
{
    return reader.readUInt32(timestamp);
}
