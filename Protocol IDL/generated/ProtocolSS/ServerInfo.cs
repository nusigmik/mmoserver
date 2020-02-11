// <auto-generated>
//  automatically generated by the FlatBuffers compiler, do not modify
// </auto-generated>

namespace ProtocolSS
{

using global::System;
using global::FlatBuffers;

public struct ServerInfo : IFlatbufferObject
{
  private Table __p;
  public ByteBuffer ByteBuffer { get { return __p.bb; } }
  public static ServerInfo GetRootAsServerInfo(ByteBuffer _bb) { return GetRootAsServerInfo(_bb, new ServerInfo()); }
  public static ServerInfo GetRootAsServerInfo(ByteBuffer _bb, ServerInfo obj) { return (obj.__assign(_bb.GetInt(_bb.Position) + _bb.Position, _bb)); }
  public void __init(int _i, ByteBuffer _bb) { __p.bb_pos = _i; __p.bb = _bb; }
  public ServerInfo __assign(int _i, ByteBuffer _bb) { __init(_i, _bb); return this; }

  public int SessionId { get { int o = __p.__offset(4); return o != 0 ? __p.bb.GetInt(o + __p.bb_pos) : (int)0; } }
  public bool MutateSessionId(int session_id) { int o = __p.__offset(4); if (o != 0) { __p.bb.PutInt(o + __p.bb_pos, session_id); return true; } else { return false; } }
  public string Name { get { int o = __p.__offset(6); return o != 0 ? __p.__string(o + __p.bb_pos) : null; } }
  public ArraySegment<byte>? GetNameBytes() { return __p.__vector_as_arraysegment(6); }
  public int Type { get { int o = __p.__offset(8); return o != 0 ? __p.bb.GetInt(o + __p.bb_pos) : (int)0; } }
  public bool MutateType(int type) { int o = __p.__offset(8); if (o != 0) { __p.bb.PutInt(o + __p.bb_pos, type); return true; } else { return false; } }

  public static Offset<ServerInfo> CreateServerInfo(FlatBufferBuilder builder,
      int session_id = 0,
      StringOffset nameOffset = default(StringOffset),
      int type = 0) {
    builder.StartObject(3);
    ServerInfo.AddType(builder, type);
    ServerInfo.AddName(builder, nameOffset);
    ServerInfo.AddSessionId(builder, session_id);
    return ServerInfo.EndServerInfo(builder);
  }

  public static void StartServerInfo(FlatBufferBuilder builder) { builder.StartObject(3); }
  public static void AddSessionId(FlatBufferBuilder builder, int sessionId) { builder.AddInt(0, sessionId, 0); }
  public static void AddName(FlatBufferBuilder builder, StringOffset nameOffset) { builder.AddOffset(1, nameOffset.Value, 0); }
  public static void AddType(FlatBufferBuilder builder, int type) { builder.AddInt(2, type, 0); }
  public static Offset<ServerInfo> EndServerInfo(FlatBufferBuilder builder) {
    int o = builder.EndObject();
    return new Offset<ServerInfo>(o);
  }
};


}