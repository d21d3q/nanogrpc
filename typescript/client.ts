import { RPCImpl } from 'protobufjs';
import { createRPCRequest, parseRPCResponse } from './rpc/rpc';
import { PacketType } from './generated/grpc';
import { RPCError } from './rpc/errors';

export interface RPCTransportAdapter {
  write: (data: Uint8Array) => Promise<Uint8Array>;
}

export const createRPCImplementation = (transportAdapter: RPCTransportAdapter, channelId?: number) => {
  let currentCallId = 0;

  const implementation: RPCImpl = async (method, reqData, callback) => {
    currentCallId++;
    const rpcRequestBuffer = createRPCRequest({
      methodName: method.name,
      rpcRequest: reqData,
      callId: currentCallId,
    });
    console.log('Created RPC request buffer', rpcRequestBuffer);
    const resultBuffer = await transportAdapter.write(rpcRequestBuffer);
    const parsedResult = parseRPCResponse(resultBuffer);
    currentCallId--;

    if (parsedResult.type == PacketType.SERVER_ERROR) {
      const rpcError = new RPCError(parsedResult.status);
      callback(rpcError, null);
      return;
    }

    callback(null, parsedResult.payload);
  };

  return implementation;
}