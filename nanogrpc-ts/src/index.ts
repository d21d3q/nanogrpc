
import * as protobufjs from "protobufjs"
import { RPCImpl } from 'protobufjs';
import { GrpcStatus, PacketType, RpcPacketRequest, RpcPacketResponse } from './generated/nanogrpc';
import { RPCError } from './rpc/errors';
import { crc32 } from "./crc32";

export interface RPCTransportAdapter {
  /**
   * Send an RPC request and return the response.
   * If the transport is used for multiple services, it is the responsibility of the transport adapter
   * to implement transactions and atomicity.
   * 
   * f.e. to deal with multiple simultaneous requests, a queuing mutex can be used.
   * https://github.com/DirtyHairy/async-mutex
   * 
   * @param request Request
   * @returns Response
   */
  send: (request: RpcPacketRequest) => Promise<RpcPacketResponse>;
}

type Constructor<K> = { new(rpcImpl: RPCImpl, requestDelimited?: boolean, responseDelimited?: boolean): K };

export interface RPCServiceFactory {
  /**
   * Generate a protobuf RPC implementation for a given service.
   * @param service 
   */
  create<T extends protobufjs.rpc.Service>(service: Constructor<T>): T
}

/**
 * This function takes a transport adapter and returns a factory for protobuf RPC services.
 * 
 * The same transport will be reused for all services.
 * The transport adapter is responsible for implementing synchronisation of the RPC calls.
 * 
 * @param transportAdapter 
 * @param service 
 * @returns 
 */
export const createRPCServiceFactory = (transportAdapter: RPCTransportAdapter) => {
  let currentCallId = 0;

  return {
    create: <T extends protobufjs.rpc.Service>(service: Constructor<T>): T => {
      return new service(async (method, reqData, callback) => {
        console.log(service.name, method.name)
        const request = RpcPacketRequest.create({
          callId: currentCallId++,
          methodId: crc32(Buffer.from(method.name, "ascii")),
          payload: reqData,
          serviceId: crc32(Buffer.from(service.name, "ascii")),
        });

        const response = await transportAdapter.send(request);

        if (response.status !== GrpcStatus.OK) {
          const rpcError = new RPCError(response.type, response.status);
          callback(rpcError, null);
          return;
        }
        
        callback(null, response.payload);
      })
    }
  }
}

