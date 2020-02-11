// <auto-generated>
//  automatically generated by the FlatBuffers compiler, do not modify
// </auto-generated>

namespace ProtocolCS.Login
{

using global::System;
using global::FlatBuffers;

public struct Reply_LoginFailed : IFlatbufferObject
{
  private Table __p;
  public ByteBuffer ByteBuffer { get { return __p.bb; } }
  public static Reply_LoginFailed GetRootAsReply_LoginFailed(ByteBuffer _bb) { return GetRootAsReply_LoginFailed(_bb, new Reply_LoginFailed()); }
  public static Reply_LoginFailed GetRootAsReply_LoginFailed(ByteBuffer _bb, Reply_LoginFailed obj) { return (obj.__assign(_bb.GetInt(_bb.Position) + _bb.Position, _bb)); }
  public void __init(int _i, ByteBuffer _bb) { __p.bb_pos = _i; __p.bb = _bb; }
  public Reply_LoginFailed __assign(int _i, ByteBuffer _bb) { __init(_i, _bb); return this; }

  public ProtocolCS.ErrorCode ErrorCode { get { int o = __p.__offset(4); return o != 0 ? (ProtocolCS.ErrorCode)__p.bb.GetInt(o + __p.bb_pos) : ProtocolCS.ErrorCode.OK; } }
  public bool MutateErrorCode(ProtocolCS.ErrorCode error_code) { int o = __p.__offset(4); if (o != 0) { __p.bb.PutInt(o + __p.bb_pos, (int)error_code); return true; } else { return false; } }

  public static Offset<Reply_LoginFailed> CreateReply_LoginFailed(FlatBufferBuilder builder,
      ProtocolCS.ErrorCode error_code = ProtocolCS.ErrorCode.OK) {
    builder.StartObject(1);
    Reply_LoginFailed.AddErrorCode(builder, error_code);
    return Reply_LoginFailed.EndReply_LoginFailed(builder);
  }

  public static void StartReply_LoginFailed(FlatBufferBuilder builder) { builder.StartObject(1); }
  public static void AddErrorCode(FlatBufferBuilder builder, ProtocolCS.ErrorCode errorCode) { builder.AddInt(0, (int)errorCode, 0); }
  public static Offset<Reply_LoginFailed> EndReply_LoginFailed(FlatBufferBuilder builder) {
    int o = builder.EndObject();
    return new Offset<Reply_LoginFailed>(o);
  }
};


}