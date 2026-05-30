-- HyperDisk X Block IO Protocol Dissector for WireShark
-- Frame Header (40B): Magic(4B) + Version(2B) + Opcode(1B) + Flags(1B) + RequestID(4B) + PayloadLen(4B) + ImageID(8B) + BlockOffset(8B) + BlockCount(4B) + LayerID(1B) + Reserved(3B)

local hdxb_proto = Proto("hdxb", "HyperDisk X Block IO Protocol")

local f_magic        = ProtoField.uint32("hdxb.magic",        "Magic",        base.HEX)
local f_version      = ProtoField.uint16("hdxb.version",      "Version",      base.DEC)
local f_opcode       = ProtoField.uint8("hdxb.opcode",        "Opcode",       base.HEX)
local f_flags        = ProtoField.uint8("hdxb.flags",         "Flags",        base.HEX)
local f_request_id   = ProtoField.uint32("hdxb.request_id",   "RequestID",    base.DEC)
local f_payload_len  = ProtoField.uint32("hdxb.payload_len",  "PayloadLen",   base.DEC)
local f_image_id     = ProtoField.uint64("hdxb.image_id",     "ImageID",      base.HEX)
local f_block_offset = ProtoField.uint64("hdxb.block_offset", "BlockOffset",  base.DEC)
local f_block_count  = ProtoField.uint32("hdxb.block_count",  "BlockCount",   base.DEC)
local f_layer_id     = ProtoField.uint8("hdxb.layer_id",      "LayerID",      base.DEC)
local f_reserved     = ProtoField.uint24("hdxb.reserved",     "Reserved",     base.HEX)

hdxb_proto.fields = {
    f_magic, f_version, f_opcode, f_flags, f_request_id, f_payload_len,
    f_image_id, f_block_offset, f_block_count, f_layer_id, f_reserved
}

local OPCODES = {
    [0x01] = "BLOCK_READ_REQ",
    [0x02] = "BLOCK_READ_RSP",
    [0x03] = "BLOCK_WRITE_REQ",
    [0x04] = "BLOCK_WRITE_RSP",
    [0x10] = "HEARTBEAT",
    [0x11] = "HEARTBEAT_ACK",
    [0x20] = "CACHE_INVALIDATE",
    [0x30] = "PREFETCH_PUSH",
    [0x40] = "VECTOR_READ_REQ",
    [0x41] = "VECTOR_READ_RSP",
    [0x42] = "VECTOR_WRITE_REQ",
    [0x43] = "VECTOR_WRITE_RSP",
    [0xFF] = "ERROR_RSP",
}

local LAYERS = {
    [0] = "OS",
    [1] = "DRIVER",
    [2] = "APP",
    [3] = "OVERLAY",
}

local FLAG_NAMES = {
    [0] = "NONE",
    [1] = "COMPRESSED",
    [2] = "URGENT",
    [4] = "CACHED",
}

function hdxb_proto.dissector(buffer, pinfo, tree)
    local buf_len = buffer:len()
    if buf_len < 40 then return false end

    local magic = buffer(0, 4):uint()
    if magic ~= 0x48444B47 then return false end

    pinfo.cols.protocol = "HDXB"

    local subtree = tree:add(hdxb_proto, buffer(), "HyperDisk X Block IO")

    subtree:add(f_magic,        buffer(0, 4))
    subtree:add(f_version,      buffer(4, 2))

    local opcode = buffer(6, 1):uint()
    local opcode_name = OPCODES[opcode] or "UNKNOWN"
    subtree:add(f_opcode, buffer(6, 1)):append_text(" (" .. opcode_name .. ")")

    local flags = buffer(7, 1):uint()
    local flag_str = ""
    if flags == 0 then flag_str = "NONE"
    else
        local parts = {}
        if bit.band(flags, 0x01) ~= 0 then parts[#parts+1] = "COMPRESSED" end
        if bit.band(flags, 0x02) ~= 0 then parts[#parts+1] = "URGENT" end
        if bit.band(flags, 0x04) ~= 0 then parts[#parts+1] = "CACHED" end
        flag_str = table.concat(parts, "|")
    end
    subtree:add(f_flags, buffer(7, 1)):append_text(" (" .. flag_str .. ")")

    subtree:add(f_request_id,   buffer(8, 4))
    subtree:add(f_payload_len,  buffer(12, 4))
    subtree:add(f_image_id,     buffer(16, 8))
    subtree:add(f_block_offset, buffer(24, 8))
    subtree:add(f_block_count,  buffer(32, 4))

    local layer_id = buffer(36, 1):uint()
    local layer_name = LAYERS[layer_id] or "UNKNOWN"
    subtree:add(f_layer_id, buffer(36, 1)):append_text(" (" .. layer_name .. ")")

    subtree:add(f_reserved, buffer(37, 3))

    local payload_len = buffer(12, 4):uint()
    if buf_len > 40 and payload_len > 0 then
        local actual_payload = math.min(payload_len, buf_len - 40)
        subtree:add(buffer(40, actual_payload), "Payload (" .. actual_payload .. " bytes)")
    end

    pinfo.cols.info = opcode_name .. " ReqID=" .. buffer(8, 4):uint() .. " Image=0x" .. string.format("%016X", buffer(16, 8):uint())

    return true
end

local tcp_table = DissectorTable.get("tcp.port")
tcp_table:add(9527, hdxb_proto)
