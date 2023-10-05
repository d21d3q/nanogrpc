import { GrpcStatus, PacketType } from "../generated/nanogrpc";


export class RPCError extends Error {
  grpcStatus: GrpcStatus;
  grpcPacketType: PacketType;

  constructor(grpcPacketType: PacketType, grpcStatus: GrpcStatus) {
    const packetTypeString = Object.entries(PacketType).find(([key, value]) => value == grpcPacketType)?.[0];
    const statusString = Object.entries(GrpcStatus).find(([key, value]) => value == grpcStatus)?.[0];

    super(`RPCError: ${packetTypeString} ${statusString}`);

    this.grpcPacketType = grpcPacketType;
    this.grpcStatus = grpcStatus;
  }
}