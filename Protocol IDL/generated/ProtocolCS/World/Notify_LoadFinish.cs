// <auto-generated>
//  automatically generated by the FlatBuffers compiler, do not modify
// </auto-generated>

namespace ProtocolCS.World
{

using global::System;
using global::FlatBuffers;

public struct Notify_LoadFinish : IFlatbufferObject
{
  private Table __p;
  public ByteBuffer ByteBuffer { get { return __p.bb; } }
  public static Notify_LoadFinish GetRootAsNotify_LoadFinish(ByteBuffer _bb) { return GetRootAsNotify_LoadFinish(_bb, new Notify_LoadFinish()); }
  public static Notify_LoadFinish GetRootAsNotify_LoadFinish(ByteBuffer _bb, Notify_LoadFinish obj) { return (obj.__assign(_bb.GetInt(_bb.Position) + _bb.Position, _bb)); }
  public void __init(int _i, ByteBuffer _bb) { __p.bb_pos = _i; __p.bb = _bb; }
  public Notify_LoadFinish __assign(int _i, ByteBuffer _bb) { __init(_i, _bb); return this; }


  public static void StartNotify_LoadFinish(FlatBufferBuilder builder) { builder.StartObject(0); }
  public static Offset<Notify_LoadFinish> EndNotify_LoadFinish(FlatBufferBuilder builder) {
    int o = builder.EndObject();
    return new Offset<Notify_LoadFinish>(o);
  }
};


}
